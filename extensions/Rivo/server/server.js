'use strict';

/*
 * Rivo — Language Server (.ro) sin dependencias.
 *
 * Características:
 *   - Diagnósticos: `rivo -c -o <temporal>` (NUNCA deja .o en el proyecto)
 *   - Autocompletado: variables, funciones, structs/campos, builtins y módulos
 *   - Ir a definición (Ctrl+clic): decls locales, funciones de módulos → lib/*.ro
 *   - Hover con firmas y documentación
 *   - Referencias en el workspace
 *   - DocumentSymbol (outline/breadcrumbs)
 *   - SignatureHelp al escribir llamadas
 *   - SemanticTokens (variables, parámetros, funciones, módulos)
 */

const { spawn } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { analyze, KEYWORDS, TYPES } = require('./symbols');
const { RivoParser } = require('./parser');
const { KEYWORD_DOCS, TYPE_DOCS, OPERATOR_DOCS } = require('./keywords');
const stdlib = require('./stdlib-data');

const SEMANTIC_LEGEND = {
  tokenTypes: [
    'namespace', 'type', 'class', 'enum', 'interface', 'struct',
    'typeParameter', 'parameter', 'variable', 'property', 'enumMember',
    'event', 'method', 'macro', 'keyword', 'comment', 'string', 'number',
    'regexp', 'operator'
  ],
  tokenModifiers: ['declaration']
};

/* Doc markdown de una keyword/tipo: firma, descripción y ejemplo. */
function docMarkdown(kind, name, info) {
  let md = '**' + kind + '** `' + name + '` — ' + (info.doc || '');
  if (info.example) {
    md += '\n\n```rivo\n' + info.example + '\n```';
  }
  return md;
}

/* Operador bajo el cursor (los operadores no son palabras). */
const OPERATOR_RE = /\/\/|\+\+|--|->|\+=|-=|\*=|\/=|==|!=|>=|<=|[+\-*/%:><]/;

function operatorAt(lineText, character) {
  if (!lineText) return null;
  for (let i = 0; i < lineText.length; i++) {
    const m = OPERATOR_RE.exec(lineText.slice(i, i + 2));
    if (!m || m.index !== 0) continue;
    const end = i + m[0].length;
    if (character >= i && character <= end && OPERATOR_DOCS[m[0]]) {
      return { sym: m[0], start: i, end };
    }
  }
  return null;
}

/* -------------------------------------------------------
 * Transporte LSP (Content-Length sobre stdio)
 * ----------------------------------------------------- */

class LspTransport {
  constructor(input, output) {
    this.input = input;
    this.output = output;
    this.buffer = Buffer.alloc(0);
    this.handlers = {};
    input.on('data', (chunk) => this._onData(chunk));
  }

  on(method, handler) {
    this.handlers[method] = handler;
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
      this._dispatch(msg);
    }
  }

  _dispatch(msg) {
    if (!msg.method) return;
    const handler = this.handlers[msg.method];
    if (handler) {
      try {
        handler(msg);
      } catch (e) {
        if (msg.id !== undefined) this.respondError(msg.id, -32603, String((e && e.message) || e));
      }
      return;
    }
    if (msg.id !== undefined) this.respondError(msg.id, -32601, 'Método no soportado: ' + msg.method);
  }

  send(message) {
    const json = JSON.stringify(message);
    const payload = Buffer.byteLength(json, 'utf8');
    this.output.write('Content-Length: ' + payload + '\r\n\r\n');
    this.output.write(json, 'utf8');
  }

  respond(id, result) {
    this.send({ jsonrpc: '2.0', id, result });
  }

  respondError(id, code, message) {
    this.send({ jsonrpc: '2.0', id, error: { code, message } });
  }
}

/* -------------------------------------------------------
 * Server
 * ----------------------------------------------------- */

