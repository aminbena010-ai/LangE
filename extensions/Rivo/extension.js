'use strict';

/*
 * Rivo — extensión unificada para VSCode.
 * gramática + language-configuration + snippets + LSP + compilar/ejecutar.
 */

const vscode = require('vscode');
const path = require('path');
const { spawn } = require('child_process');
const { LanguageClient } = require('./client');
const runner = require('./runner');

let client = null;
let statusItem = null;

function initializationOptions() {
  const cfg = vscode.workspace.getConfiguration('rivo');
  return {
    compilerPath: cfg.get('compilerPath', 'rivo'),
    diagnostics: cfg.get('diagnostics', true),
    semanticTokens: cfg.get('semanticTokens', true)
  };
}

/* Barra de estado: versión del compilador; clic → reiniciar servidor. */
function refreshStatus() {
  if (!statusItem) return;
  const cfg = vscode.workspace.getConfiguration('rivo');
  const compiler = cfg.get('compilerPath', 'rivo');
  statusItem.text = '$(loading~spin) Rivo';
  statusItem.tooltip = 'Rivo: comprobando compilador…';
  let child;
  try {
    child = spawn(compiler, ['--version'], { stdio: ['ignore', 'pipe', 'ignore'] });
  } catch (e) {
    statusItem.text = '$(circle-slash) Rivo';
    statusItem.tooltip = 'Compilador no encontrado. Pulsa para ajustar rivo.compilerPath.';
    return;
  }
  let out = '';
  child.stdout.on('data', (d) => { out += d.toString('utf8'); });
  child.on('error', () => {
    statusItem.text = '$(circle-slash) Rivo';
    statusItem.tooltip = 'Compilador no encontrado. Pulsa para ajustar rivo.compilerPath.';
    statusItem.command = 'rivo.openSettings';
  });
  child.on('close', () => {
    const m = /(\d+\.\d+\.\d+(?:\+\S+)?)/.exec(out);
    if (m) {
      statusItem.text = '$(symbol-method) Rivo ' + m[1];
      statusItem.tooltip = 'Compilador: ' + compiler + '\nPulsa para reiniciar el servidor de lenguaje.';
      statusItem.command = 'rivo.restartServer';
    } else {
      statusItem.text = '$(circle-slash) Rivo';
      statusItem.tooltip = 'Compilador no encontrado. Pulsa para ajustar rivo.compilerPath.';
      statusItem.command = 'rivo.openSettings';
    }
  });
}

function activate(context) {
  const serverModule = context.asAbsolutePath(path.join('server', 'server.js'));
  client = new LanguageClient(serverModule, initializationOptions);
  client.start();

  statusItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
  statusItem.name = 'Rivo';
  statusItem.text = 'Rivo';
  statusItem.show();
  refreshStatus();

  context.subscriptions.push(client, statusItem);
  context.subscriptions.push(
    vscode.commands.registerCommand('rivo.run', () => runner.runFile()),
    vscode.commands.registerCommand('rivo.compile', () => runner.compileOnly()),
    vscode.commands.registerCommand('rivo.format', () => runner.formatFile()),
    vscode.commands.registerCommand('rivo.restartServer', () => {
      if (client) client.restart();
      refreshStatus();
      vscode.window.setStatusBarMessage('Rivo: servidor de lenguaje reiniciado', 3000);
    }),
    vscode.commands.registerCommand('rivo.openSettings', () => {
      vscode.commands.executeCommand('workbench.action.openSettings', 'rivo.');
    }),
    vscode.workspace.onDidChangeConfiguration((e) => {
      if (e.affectsConfiguration('rivo') && client) {
        client.restart();
        refreshStatus();
      }
    }),
    { dispose: () => runner.dispose() }
  );
}

function deactivate() {
  runner.dispose();
  if (client) return client.stop();
  return undefined;
}

module.exports = { activate, deactivate };
