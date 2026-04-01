"use strict";

const path = require("path");
const fs = require("fs");
const vscode = require("vscode");
const lc = require("vscode-languageclient/node");

let client = null;

function candidateServerPaths(context) {
  const out = [];
  const cfgPath = vscode.workspace.getConfiguration("kern").get("languageServer.path", "");
  if (cfgPath && cfgPath.trim()) out.push(cfgPath.trim());

  const exe = process.platform === "win32" ? "kern_lsp.exe" : "kern_lsp";
  if (vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders.length > 0) {
    const ws = vscode.workspace.workspaceFolders[0].uri.fsPath;
    out.push(path.join(ws, "build", "Release", exe));
    out.push(path.join(ws, "build", exe));
    out.push(path.join(ws, "shareable-ide", "compiler", exe));
  }
  out.push(exe);
  return out;
}

function resolveServerPath(context) {
  const candidates = candidateServerPaths(context);
  for (const p of candidates) {
    if (!p) continue;
    if (p === "kern_lsp" || p === "kern_lsp.exe") return p;
    if (fs.existsSync(p)) return p;
  }
  return candidates[candidates.length - 1];
}

function activate(context) {
  const command = resolveServerPath(context);
  const serverOptions = {
    command,
    transport: lc.TransportKind.stdio
  };

  const clientOptions = {
    documentSelector: [{ scheme: "file", language: "kern" }]
  };

  client = new lc.LanguageClient(
    "kern-lsp",
    "Kern Language Server",
    serverOptions,
    clientOptions
  );

  context.subscriptions.push(client.start());
}

function deactivate() {
  if (!client) return undefined;
  return client.stop();
}

module.exports = {
  activate,
  deactivate
};
