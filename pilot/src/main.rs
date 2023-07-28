use clap::Parser;
use serde_json::Value;
use std::collections::{HashMap, VecDeque};
use std::fs::File;
use std::process::Stdio;
use tokio::io::AsyncWriteExt;
use tokio::process::Command;
use tokio::sync::Mutex;
use tower_lsp::jsonrpc;
use tower_lsp::lsp_types::*;
use tower_lsp::{Client, LanguageServer, LspService, Server};

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

#[derive(Parser, Debug)]
struct Args {
    #[arg(long)]
    cobra_path: String,
}

#[derive(Debug)]
struct Backend {
    client: Client,
    args: Args,
    semantic_tokens: Mutex<HashMap<Url, Vec<SemanticToken>>>,
    inlay_hints: Mutex<HashMap<Url, Vec<InlayHint>>>,
}

fn get_position(value: &Value) -> Result<Position> {
    let line = value.get("line").and_then(Value::as_u64).ok_or("bad line")?;
    let col = value.get("col").and_then(Value::as_u64).ok_or("bad column")?;
    Ok(Position::new(line.try_into()?, col.try_into()?))
}

fn get_range(value: &Value) -> Result<Range> {
    let start = get_position(value.get("start").ok_or("missing start")?)?;
    let end = get_position(value.get("end").ok_or("missing end")?)?;
    Ok(Range::new(start, end))
}

fn get_message(value: &Value) -> Result<String> {
    let message = value
        .get("message")
        .and_then(Value::as_str)
        .ok_or("bad message")?;
    let primary_label = value
        .get("primary_label")
        .and_then(Value::as_str)
        .ok_or("bad primary label")?;
    let secondary_label = value
        .get("secondary_label")
        .and_then(Value::as_str)
        .ok_or("bad secondary label")?;

    if !primary_label.is_empty() && !secondary_label.is_empty() {
        Ok(format!(
            "{} ({}) ({})",
            message, primary_label, secondary_label
        ))
    } else if !primary_label.is_empty() {
        Ok(format!("{} ({})", message, primary_label))
    } else if !secondary_label.is_empty() {
        Ok(format!("{} ({})", message, secondary_label))
    } else {
        Ok(format!("{}", message))
    }
}

impl Backend {
    async fn checked_update(&self, uri: Url, text: Option<String>) -> Result<()> {
        let mut semantic_tokens = self.semantic_tokens.lock().await;
        let mut inlay_hints = self.inlay_hints.lock().await;

        let output = if let Some(text) = text {
            let mut child = Command::new(&self.args.cobra_path)
                .stdin(Stdio::piped())
                .stdout(Stdio::piped())
                .spawn()?;

            let mut stdin = child.stdin.take().unwrap();

            tokio::spawn(async move {
                if let Err(err) = stdin.write_all(text.as_bytes()).await {
                    eprintln!("{}", err);
                }
            });

            child.wait_with_output().await?
        } else {
            if uri.scheme() != "file" {
                Err("bad scheme")?;
            }

            let path = uri.to_file_path().map_err(|_| "bad uri")?;

            Command::new(&self.args.cobra_path)
                .stdin(File::open(path)?)
                .output()
                .await?
        };

        let output: Value = serde_json::from_slice(&output.stdout)?;
        let mut queue: VecDeque<_> = output
            .get("diags")
            .and_then(Value::as_array)
            .ok_or("bad diags")?
            .clone()
            .into();
        let mut diags = Vec::new();
        let mut tokens = Vec::new();
        let mut hints = Vec::new();
        let mut last_pos = Position::new(0, 0);

        while let Some(object) = queue.pop_front() {
            let lvl_str = object.get("lvl").and_then(Value::as_str).ok_or("bad lvl")?;
            let mut related_information = Vec::new();

            for object in object
                .get("sub_diags")
                .and_then(Value::as_array)
                .ok_or("bad array")?
            {
                related_information.push(DiagnosticRelatedInformation {
                    location: Location {
                        uri: uri.clone(),
                        range: get_range(object)?,
                    },
                    message: get_message(object)?,
                });

                queue.push_back(object.clone());
            }

            let severity = match lvl_str {
                "warning" => DiagnosticSeverity::WARNING,
                "note" => DiagnosticSeverity::INFORMATION,
                _ => DiagnosticSeverity::ERROR,
            };

            diags.push(Diagnostic {
                range: get_range(&object)?,
                severity: Some(severity),
                message: get_message(&object)?,
                related_information: Some(related_information),
                ..Default::default()
            });
        }

        for object in output
            .get("tokens")
            .and_then(Value::as_array)
            .ok_or("bad tokens")?
        {
            let range = get_range(&object)?;
            let token_type = object
                .get("type")
                .and_then(Value::as_str)
                .ok_or("bad type")?;

            tokens.push(SemanticToken {
                delta_line: range.start.line - last_pos.line,
                delta_start: if range.start.line == last_pos.line {
                    range.start.character - last_pos.character
                } else {
                    range.start.character
                },
                length: range.end.character - range.start.character,
                token_type: match token_type {
                    "keyword" => 0,
                    "string" => 1,
                    "number" => 2,
                    "filter" => 3,
                    _ => continue,
                },
                token_modifiers_bitset: 0,
            });

            last_pos = range.start;
        }

        for object in output.get("inlay_hints").and_then(Value::as_array).ok_or("bad inlay hints")? {
            let hint = object.get("hint").and_then(Value::as_str).ok_or("bad hint")?;

            hints.push(InlayHint {
                position: get_position(&object)?,
                label: InlayHintLabel::String(format!("{}:", hint)),
                kind: Some(InlayHintKind::PARAMETER),
                text_edits: None,
                tooltip: None,
                padding_left: Some(false),
                padding_right: Some(true),
                data: None,
            });
        }

        semantic_tokens.insert(uri.clone(), tokens);
        inlay_hints.insert(uri.clone(), hints);

        self.client.publish_diagnostics(uri, diags, None).await;

        Ok(())
    }

