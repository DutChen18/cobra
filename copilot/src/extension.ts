// The module 'vscode' contains the VS Code extensibility API
// Import the module and reference it with the alias vscode in your code below
import * as vscode from 'vscode';

import { LanguageClient, ServerOptions, LanguageClientOptions, TransportKind } from 'vscode-languageclient/node';

let client: LanguageClient;

// This method is called when your extension is activated
// Your extension is activated the very first time the command is executed
export function activate(context: vscode.ExtensionContext) {
	let outputChannel = vscode.window.createOutputChannel("cobra", "cobra");

	let serverOptions: ServerOptions = {
		command: '/home/chen/mnt/home/codam/cobra/pilot/target/debug/pilot',
		args: ['--cobra-path', '/home/chen/mnt/home/codam/cobra/webserv'],
	};

	let clientOptions: LanguageClientOptions = {
		documentSelector: [{ scheme: 'file', language: 'cobra' }],
		outputChannel,
		traceOutputChannel: outputChannel,
	};

	client = new LanguageClient(
		'cobra',
		'cobra',
		serverOptions,
		clientOptions,
	);

	client.start();
}

// This method is called when your extension is deactivated
export function deactivate(): Thenable<void> | undefined {
	if (!client) {
		return undefined;
	} else {
		return client.stop();
	}
}
