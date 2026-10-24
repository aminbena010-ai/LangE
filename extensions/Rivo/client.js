'use strict';

/*
 * Rivo — Cliente LSP mínimo (sin dependencias externas).
 *
 * Lanza server/server.js por stdio y registra en VSCode:
 *   diagnostics, completion, definition, hover, references,
 *   documentSymbol, signatureHelp y semanticTokens.
 */

const { spawn } = require('child_process');
const vscode = require('vscode');

const SEMANTIC_LEGEND = {
  tokenTypes: [
    'namespace', 'type', 'class', 'enum', 'interface', 'struct',
    'typeParameter', 'parameter', 'variable', 'property', 'enumMember',
    'event', 'method', 'macro', 'keyword', 'comment', 'string', 'number',
    'regexp', 'operator'
  ],
  tokenModifiers: ['declaration']
};

function md(v) {
  if (!v) return undefined;
  const s = new vscode.MarkdownString(v);
  s.supportThemeIcons = true;
  return s;
}

class LanguageClient {
  constructor(serverModule, getInitializationOptions) {
    this.serverModule = serverModule;
    this.getInitializationOptions = getInitializationOptions;
    this.child = null;
    this.buffer = Buffer.alloc(0);
    this.nextId = 1;
    this.pending = new Map();
    this.disposables = [];
    this.stopped = false;
    this.restarts = 0;
    this.diagnosticCollection = vscode.languages.createDiagnosticCollection('rivo');
  }

  start() {
    this.stopped = false;
    this.child = spawn(process.execPath, [this.serverModule], {
      stdio: ['pipe', 'pipe', 'pipe']
    });
    this.child.stdout.on('data', (c) => this._onData(c));
    this.child.on('error', () => {});
    this.child.on('close', () => this._onClose());
    this._initialize().catch(() => {});
  }

  restart() {
    this.stop();
    this.start();
  }

  _initialize() {
    const folders = vscode.workspace.workspaceFolders || [];
    const params = {
      processId: process.pid,
      rootUri: folders[0] ? folders[0].uri.toString() : null,
      workspaceFolders: folders.map((f) => ({ uri: f.uri.toString(), name: f.name })),
      capabilities: {
        textDocument: {
          publishDiagnostics: {},
          synchronization: { didSave: true },
          completion: {
            completionItem: {
              snippetSupport: false,
              documentationFormat: ['markdown'],
              deprecatedSupport: true
            }
          },
          hover: { contentFormat: ['markdown'] },
          definition: { linkSupport: false },
          references: {},
          documentSymbol: { hierarchicalDocumentSymbolSupport: true },
          signatureHelp: { signatureInformation: { documentationFormat: ['markdown'] } },
          semanticTokens: {}
        },
        workspace: { workspaceFolders: true }
      },
      initializationOptions: this.getInitializationOptions()
    };

    return this._request('initialize', params).then(() => {
      this.restarts = 0;
      this._notify('initialized', {});
      this._registerProviders();
      this._watchDocuments();

      for (const doc of vscode.workspace.textDocuments) {
        if (doc.languageId === 'rivo') this._sendDidOpen(doc);
      }
    });
  }