    async fn update(&self, uri: Url, text: Option<String>) {
        if let Err(err) = self.checked_update(uri, text).await {
            eprintln!("{}", err);
        }
    }
}

#[tower_lsp::async_trait]
impl LanguageServer for Backend {
    async fn initialize(&self, _: InitializeParams) -> jsonrpc::Result<InitializeResult> {
        Ok(InitializeResult {
            capabilities: ServerCapabilities {
                text_document_sync: Some(TextDocumentSyncCapability::Options(
                    TextDocumentSyncOptions {
                        save: Some(TextDocumentSyncSaveOptions::Supported(true)),
                        open_close: Some(true),
                        change: Some(TextDocumentSyncKind::FULL),
                        ..Default::default()
                    },
                )),
                semantic_tokens_provider: Some(
                    SemanticTokensServerCapabilities::SemanticTokensOptions(
                        SemanticTokensOptions {
                            legend: SemanticTokensLegend {
                                token_types: vec![
                                    SemanticTokenType::KEYWORD,
                                    SemanticTokenType::STRING,
                                    SemanticTokenType::NUMBER,
                                    SemanticTokenType::VARIABLE,
                                ],
                                token_modifiers: vec![],
                            },
                            full: Some(SemanticTokensFullOptions::Delta { delta: Some(false) }),
                            range: Some(false),
                            ..Default::default()
                        },
                    ),
                ),
                inlay_hint_provider: Some(OneOf::Right(InlayHintServerCapabilities::Options(
                    InlayHintOptions {
                        resolve_provider: Some(false),
                        ..Default::default()
                    },
                ))),
                ..Default::default()
            },
            ..Default::default()
        })
    }

    async fn shutdown(&self) -> jsonrpc::Result<()> {
        Ok(())
    }

    async fn did_open(&self, params: DidOpenTextDocumentParams) {
        self.update(params.text_document.uri, None).await;
    }

    async fn did_save(&self, params: DidSaveTextDocumentParams) {
        self.update(params.text_document.uri, None).await;
    }

    async fn did_change(&self, params: DidChangeTextDocumentParams) {
        if let Some(change) = params.content_changes.first() {
            self.update(params.text_document.uri, Some(change.text.clone()))
                .await;
        }
    }

    async fn semantic_tokens_full(
        &self,
        params: SemanticTokensParams,
    ) -> jsonrpc::Result<Option<SemanticTokensResult>> {
        tokio::time::sleep(std::time::Duration::from_secs_f32(0.1)).await;

        let semantic_tokens = self.semantic_tokens.lock().await;

        Ok(semantic_tokens
            .get(&params.text_document.uri)
            .map(|tokens| {
                SemanticTokensResult::Tokens(SemanticTokens {
                    data: tokens.clone(),
                    ..Default::default()
                })
            }))
    }

    async fn inlay_hint(&self, params: InlayHintParams) -> jsonrpc::Result<Option<Vec<InlayHint>>> {
        tokio::time::sleep(std::time::Duration::from_secs_f32(0.1)).await;

        let inlay_hints = self.inlay_hints.lock().await;

        Ok(inlay_hints.get(&params.text_document.uri).cloned())
    }
}

#[tokio::main]
async fn main() {
    let stdin = tokio::io::stdin();
    let stdout = tokio::io::stdout();
    let args = Args::parse();

    let (service, socket) = LspService::new(|client| Backend {
        client,
        args,
        semantic_tokens: Mutex::new(HashMap::new()),
        inlay_hints: Mutex::new(HashMap::new()),
    });

    Server::new(stdin, stdout, socket).serve(service).await;
}