class RivoServer {
  constructor(transport) {
    this.transport = transport;
    this.parser = new RivoParser();
    this.documents = new Map();   // uri -> texto
    this.analyzers = new Map();   // uri -> analyze()
    this.timers = new Map();      // uri -> timeout
    this.running = new Map();     // uri -> child (diagnóstico)
    this.roPath = 'rivo';
    this.enableDiagnostics = true;
    this.enableSemantic = true;
    this.roots = [];              // rutas de workspace
    this.tempCounter = 0;
    this._bind();
  }

  _bind() {
    const t = this.transport;
    t.on('initialize', (m) => this._onInitialize(m));
    t.on('initialized', () => {});
    t.on('shutdown', (m) => t.respond(m.id, null));
    t.on('exit', () => process.exit(0));
    t.on('textDocument/didOpen', (m) => this._onDidOpen(m));
    t.on('textDocument/didChange', (m) => this._onDidChange(m));
    t.on('textDocument/didSave', (m) => this._onDidSave(m));
    t.on('textDocument/didClose', (m) => this._onDidClose(m));
    t.on('textDocument/completion', (m) => this._reply(m, () => this._completion(m.params)));
    t.on('textDocument/definition', (m) => this._reply(m, () => this._definition(m.params)));
    t.on('textDocument/hover', (m) => this._reply(m, () => this._hover(m.params)));
    t.on('textDocument/references', (m) => this._reply(m, () => this._references(m.params)));
    t.on('textDocument/documentSymbol', (m) => this._reply(m, () => this._documentSymbol(m.params)));
    t.on('textDocument/signatureHelp', (m) => this._reply(m, () => this._signatureHelp(m.params)));
    t.on('textDocument/semanticTokens/full', (m) => this._reply(m, () => this._semanticTokens(m.params)));
    t.on('rivo/didChangeConfiguration', (m) => {
      const c = (m.params && m.params.settings) || {};
      if (typeof c.compilerPath === 'string') this.roPath = c.compilerPath;
      if (typeof c.diagnostics === 'boolean') this.enableDiagnostics = c.diagnostics;
      if (typeof c.semanticTokens === 'boolean') this.enableSemantic = c.semanticTokens;
      // reanalizar y recompilar todo lo abierto
      for (const uri of this.documents.keys()) {
        this.analyzers.set(uri, analyze(this.documents.get(uri)));
        this._schedule(uri, 0);
      }
    });
  }

  _reply(msg, fn) {
    try {
      this.transport.respond(msg.id, fn());
    } catch (e) {
      this.transport.respond(msg.id, null);
    }
  }

  _onInitialize(msg) {
    const p = msg.params || {};
    this.roPath =
      (p.initializationOptions && p.initializationOptions.compilerPath) ||
      process.env.RIVO_COMPILER || 'rivo';
    if (p.initializationOptions && typeof p.initializationOptions.diagnostics === 'boolean') {
      this.enableDiagnostics = p.initializationOptions.diagnostics;
    }
    if (p.initializationOptions && typeof p.initializationOptions.semanticTokens === 'boolean') {
      this.enableSemantic = p.initializationOptions.semanticTokens;
    }
    this.roots = [];
    if (p.workspaceFolders) {
      for (const f of p.workspaceFolders) {
        const up = this._uriToPath(f.uri);
        if (up) this.roots.push(up);
      }
    } else if (p.rootUri) {
      const up = this._uriToPath(p.rootUri);
      if (up) this.roots.push(up);
    }

    this.transport.respond(msg.id, {
      capabilities: {
        textDocumentSync: { openClose: true, change: 1, save: { includeText: false } },
        completionProvider: { triggerCharacters: ['.'], resolveProvider: false },
        definitionProvider: true,
        hoverProvider: true,
        referencesProvider: true,
        documentSymbolProvider: true,
        signatureHelpProvider: { triggerCharacters: ['(', ','] },
        semanticTokensProvider: {
          legend: SEMANTIC_LEGEND,
          full: true,
          range: false
        }
      },
      serverInfo: { name: 'Rivo Language Server', version: '1.4.0' }
    });
  }

  /* ---------------- documentos ---------------- */