  _registerProviders() {
    const sel = { language: 'rivo', scheme: 'file' };

    this.disposables.push(
      vscode.languages.registerCompletionItemProvider(
        sel,
        {
          provideCompletionItems: async (doc, pos) => {
            const r = await this._request('textDocument/completion', {
              textDocument: { uri: doc.uri.toString() },
              position: { line: pos.line, character: pos.character }
            });
            if (!r) return [];
            const items = Array.isArray(r) ? r : r.items || [];
            return items.map((i) => this._toCompletionItem(i));
          }
        },
        '.'
      )
    );

    this.disposables.push(
      vscode.languages.registerDefinitionProvider(sel, {
        provideDefinition: async (doc, pos) => {
          const r = await this._request('textDocument/definition', {
            textDocument: { uri: doc.uri.toString() },
            position: { line: pos.line, character: pos.character }
          });
          if (!r) return null;
          return (Array.isArray(r) ? r : [r]).map((l) => this._toLocation(l));
        }
      })
    );

    this.disposables.push(
      vscode.languages.registerHoverProvider(sel, {
        provideHover: async (doc, pos) => {
          const r = await this._request('textDocument/hover', {
            textDocument: { uri: doc.uri.toString() },
            position: { line: pos.line, character: pos.character }
          });
          if (!r || !r.contents) return null;
          const c = r.contents;
          const value = typeof c === 'string' ? c : c.value || '';
          if (!value) return null;
          const range = r.range
            ? new vscode.Range(
                r.range.start.line, r.range.start.character,
                r.range.end.line, r.range.end.character
              )
            : undefined;
          return new vscode.Hover(md(value), range);
        }
      })
    );

    this.disposables.push(
      vscode.languages.registerReferenceProvider(sel, {
        provideReferences: async (doc, pos, ctx) => {
          const r = await this._request('textDocument/references', {
            textDocument: { uri: doc.uri.toString() },
            position: { line: pos.line, character: pos.character },
            context: { includeDeclaration: !!(ctx && ctx.includeDeclaration) }
          });
          if (!r) return [];
          return r.map((l) => this._toLocation(l));
        }
      })
    );

    this.disposables.push(
      vscode.languages.registerDocumentSymbolProvider(sel, {
        provideDocumentSymbols: async (doc) => {
          const r = await this._request('textDocument/documentSymbol', {
            textDocument: { uri: doc.uri.toString() }
          });
          if (!r) return [];
          return r.map((s) => this._toDocumentSymbol(s));
        }
      })
    );

    this.disposables.push(
      vscode.languages.registerSignatureHelpProvider(
        sel,
        {
          provideSignatureHelp: async (doc, pos) => {
            const r = await this._request('textDocument/signatureHelp', {
              textDocument: { uri: doc.uri.toString() },
              position: { line: pos.line, character: pos.character }
            });
            if (!r || !r.signatures || r.signatures.length === 0) return null;
            const sigs = r.signatures.map((s) => {
              const si = new vscode.SignatureInformation(s.label, md(s.documentation));
              if (s.parameters) {
                si.parameters = s.parameters.map(
                  (p) => new vscode.ParameterInformation(p.label)
                );
              }
              return si;
            });
            const help = new vscode.SignatureHelp();
            help.signatures = sigs;
            help.activeSignature = r.activeSignature || 0;
            help.activeParameter = r.activeParameter || 0;
            return help;
          }
        },
        '(', ','
      )
    );

    this.disposables.push(
      vscode.languages.registerDocumentSemanticTokensProvider(
        sel,
        {
          provideDocumentSemanticTokens: async (doc) => {
            const r = await this._request('textDocument/semanticTokens/full', {
              textDocument: { uri: doc.uri.toString() }
            });
            if (!r || !r.data) return null;
            return new vscode.SemanticTokens(new Uint32Array(r.data));
          }
        },
        SEMANTIC_LEGEND
      )
    );
  }

  _toCompletionItem(i) {
    const item = new vscode.CompletionItem(i.label, i.kind);
    if (i.detail) item.detail = i.detail;
    if (i.documentation) {
      item.documentation =
        typeof i.documentation === 'string'
          ? i.documentation
          : md(i.documentation.value);
    }
    if (i.sortText) item.sortText = i.sortText;
    if (i.insertText) item.insertText = i.insertText;
    if (i.filterText) item.filterText = i.filterText;
    return item;
  }

  _toLocation(l) {
    const target = l.uri || (l.targetUri);
    return new vscode.Location(
      vscode.Uri.parse(target),
      new vscode.Range(
        l.range.start.line, l.range.start.character,
        l.range.end.line, l.range.end.character
      )
    );
  }

  _toDocumentSymbol(s) {
    const range = new vscode.Range(
      s.range.start.line, s.range.start.character,
      s.range.end.line, s.range.end.character
    );
    const sel = s.selectionRange
      ? new vscode.Range(
          s.selectionRange.start.line, s.selectionRange.start.character,
          s.selectionRange.end.line, s.selectionRange.end.character
        )
      : range;
    const sym = new vscode.DocumentSymbol(s.name, s.detail || '', s.kind, range, sel);
    if (s.children) sym.children = s.children.map((c) => this._toDocumentSymbol(c));
    return sym;
  }

  _watchDocuments() {
    this.disposables.push(
      vscode.workspace.onDidOpenTextDocument((doc) => {
        if (doc.languageId !== 'rivo') return;
        this._sendDidOpen(doc);
      }),
      vscode.workspace.onDidChangeTextDocument((e) => {
        if (e.document.languageId !== 'rivo') return;
        this._notify('textDocument/didChange', {
          textDocument: { uri: e.document.uri.toString(), version: e.document.version },
          contentChanges: [{ text: e.document.getText() }]
        });
      }),
      vscode.workspace.onDidSaveTextDocument((doc) => {
        if (doc.languageId !== 'rivo') return;
        this._notify('textDocument/didSave', {
          textDocument: { uri: doc.uri.toString() }
        });
      }),
      vscode.workspace.onDidCloseTextDocument((doc) => {
        if (doc.languageId !== 'rivo') return;
        this._notify('textDocument/didClose', {
          textDocument: { uri: doc.uri.toString() }
        });
        this.diagnosticCollection.delete(doc.uri);
      })
    );
  }

