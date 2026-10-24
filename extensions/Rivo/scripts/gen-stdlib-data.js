'use strict';

/*
 * Genera server/stdlib-data.js a partir de las fuentes reales:
 *   - include/builtins.h        -> builtins (nombres, aridad, tipos)
 *   - lib/*.ro                  -> módulos, funciones y firmas
 *
 * Uso:  node scripts/gen-stdlib-data.js
 * El resultado está versionado; reejecutar tras tocar builtins.h/claude.md/lib.
 */

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..', '..');
const OUT = path.resolve(__dirname, '..', 'server', 'stdlib-data.js');

/* ---------------- builtins.h ---------------- */

const TYPE_MAP = {
  TYPE_INT: 'int',
  TYPE_FLOAT: 'float',
  TYPE_STRING: 'string',
  TYPE_BOOL: 'bool',
  TYPE_CHAR: 'char',
  TYPE_LIST: 'list',
  TYPE_MAP: 'map',
  TYPE_NULL: null
};

/* Builtins implementados en el compilador pero ausentes de la tabla
 * de builtins.h (casos especiales de semantic.c/llvm.c). */
const EXTRA_BUILTINS = [
  'print', 'input', 'len', 'str', 'range', 'divmod',
  'sorted', 'reversed', 'all', 'any'
];

function parseBuiltinsH() {
  const src = fs.readFileSync(path.join(ROOT, 'include', 'builtins.h'), 'utf8');
  const out = {};
  const re = /\{\s*"([A-Za-z_]\w*)"\s*,\s*(\d+)\s*,\s*\{([^}]*)\}\s*,\s*(\w+)\s*\}/g;
  let m;
  while ((m = re.exec(src))) {
    const name = m[1];
    const arity = parseInt(m[2], 10);
    const argTokens = m[3].split(',').map((s) => s.trim()).filter(Boolean);
    const args = argTokens.slice(0, arity).map((t) => TYPE_MAP[t] || '').filter(Boolean);
    const ret = TYPE_MAP[m[4]] || '';
    out[name] = { args, ret, doc: '' };
  }
  for (const name of EXTRA_BUILTINS) {
    if (!out[name]) out[name] = { args: [], ret: '', doc: '' };
  }
  return out;
}

/* ---------------- claude.md: firmas documentadas ---------------- */

function parseClaudeDocs(builtins) {
  const src = fs.readFileSync(path.join(ROOT, 'document', 'claude.md'), 'utf8');
  const lines = src.split(/\r?\n/);
  const re = /^\s{4}([a-z_]\w*)\(([^)]*)\)\s*(?:->\s*([^#]*?))?\s*(?:#\s*(.*))?$/;
  for (const line of lines) {
    const m = re.exec(line);
    if (!m) continue;
    const name = m[1];
    if (!Object.prototype.hasOwnProperty.call(builtins, name)) continue;
    const params = m[2].trim();
    const ret = (m[3] || '').trim();
    const doc = (m[4] || '').trim();
    // Descartar ejemplos de llamada (ej. print(f"hola {M}")):
    // una firma lleva parámetros tipados, retorno o documentación.
    const typedOk =
      params === '' ||
      params.split(',').every((p) => /^[a-z_]\w*\s*:\s*[\w<>,\s\]]+$/.test(p.trim()));
    const looksLikeSig = typedOk || ret !== '' || doc !== '';
    if (!looksLikeSig) continue;
    const sig = `${name}(${params})${ret ? ' -> ' + ret : ''}`;
    const cur = builtins[name];
    if (!cur.sig || doc) {
      cur.sig = sig;
      if (doc) cur.doc = doc;
      if (ret && !cur.ret) cur.ret = ret;
      if (params && cur.params === undefined) cur.params = params;
    }
  }

  // Fallbacks manuales para builtins de consola (claude.md solo trae ejemplos)
  const MANUAL = {
    print: {
      sig: 'print(escalares...)',
      doc: 'Imprime uno o más escalares separados por espacios y añade una nueva línea.'
    },
    input: {
      sig: 'input(prompt?:string) -> string',
      doc: 'Lee una línea de la consola; en fin de entrada devuelve cadena vacía.'
    },
    len: {
      sig: 'len(valor) -> int',
      doc: 'Longitud de un string o de una lista.'
    },
    str: {
      sig: 'str(valor) -> string',
      doc: 'Convierte un escalar a string.'
    }
  };
  for (const [name, info] of Object.entries(MANUAL)) {
    if (!builtins[name]) continue;
    if (!builtins[name].sig || builtins[name].sig.includes('"')) builtins[name].sig = info.sig;
    if (!builtins[name].doc) builtins[name].doc = info.doc;
  }
}

/* ---------------- lib/*.ro: módulos ---------------- */

function parseLib() {
  const dir = path.join(ROOT, 'lib');
  const modules = {};
  for (const file of fs.readdirSync(dir)) {
    if (!file.endsWith('.ro')) continue;
    const name = path.basename(file, '.ro');
    const src = fs.readFileSync(path.join(dir, file), 'utf8');
    const lines = src.split(/\r?\n/);
    const fns = {};
    const re = /^(?:external\s+"[^"]*"\s+)?fn\s+([A-Za-z_]\w*)\s*\(([^)]*)\)\s*(?:->\s*([^:]+?))?\s*(?::\s*)?$/;
    for (let i = 0; i < lines.length; i++) {
      const line = lines[i].replace(/#.*$/, '');
      const m = re.exec(line);
      if (!m) continue;
      const params = m[2].trim();
      const ret = (m[3] || '').trim();
      const docLines = [];
      for (let k = i - 1; k >= 0 && docLines.length < 2; k--) {
        const prev = lines[k].trim();
        if (prev.startsWith('#') && !prev.startsWith('#!')) docLines.unshift(prev.replace(/^#+\s?/, ''));
        else break;
      }
      fns[m[1]] = {
        sig: `${m[1]}(${params})${ret ? ' -> ' + ret : ''}`,
        doc: docLines.join(' ')
      };
    }
    if (Object.keys(fns).length > 0) modules[name] = { functions: fns };
  }
  return modules;
}

/* ---------------- salida ---------------- */

function main() {
  const builtins = parseBuiltinsH();
  parseClaudeDocs(builtins);

  for (const [name, b] of Object.entries(builtins)) {
    if (!b.sig) {
      const params = b.args.map((t, i) => `${t || 'any'}`).join(', ');
      b.sig = `${name}(${params})${b.ret ? ' -> ' + b.ret : ''}`;
    }
    if (b.params !== undefined) delete b.params;
    delete b.args;
    delete b.ret;
  }

  const modules = parseLib();
  const payload = { builtins, modules };
  const json = JSON.stringify(payload, null, 2);
  const header =
    "'use strict';\n\n" +
    "/* GENERADO por scripts/gen-stdlib-data.js — NO editar a mano.\n" +
    " * Fuentes: include/builtins.h, lib/*.ro */\n\n" +
    'module.exports = ' + json + ';\n';
  fs.writeFileSync(OUT, header);
  const nB = Object.keys(builtins).length;
  const nM = Object.keys(modules).length;
  let nF = 0;
  for (const m of Object.values(modules)) nF += Object.keys(m.functions).length;
  console.log(`stdlib-data.js: ${nB} builtins, ${nM} módulos, ${nF} funciones`);
}

main();