  _onDidOpen(msg) {
    const doc = msg.params.textDocument;
    this.documents.set(doc.uri, doc.text);
    this.analyzers.set(doc.uri, analyze(doc.text));
    this._schedule(doc.uri, 250);
  }

  _onDidChange(msg) {
    const doc = msg.params.textDocument;
    const changes = msg.params.contentChanges;
    if (changes && changes.length > 0) {
      const last = changes[changes.length - 1];
      this.documents.set(doc.uri, last.text);
      this.analyzers.set(doc.uri, analyze(last.text));
    }
    this._schedule(doc.uri, 500);
  }

  _onDidSave(msg) {
    this._schedule(msg.params.textDocument.uri, 0);
  }

  _onDidClose(msg) {
    const uri = msg.params.textDocument.uri;
    this.documents.delete(uri);
    this.analyzers.delete(uri);
    this._clearTimer(uri);
    this._kill(uri);
    this.transport.send({
      jsonrpc: '2.0',
      method: 'textDocument/publishDiagnostics',
      params: { uri, diagnostics: [] }
    });
  }

  _analyzer(uri) {
    let a = this.analyzers.get(uri);
    if (!a) {
      const text = this.documents.get(uri);
      if (text === undefined) return null;
      a = analyze(text);
      this.analyzers.set(uri, a);
    }
    return a;
  }

  /* ---------------- diagnósticos (temporal, sin .o en el proyecto) ---------------- */

  _clearTimer(uri) {
    const t = this.timers.get(uri);
    if (t) {
      clearTimeout(t);
      this.timers.delete(uri);
    }
  }

  _kill(uri) {
    const c = this.running.get(uri);
    if (c) {
      this.running.delete(uri);
      try { c.kill(); } catch (e) {}
    }
  }

  _schedule(uri, delay) {
    if (!this.enableDiagnostics) {
      this._publish(uri, []);
      return;
    }
    this._clearTimer(uri);
    if (delay <= 0) {
      this._compile(uri);
      return;
    }
    const t = setTimeout(() => {
      this.timers.delete(uri);
      this._compile(uri);
    }, delay);
    this.timers.set(uri, t);
  }

  _compile(uri) {
    const text = this.documents.get(uri);
    if (text === undefined) return;
    const filePath = this._uriToPath(uri);
    if (!filePath) return;

    this._kill(uri);
    const tmpBase = path.join(
      os.tmpdir(),
      `rivo-diag-${process.pid}-${this.tempCounter++}`
    );

    let child;
    try {
      child = spawn(this.roPath, ['-c', '-o', tmpBase, filePath], {
        cwd: path.dirname(filePath),
        stdio: ['ignore', 'pipe', 'pipe']
      });
    } catch (e) {
      return;
    }
    this.running.set(uri, child);

    let out = '';
    let err = '';
    child.stdout.on('data', (d) => { out += d.toString('utf8'); });
    child.stderr.on('data', (d) => { err += d.toString('utf8'); });
    const timer = setTimeout(() => {
      try { child.kill(); } catch (e) {}
    }, 15000);

    child.on('error', () => {
      clearTimeout(timer);
      this.running.delete(uri);
      // compilador no disponible: no publicar diagnósticos falsos
    });

    child.on('close', () => {
      clearTimeout(timer);
      if (this.running.get(uri) === child) this.running.delete(uri);
      // limpiar el objeto temporal (nunca en el proyecto)
      for (const p of [tmpBase, tmpBase + '.o']) {
        fs.unlink(p, () => {});
      }
      const diagnostics = this.parser.parse(err + '\n' + out, filePath, text);
      this._publish(uri, diagnostics);
    });
  }

  _publish(uri, diagnostics) {
    this.transport.send({
      jsonrpc: '2.0',
      method: 'textDocument/publishDiagnostics',
      params: { uri, diagnostics }
    });
  }

  /* ---------------- utilidades ---------------- */

  _uriToPath(uri) {
    if (!uri || !uri.startsWith('file://')) return null;
    let p = decodeURIComponent(uri.slice('file://'.length));
    if (/^\/[A-Za-z]:/.test(p)) p = p.slice(1);
    return p;
  }