  _sendDidOpen(doc) {
    this._notify('textDocument/didOpen', {
      textDocument: {
        uri: doc.uri.toString(),
        languageId: doc.languageId,
        version: doc.version,
        text: doc.getText()
      }
    });
  }

  _request(method, params) {
    if (!this.child || !this.child.stdin.writable) {
      return Promise.resolve(null);
    }
    const id = this.nextId++;
    const payload = JSON.stringify({ jsonrpc: '2.0', id, method, params });
    return new Promise((resolve) => {
      // timeout defensivo: nunca colgar la UI
      const timer = setTimeout(() => {
        if (this.pending.has(id)) {
          this.pending.delete(id);
          resolve(null);
        }
      }, 8000);
      this.pending.set(id, {
        resolve: (v) => { clearTimeout(timer); resolve(v); },
        reject: () => { clearTimeout(timer); resolve(null); }
      });
      this._write(payload);
    });
  }

  _notify(method, params) {
    this._write(JSON.stringify({ jsonrpc: '2.0', method, params }));
  }

  _write(json) {
    if (!this.child || !this.child.stdin.writable) return;
    const len = Buffer.byteLength(json, 'utf8');
    this.child.stdin.write('Content-Length: ' + len + '\r\n\r\n');
    this.child.stdin.write(json, 'utf8');
  }

  _onData(chunk) {
    this.buffer = Buffer.concat([this.buffer, chunk]);
    while (true) {
      const headerEnd = this.buffer.indexOf('\r\n\r\n');
      if (headerEnd === -1) return;
      const header = this.buffer.slice(0, headerEnd).toString('utf8');
      const m = /Content-Length:\s*(\d+)/i.exec(header);
      if (!m) {
        this.buffer = this.buffer.slice(headerEnd + 4);
        continue;
      }
      const length = parseInt(m[1], 10);
      const bodyStart = headerEnd + 4;
      if (this.buffer.length < bodyStart + length) return;
      const body = this.buffer.slice(bodyStart, bodyStart + length).toString('utf8');
      this.buffer = this.buffer.slice(bodyStart + length);
      let msg;
      try { msg = JSON.parse(body); } catch (e) { continue; }
      this._handle(msg);
    }
  }

  _handle(msg) {
    if (msg.id !== undefined && msg.method === undefined) {
      const p = this.pending.get(msg.id);
      if (!p) return;
      this.pending.delete(msg.id);
      if (msg.error) p.reject(new Error(msg.error.message));
      else p.resolve(msg.result);
      return;
    }

    if (msg.method === 'textDocument/publishDiagnostics') {
      const uri = vscode.Uri.parse(msg.params.uri);
      const diags = (msg.params.diagnostics || []).map((d) => {
        const range = new vscode.Range(
          d.range.start.line, d.range.start.character,
          d.range.end.line, d.range.end.character
        );
        const severity =
          d.severity === 2 ? vscode.DiagnosticSeverity.Warning
          : d.severity === 3 ? vscode.DiagnosticSeverity.Information
          : d.severity === 4 ? vscode.DiagnosticSeverity.Hint
          : vscode.DiagnosticSeverity.Error;
        const diag = new vscode.Diagnostic(range, d.message, severity);
        diag.source = d.source || 'Rivo';
        if (d.code) diag.code = d.code;
        return diag;
      });
      this.diagnosticCollection.set(uri, diags);
    }
  }

  _onClose() {
    this._flushPending();
    if (this.stopped) return;
    // caída inesperada: reintentos limitados
    if (this.restarts < 5) {
      this.restarts++;
      setTimeout(() => {
        if (!this.stopped) this.start();
      }, 1000 * this.restarts);
    }
  }

  _flushPending() {
    for (const [, p] of this.pending) p.resolve(null);
    this.pending.clear();
  }

  stop() {
    this.stopped = true;
    this._flushPending();
    for (const d of this.disposables) {
      try { d.dispose(); } catch (e) {}
    }
    this.disposables = [];
    if (this.child) {
      try { this.child.kill(); } catch (e) {}
      this.child = null;
    }
    return Promise.resolve();
  }

  dispose() {
    this.stop();
    this.diagnosticCollection.clear();
    this.diagnosticCollection.dispose();
  }
}

module.exports = { LanguageClient };
