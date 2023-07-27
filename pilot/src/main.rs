use serde_json::Value;
use std::collections::VecDeque;
use std::fs::File;
use std::process::Stdio;
use tokio::io::AsyncWriteExt;
use tokio::process::Command;
use tower_lsp::jsonrpc;
use tower_lsp::lsp_types::*;
use tower_lsp::{Client, LanguageServer, LspService, Server};

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

#[derive(Debug)]
struct Backend {
    client: Client,
}

fn get_range(value: &Value) -> Result<Range> {
    let start = value.get("start").ok_or("missing start")?;
    let end = value.get("end").ok_or("missing end")?;
    let start_line = start
        .get("line")
        .and_then(Value::as_u64)
        .ok_or("bad start line")?;
    let start_col = start
        .get("col")
        .and_then(Value::as_u64)
        .ok_or("bad start column")?;
    let end_line = end
        .get("line")
        .and_then(Value::as_u64)
        .ok_or("bad end line")?;
    let end_col = end
        .get("col")
        .and_then(Value::as_u64)
        .ok_or("bad end column")?;
    let start = Position::new(start_line.try_into()?, start_col.try_into()?);
    let end = Position::new(end_line.try_into()?, end_col.try_into()?);
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
        let output = if let Some(text) = text {
            let mut child = Command::new("/home/codam/cobra/webserv")
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

            Command::new("/home/codam/cobra/webserv")
                .stdin(File::open(path)?)
                .output()
                .await?
        };

        let output: Value = serde_json::from_slice(&output.stdout)?;
        let mut queue: VecDeque<_> = output.as_array().ok_or("bad array")?.clone().into();
        let mut diags = Vec::new();

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
                        change: Some(TextDocumentSyncKind::FULL),
                        ..Default::default()
                    },
                )),
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
}

#[tokio::main]
async fn main() {
    let stdin = tokio::io::stdin();
    let stdout = tokio::io::stdout();

    let (service, socket) = LspService::new(|client| Backend { client });
    Server::new(stdin, stdout, socket).serve(service).await;
}