  _pathToUri(p) {
    let u = 'file://' + p.replace(/\\/g, '/');
    if (/^\/[A-Za-z]:/.test(p)) u = 'file:///' + p.replace(/\\/g, '/');
    return encodeURI(u).replace(/#/g, '%23');
  }

  /* Localiza el fichero fuente de un módulo: .ro junto al actual,
   * lib/ relativo al compilador o al workspace. */
  _compilerDir() {
    try {
      let p = this.roPath;
      if (!p.includes('/') && !p.includes('\\')) {
        const dirs = (process.env.PATH || '').split(path.delimiter);
        for (const d of dirs) {
          const c = path.join(d, p);
          if (fs.existsSync(c)) { p = c; break; }
        }
      }
      return path.dirname(fs.realpathSync(p));
    } catch (e) {
      return null;
    }
  }

  _resolveModuleFile(moduleName, currentFile) {
    const candidates = [];
    if (currentFile) candidates.push(path.join(path.dirname(currentFile), moduleName + '.ro'));
    const compDir = this._compilerDir();
    if (compDir) {
      candidates.push(path.join(compDir, '..', 'lib', moduleName + '.ro'));
      candidates.push(path.join(compDir, 'lib', moduleName + '.ro'));
    }
    for (const root of this.roots) {
      candidates.push(path.join(root, 'lib', moduleName + '.ro'));
      candidates.push(path.join(root, moduleName + '.ro'));
      candidates.push(path.join(root, 'lib', moduleName, moduleName + '.ro'));
    }
    for (const c of candidates) {
      try {
        if (fs.existsSync(c) && fs.statSync(c).isFile()) return fs.realpathSync(c);
      } catch (e) {}
    }
    return null;
  }

  _analyzeFile(file) {
    try {
      const text = fs.readFileSync(file, 'utf8');
      return { text, a: analyze(text) };
    } catch (e) {
      return null;
    }
  }

  /* ---------------- completion ---------------- */

  _completion(params) {
    const uri = params.textDocument.uri;
    const a = this._analyzer(uri);
    if (!a) return { isIncomplete: false, items: [] };
    const pos = params.position;
    const file = this._uriToPath(uri);
    const items = [];

    // 1) tras "alias." → funciones del módulo
    const mod = a.moduleAt(pos);
    if (mod) {
      const std = stdlib.modules[mod.module];
      if (std && mod.kind === 'module') {
        for (const [name, info] of Object.entries(std.functions)) {
          items.push({
            label: name,
            kind: 3,
            detail: info.sig,
            documentation: info.doc
              ? { kind: 'markdown', value: '```rivo\n' + info.sig + '\n```\n\n' + info.doc }
              : { kind: 'markdown', value: '```rivo\n' + info.sig + '\n```' },
            sortText: '0_' + name
          });
        }
      } else {
        // módulo local (fichero .ro)
        const mf = this._resolveModuleFile(mod.module, file);
        const res = mf ? this._analyzeFile(mf) : null;
        if (res) {
          for (const fn of res.a.functions) {
            const sig = `fn ${fn.name}(${fn.params.map((p) => p.name + (p.type ? ':' + p.type : '')).join(', ')})${fn.ret ? ' -> ' + fn.ret : ''}`;
            items.push({
              label: fn.name,
              kind: 3,
              detail: sig,
              documentation: { kind: 'markdown', value: '```rivo\n' + sig + '\n```' + (fn.doc ? '\n\n' + fn.doc : '') },
              sortText: '0_' + fn.name
            });
          }
        }
      }
      if (items.length > 0) return { isIncomplete: false, items };
    }

    // 1b) tras "instancia." → campos de su struct
    const receiver = a.receiverAt(pos);
    if (receiver) {
      for (const field of receiver.struct.fields) {
        items.push({
          label: field.name,
          kind: 10,
          detail: field.type,
          sortText: '0_' + field.name,
          documentation: {
            kind: 'markdown',
            value: '`' + receiver.struct.name + '.' + field.name + '` — `' + field.type + '`'
          }
        });
      }
      return { isIncomplete: false, items };
    }

    // 2) ámbito local
    const { locals } = a.completeAt(pos);
    for (const v of locals) {
      const label = v.name;
      items.push({
        label,
        kind: 6,
        detail: v.type || v.kind,
        sortText: '0_' + label,
        documentation: {
          kind: 'markdown',
          value: '```rivo\n' + (v.kind === 'const' ? 'const ' : 'var ') + v.name + (v.type ? ':' + v.type : '') + '\n```' +
            (v.owner ? '\n\n_en `' + v.owner + '`_' : '')
        }
      });
    }
    const seen = new Set(locals.map((v) => v.name));
    for (const fn of a.functions) {
      if (seen.has(fn.name)) continue;
      seen.add(fn.name);
      const sig = `fn ${fn.name}(${fn.params.map((p) => p.name + (p.type ? ':' + p.type : '')).join(', ')})${fn.ret ? ' -> ' + fn.ret : ''}`;
      items.push({
        label: fn.name,
        kind: 3,
        detail: sig,
        sortText: '1_' + fn.name,
        documentation: { kind: 'markdown', value: '```rivo\n' + sig + '\n```' + (fn.doc ? '\n\n' + fn.doc : '') }
      });
    }
    for (const im of a.imports) {
      if (seen.has(im.alias)) continue;
      seen.add(im.alias);
      items.push({
        label: im.alias,
        kind: 9,
        detail: 'import ' + im.module,
        sortText: '1_' + im.alias
      });
    }

    // 3) builtins, módulos, tipos, keywords
    for (const [name, info] of Object.entries(stdlib.builtins)) {
      if (seen.has(name)) continue;
      seen.add(name);
      items.push({
        label: name,
        kind: 3,
        detail: info.sig,
        sortText: '1_' + name,
        documentation: info.doc
          ? { kind: 'markdown', value: '```rivo\n' + info.sig + '\n```\n\n' + info.doc }
          : { kind: 'markdown', value: '```rivo\n' + info.sig + '\n```' }
      });
    }
    for (const name of Object.keys(stdlib.modules)) {
      if (seen.has(name)) continue;
      seen.add(name);
      items.push({ label: name, kind: 9, detail: 'módulo', sortText: '1_' + name });
    }
    // módulos .ro locales del directorio
    try {
      const dir = file ? path.dirname(file) : null;
      if (dir) {
        for (const f of fs.readdirSync(dir)) {
          if (!f.endsWith('.ro')) continue;
          const name = path.basename(f, '.ro');
          if (seen.has(name)) continue;
          seen.add(name);
          items.push({ label: name, kind: 19, detail: f, sortText: '2_' + name });
        }
      }
    } catch (e) {}

    for (const t of TYPES) {
      if (seen.has(t)) continue;
      const info = TYPE_DOCS[t];
      items.push({
        label: t,
        kind: 7,
        detail: 'tipo',
        sortText: '2_' + t,
        documentation: info ? { kind: 'markdown', value: docMarkdown('tipo', t, info) } : undefined
      });
    }
    for (const s of a.structs) {
      if (seen.has(s.name)) continue;
      seen.add(s.name);
      items.push({
        label: s.name,
        kind: 7,
        detail: 'struct',
        sortText: '2_' + s.name,
        documentation: {
          kind: 'markdown',
          value: '```rivo\n' + s.name + '(' +
            s.fields.map((field) => field.name + ':' + field.type).join(', ') +
            ')\n```'
        }
      });
    }
    for (const k of KEYWORDS) {
      if (seen.has(k)) continue;
      const info = KEYWORD_DOCS[k];
      items.push({
        label: k,
        kind: 14,
        detail: info && info.group ? 'palabra clave · ' + info.group : 'palabra clave',
        sortText: '3_' + k,
        documentation: info ? { kind: 'markdown', value: docMarkdown('palabra clave', k, info) } : undefined
      });
    }

    return { isIncomplete: false, items };
  }

  /* ---------------- definición (Ctrl+clic) ---------------- */

  _definition(params) {
    const uri = params.textDocument.uri;
    const a = this._analyzer(uri);
    if (!a) return null;
    const pos = params.position;
    const lineText = a.lines[pos.line];
    const w = a.wordAt(lineText, pos.character);
    if (!w) return null;
    const file = this._uriToPath(uri);

    const member = a.fieldAt(pos);
    if (member) {
      return {
        uri,
        range: {
          start: { line: member.field.line, character: member.field.col },
          end: {
            line: member.field.line,
            character: member.field.col + member.field.len
          }
        }
      };
    }

    // ¿es "alias.palabra"? → miembro de módulo
    const before = lineText.slice(0, w.start).replace(/\s+$/, '');
    const mm = /([A-Za-z_]\w*)\s*\.$/.exec(before);
    if (mm && a.importIndex.has(mm[1])) {
      const im = a.importIndex.get(mm[1]);
      const mf = this._resolveModuleFile(im.module, file);
      if (mf) {
        const res = this._analyzeFile(mf);
        if (res) {
          const fn = res.a.functions.find((f) => f.name === w.name);
          if (fn) {
            return {
              uri: this._pathToUri(mf),
              range: {
                start: { line: fn.line, character: fn.col },
                end: { line: fn.line, character: fn.col + fn.len }
              }
            };
          }
        }
      }
      // miembro no encontrado: ir a la sentencia import
      return {
        uri,
        range: {
          start: { line: im.line, character: im.col },
          end: { line: im.line, character: im.col + im.len }
        }
      };
    }

    const decl = a.definitionOf(w.name, pos);
    if (!decl) return null;

    // ¿es un import (alias o nombre importado)? → módulo local
    if (decl.module !== undefined) {
      const mf = this._resolveModuleFile(decl.module, file);
      if (mf) {
        const res = this._analyzeFile(mf);
        if (res) {
          const target =
            res.a.functions.find((f) => f.name === (decl.name || decl.alias)) ||
            res.a.variables.find((v) => v.name === (decl.name || decl.alias));
          if (target) {
            return {
              uri: this._pathToUri(mf),
              range: {
                start: { line: target.line, character: target.col },
                end: { line: target.line, character: target.col + target.len }
              }
            };
          }
          return {
            uri: this._pathToUri(mf),
            range: { start: { line: 0, character: 0 }, end: { line: 0, character: 0 } }
          };
        }
      }
      // módulo sin fuente local: ir a la sentencia import
      return {
        uri,
        range: {
          start: { line: decl.line, character: decl.col },
          end: { line: decl.line, character: decl.col + decl.len }
        }
      };
    }

    return {
      uri,
      range: {
        start: { line: decl.line, character: decl.col },
        end: { line: decl.line, character: decl.col + decl.len }
      }
    };
  }

  /* ---------------- hover ---------------- */

  _hover(params) {
    const uri = params.textDocument.uri;
    const a = this._analyzer(uri);
    if (!a) return null;
    const pos = params.position;
    const lineText = a.lines[pos.line];
    const w = a.wordAt(lineText, pos.character);
    if (!w) {
      // operadores (//, ++, ->, +=, ...) no son palabras: hover dedicado
      const op = operatorAt(lineText, pos.character);
      if (!op) return null;
      return {
        contents: {
          kind: 'markdown',
          value: '**operador** `' + op.sym + '` — ' + OPERATOR_DOCS[op.sym].doc +
            (OPERATOR_DOCS[op.sym].example ? '\n\n```rivo\n' + OPERATOR_DOCS[op.sym].example + '\n```' : '')
        },
        range: {
          start: { line: pos.line, character: op.start },
          end: { line: pos.line, character: op.end }
        }
      };
    }

    const md = (value) => ({
      contents: { kind: 'markdown', value },
      range: {
        start: { line: pos.line, character: w.start },
        end: { line: pos.line, character: w.end }
      }
    });

    const member = a.fieldAt(pos);
    if (member) {
      return md('**campo** `' + member.struct.name + '.' + member.field.name +
        '` — `' + member.field.type + '`');
    }

    // miembro de módulo (alias.palabra)
    const before = lineText.slice(0, w.start).replace(/\s+$/, '');
    const mm = /([A-Za-z_]\w*)\s*\.$/.exec(before);
    if (mm && a.importIndex.has(mm[1])) {
      const im = a.importIndex.get(mm[1]);
      const std = stdlib.modules[im.module];
      const info = std && std.functions[w.name];
      if (info) {
        return md('`' + im.module + '`.`' + w.name + '`\n\n```rivo\n' + info.sig + '\n```' + (info.doc ? '\n\n' + info.doc : ''));
      }
      return md('`' + im.module + '`.`' + w.name + '`');
    }

    const decl = a.definitionOf(w.name, pos);
    if (decl && decl.fields !== undefined) {
      const fields = decl.fields
        .map((field) => '    var ' + field.name + ':' + field.type)
        .join('\n');
      return md('**struct** `' + decl.name + '`' +
        (fields ? '\n\n```rivo\nstruct ' + decl.name + ':\n' + fields + '\n```' : ''));
    }
    if (decl && decl.params !== undefined) {
      const sig = 'fn ' + decl.name + '(' +
        decl.params.map((p) => p.name + (p.type ? ':' + p.type : '')).join(', ') +
        ')' + (decl.ret ? ' -> ' + decl.ret : '');
      return md('```rivo\n' + sig + '\n```' + (decl.doc ? '\n\n' + decl.doc : ''));
    }
    if (decl && decl.kind !== undefined && decl.owner !== undefined) {
      const kw = decl.kind === 'const' ? 'const' : decl.kind === 'param' ? 'parámetro' : decl.kind === 'loop' ? 'variable de bucle' : 'variable';
      return md('**' + kw + '** `' + decl.name + (decl.type ? ':' + decl.type : '') + '`' + (decl.owner ? '\n\n_en `' + decl.owner + '`_' : ''));
    }
    if (decl && decl.alias !== undefined) {
      return md('**módulo** `' + decl.alias + '` → `' + decl.module + '`');
    }
    if (decl && decl.name !== undefined && decl.line !== undefined) {
      // fallback: otra decl
      return md('`' + w.name + '`');
    }

    const b = stdlib.builtins[w.name];
    if (b) {
      return md('**función incorporada**\n\n```rivo\n' + b.sig + '\n```' + (b.doc ? '\n\n' + b.doc : ''));
    }
    if (stdlib.modules[w.name]) {
      return md('**módulo** `' + w.name + '` (' + Object.keys(stdlib.modules[w.name].functions).length + ' funciones) — `import ' + w.name + '`');
    }
    if (TYPE_DOCS[w.name]) return md(docMarkdown('tipo', w.name, TYPE_DOCS[w.name]));
    if (KEYWORD_DOCS[w.name]) return md(docMarkdown('palabra clave', w.name, KEYWORD_DOCS[w.name]));
    if (KEYWORDS.has(w.name)) return md('**palabra clave** `' + w.name + '`');
    return null;
  }

  /* ---------------- referencias ---------------- */

  _references(params) {
    const uri = params.textDocument.uri;
    const a = this._analyzer(uri);
    if (!a) return null;
    const pos = params.position;
    const w = a.wordAt(a.lines[pos.line], pos.character);
    if (!w) return null;
    const includeDecl = !!(params.context && params.context.includeDeclaration);

    const out = [];
    for (const occ of a.referencesOf(w.name)) {
      if (!includeDecl && occ.line === pos.line && occ.col === w.start) continue;
      out.push({
        uri,
        range: {
          start: { line: occ.line, character: occ.col },
          end: { line: occ.line, character: occ.col + occ.len }
        }
      });
    }

    // workspace
    const file = this._uriToPath(uri);
    const re = new RegExp('\\b' + w.name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + '\\b', 'g');
    const SKIP = new Set(['node_modules', 'build', 'dist', '.git', '.rivo', '__pycache__', '.venv', 'venv', '.cache']);
    const seenFiles = new Set();
    let fileCount = 0;
    let matchCount = 0;

    const searchIn = (dir) => {
      if (fileCount > 600 || matchCount > 600) return;
      let entries;
      try { entries = fs.readdirSync(dir, { withFileTypes: true }); } catch (e) { return; }
      for (const e of entries) {
        if (fileCount > 600 || matchCount > 600) return;
        const full = path.join(dir, e.name);
        if (e.isDirectory()) {
          if (SKIP.has(e.name)) continue;
          searchIn(full);
        } else if (e.isFile() && e.name.endsWith('.ro')) {
          if (seenFiles.has(full) || full === file) continue;
          seenFiles.add(full);
          fileCount++;
          let text;
          try { text = fs.readFileSync(full, 'utf8'); } catch (err) { continue; }
          const lines = text.split(/\r?\n/);
          for (let i = 0; i < lines.length; i++) {
            re.lastIndex = 0;
            let m;
            while ((m = re.exec(lines[i])) !== null) {
              out.push({
                uri: this._pathToUri(full),
                range: {
                  start: { line: i, character: m.index },
                  end: { line: i, character: m.index + w.name.length }
                }
              });
              matchCount++;
              if (matchCount > 600) break;
            }
          }
        }
      }
    };
    for (const root of this.roots) searchIn(root);

    return out;
  }

  /* ---------------- outline ---------------- */

  _documentSymbol(params) {
    const a = this._analyzer(params.textDocument.uri);
    if (!a) return null;
    return a.documentSymbols();
  }

  /* ---------------- signature help ---------------- */

  _signatureHelp(params) {
    const uri = params.textDocument.uri;
    const a = this._analyzer(uri);
    if (!a) return null;
    const s = a.signatureAt(params.position);
    if (!s) return null;

    if (!s.builtin && s.fn) {
      const labels = s.fn.params.map((p) => p.name + (p.type ? ':' + p.type : ''));
      const sig = 'fn ' + s.fn.name + '(' + labels.join(', ') + ')' + (s.fn.ret ? ' -> ' + s.fn.ret : '');
      return {
        signatures: [{
          label: sig,
          parameters: labels.map((l) => ({ label: l })),
          documentation: s.fn.doc ? { kind: 'markdown', value: s.fn.doc } : undefined
        }],
        activeSignature: 0,
        activeParameter: Math.min(s.active, Math.max(labels.length - 1, 0))
      };
    }

    const b = stdlib.builtins[s.name];
    if (b) {
      const inner = /^\w*\((.*)\)/.exec(b.sig);
      const labels = inner && inner[1] && inner[1] !== 'escalares...'
        ? splitParams(inner[1])
        : [];
      return {
        signatures: [{
          label: b.sig,
          parameters: labels.map((l) => ({ label: l })),
          documentation: b.doc ? { kind: 'markdown', value: b.doc } : undefined
        }],
        activeSignature: 0,
        activeParameter: labels.length ? Math.min(s.active, labels.length - 1) : 0
      };
    }
    return null;
  }

  /* ---------------- semantic tokens ---------------- */

  _semanticTokens(params) {
    if (!this.enableSemantic) return { data: [] };
    const a = this._analyzer(params.textDocument.uri);
    if (!a) return { data: [] };
    return { data: a.semanticTokens() };
  }
}

function splitParams(s) {
  const out = [];
  let depth = 0;
  let cur = '';
  for (const c of s) {
    if (c === '<' || c === '(' || c === '[') depth++;
    else if (c === '>' || c === ')' || c === ']') depth--;
    if (c === ',' && depth === 0) { out.push(cur.trim()); cur = ''; continue; }
    cur += c;
  }
  if (cur.trim()) out.push(cur.trim());
  return out;
}

/* -------------------------------------------------------
 * Arranque
 * ----------------------------------------------------- */

function main() {
  const transport = new LspTransport(process.stdin, process.stdout);
  new RivoServer(transport);
}

main();
