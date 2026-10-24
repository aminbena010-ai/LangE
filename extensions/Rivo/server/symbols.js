'use strict';

/*
 * Rivo — analizador ligero de documentos .ro.
 *
 * Un único barrido por líneas produce:
 *   - words: todos los identificadores en posición de código
 *   - imports, functions, variables (decls con rangos exactos)
 *   - documentSymbols, completion, definition, references,
 *     signatureHelp y semanticTokens sobre esos datos.
 *
 * Es una aproximación heurística sin AST: suficiente para
 * IDE (autocompletado, ir a definición, outline, coloreado),
 * la verdad última sigue siendo el compilador.
 */

const KEYWORDS = new Set([
  'var', 'const', 'fn', 'struct', 'return', 'if', 'elif', 'else', 'while', 'for', 'in',
  'break', 'continue', 'pass', 'try', 'catch', 'import', 'from', 'as',
  'external', 'async', 'await', 'true', 'false', 'null', 'and', 'or', 'not'
]);

const TYPES = new Set([
  'int', 'float', 'string', 'bool', 'char', 'list', 'map', 'task'
]);

const IDENT_RE = /[A-Za-z_]\w*/g;

function isIdentStart(c) {
  return c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' || c === '_';
}

function isIdent(c) {
  return isIdentStart(c) || c >= '0' && c <= '9';
}

function wordAt(lineText, character) {
  if (lineText === undefined) return null;
  let start = character;
  while (start > 0 && isIdent(lineText[start - 1])) start--;
  let end = character;
  while (end < lineText.length && isIdent(lineText[end])) end++;
  if (start === end) return null;
  const word = lineText.slice(start, end);
  if (!/^[A-Za-z_]/.test(word)) return null;
  return { name: word, start, end };
}

function indentOf(s) {
  const m = /^[ \t]*/.exec(s)[0].replace(/\t/g, '    ');
  return m.length;
}

function stripComment(s) {
  let inStr = null;
  for (let i = 0; i < s.length; i++) {
    const c = s[i];
    if (inStr) {
      if (c === '\\') { i++; continue; }
      if (c === inStr) inStr = null;
      continue;
    }
    if (c === '"' || c === "'") { inStr = c; continue; }
    if (c === '#') return s.slice(0, i);
  }
  return s;
}

function analyze(text) {
  const lines = text.split(/\r?\n/);

  const words = [];        // {name, line, col, len}
  const imports = [];      // {alias, module, line, col, len, kind}
  const functions = [];    // {name, params, ret, line, col, len, doc, endLine}
  const structs = [];      // {name, line, col, len, fields, endLine}
  const variables = [];    // {name, type, kind, line, col, len, owner}
  const lineCodeStart = []; // ¿la línea empieza en código (fuera de bloques #! )?

  let inBlock = false;
  let activeStruct = null;

  for (let line = 0; line < lines.length; line++) {
    const s = lines[line];
    lineCodeStart.push(!inBlock);

    let i = 0;
    while (i < s.length) {
      const c = s[i];
      if (inBlock) {
        const end = s.indexOf('!#', i);
        if (end === -1) { i = s.length; } else { inBlock = false; i = end + 2; }
        continue;
      }
      if (c === '#' && s[i + 1] === '!') { inBlock = true; i += 2; continue; }
      if (c === '#') break;
      if (c === '"' || c === "'") {
        const q = c;
        i++;
        while (i < s.length) {
          if (s[i] === '\\') { i += 2; continue; }
          if (s[i] === q) { i++; break; }
          i++;
        }
        continue;
      }
      if (isIdentStart(c)) {
        let j = i;
        while (j < s.length && isIdent(s[j])) j++;
        const name = s.slice(i, j);
        // "f" seguido de comilla: prefijo de f-string, no identificador.
        if (!(name === 'f' && (s[j] === '"' || s[j] === "'"))) {
          words.push({ name, line, col: i, len: j - i });
        }
        i = j;
        continue;
      }
      i++;
    }
  }

  /* ---------------- declaraciones por línea ---------------- */

  for (let line = 0; line < lines.length; line++) {
    if (!lineCodeStart[line]) continue;
    const s = lines[line];
    const code = stripComment(s);
    if (!code.trim()) continue;

    let m;

    if (activeStruct && code.trim() &&
        indentOf(code) <= activeStruct.indent) {
      activeStruct.endLine = line - 1;
      activeStruct = null;
    }

    if ((m = /^(\s*)struct\s+([A-Za-z_]\w*)\s*:/.exec(code))) {
      const nameAt = s.indexOf(m[2], m[1].length + 'struct '.length);
      activeStruct = {
        name: m[2],
        line,
        col: nameAt,
        len: m[2].length,
        indent: indentOf(m[1]),
        fields: [],
        endLine: lines.length - 1
      };
      structs.push(activeStruct);
      continue;
    }

    if (activeStruct &&
        (m = /^(\s*)var\s+([A-Za-z_]\w*)\s*:\s*([^=\n]+)$/.exec(code)) &&
        indentOf(m[1]) > activeStruct.indent) {
      const col = s.indexOf(m[2], m[1].length);
      activeStruct.fields.push({
        name: m[2],
        type: m[3].trim(),
        line,
        col,
        len: m[2].length,
        kind: 'field',
        struct: activeStruct.name
      });
      continue;
    }

    // fn nombre(...) -> tipo:  (también tras `external "..."`)
    const fnRe = /^\s*(?:external\s+"[^"]*"\s+)?fn\s+([A-Za-z_]\w*)\s*\(/;
    if ((m = fnRe.exec(code))) {
      const sm = /^\s*(?:external\s+"[^"]*"\s+)?fn\s+([A-Za-z_]\w*)\s*\(/.exec(s) || m;
      const name = m[1];
      const declHead = sm[0].slice(0, sm[0].lastIndexOf('('));
      const nameAt = Math.max(declHead.lastIndexOf(name), 0);
      const open = s.indexOf('(', nameAt + name.length);
      const params = extractParams(lines, line, open, s);
      const ret = extractReturn(lines, line, open, params.endOffsetLine);
      // doc: líneas # inmediatamente encima
      const docLines = [];
      for (let k = line - 1; k >= 0 && docLines.length < 3; k--) {
        const prev = lines[k].trim();
        if (prev.startsWith('#') && !prev.startsWith('#!')) docLines.unshift(prev.replace(/^#+\s?/, ''));
        else break;
      }
      functions.push({
        name,
        params: params.list,
        ret,
        line,
        col: nameAt,
        len: name.length,
        openCol: open,
        doc: docLines.join(' '),
        paramsRaw: params.raw
      });
      continue;
    }

    // var/const nombre: tipo ...
    if ((m = /^(\s*)(?:var|const)\s+([A-Za-z_]\w*)\s*:\s*([^=\n]+)/.exec(code))) {
      const nameAt = s.indexOf(m[2], m[1].length);
      variables.push({
        name: m[2],
        type: m[3].trim().replace(/\s+$/, ''),
        kind: /^\s*var/.test(code) ? 'var' : 'const',
        line,
        col: nameAt,
        len: m[2].length
      });
      continue;
    }

    // for x in ...
    if ((m = /^(\s*)for\s+([A-Za-z_]\w*)\s+in\b/.exec(code))) {
      const nameAt = s.indexOf(m[2], m[1].length);
      variables.push({
        name: m[2],
        type: '',
        kind: 'loop',
        line,
        col: nameAt,
        len: m[2].length
      });
      continue;
    }

    // import modulo [as alias]
    if ((m = /^(\s*)import\s+([A-Za-z_]\w*)(?:\s+as\s+([A-Za-z_]\w*))?/.exec(code))) {
      const alias = m[3] || m[2];
      const col = s.indexOf(alias, m[1].length);
      imports.push({ alias, module: m[2], line, col, len: alias.length, kind: 'module' });
      continue;
    }

    // from modulo import a, b [as c]
    if ((m = /^(\s*)from\s+([A-Za-z_]\w*)\s+import\s+(.+)$/.exec(code))) {
      const items = m[3].split(',');
      for (const raw of items) {
        const parts = raw.trim().split(/\s+as\s+/);
        const name = (parts[0] || '').trim();
        const alias = (parts[1] || parts[0] || '').trim();
        if (!/^[A-Za-z_]\w*$/.test(name)) continue;
        const col = s.indexOf(alias, m[1].length + m[3].indexOf(alias));
        imports.push({ alias, module: m[2], name, line, col, len: alias.length, kind: 'member' });
      }
      continue;
    }
  }

  /* ---------------- cuerpos de función y propietarios ---------------- */

  functions.sort((a, b) => a.line - b.line);
  for (let i = 0; i < functions.length; i++) {
    const start = functions[i].line;
    const indent = indentOf(lines[start]);
    let end = lines.length - 1;
    for (let k = start + 1; k < lines.length; k++) {
      const t = lines[k];
      if (t.trim() === '') continue;
      if (indentOf(t) <= indent && /^\s*(?:struct|fn|external|import|from)\b/.test(t)) { end = k - 1; break; }
    }
    functions[i].endLine = end;
  }

  // parámetros -> variables kind=param
  for (const fn of functions) {
    for (const p of fn.params) {
      const lineText = lines[fn.line] || '';
      const col = lineText.indexOf(p.name, fn.openCol >= 0 ? fn.openCol : 0);
      variables.push({
        name: p.name,
        type: p.type || '',
        kind: 'param',
        line: fn.line,
        col: col >= 0 ? col : fn.col,
        len: p.name.length,
        owner: fn.name
      });
    }
  }

  for (const v of variables) {
    if (v.owner !== undefined) continue;
    let owner = null;
    for (const fn of functions) {
      if (v.line >= fn.line && v.line <= fn.endLine) owner = fn.name;
    }
    v.owner = owner;
  }

  /* ---------------- índice de nombres ---------------- */

  const declIndex = new Map(); // name -> [decl...]
  function pushDecl(name, decl) {
    if (!declIndex.has(name)) declIndex.set(name, []);
    const arr = declIndex.get(name);
    arr.push(decl);
  }
  for (const v of variables) pushDecl(v.name, v);
  for (const f of functions) pushDecl(f.name, f);
  for (const s of structs) pushDecl(s.name, s);
  for (const im of imports) pushDecl(im.alias, im);

  const importIndex = new Map(); // alias -> import
  for (const im of imports) importIndex.set(im.alias, im);

  /* ---------------- utilidades de posición ---------------- */

  function locationOf(d) {
    return {
      uri: null, // lo rellena el server
      range: {
        start: { line: d.line, character: d.col },
        end: { line: d.line, character: d.col + d.len }
      }
    };
  }

  function completionOwner(line) {
    let owner = null;
    for (const fn of functions) {
      if (fn.line <= line && line <= fn.endLine) owner = fn;
    }
    return owner;
  }

  function completeAt(pos) {
    const owner = completionOwner(pos.line);
    const locals = [];
    for (const v of variables) {
      const inScope = v.owner === null || (owner !== null && v.owner === owner.name);
      if (inScope && v.line <= pos.line) locals.push(v);
    }
    return { locals, owner };
  }

  function moduleAt(pos) {
    const lineText = lines[pos.line];
    if (lineText === undefined) return null;
    const before = lineText.slice(0, pos.character);
    const m = /([A-Za-z_]\w*)\s*\.\s*[A-Za-z_]*$/.exec(before);
    if (!m) return null;
    return importIndex.get(m[1]) || null;
  }

  function structForType(type) {
    const m = /^\s*(?:list\s*<\s*)?([A-Za-z_]\w*)/.exec(type || '');
    if (!m) return null;
    return structs.find((s) => s.name === m[1]) || null;
  }

  function receiverAt(pos) {
    const lineText = lines[pos.line];
    if (lineText === undefined) return null;
    const before = lineText.slice(0, pos.character);
    const m = /([A-Za-z_]\w*)(?:\s*\[[^\]]*\])?\s*\.\s*[A-Za-z_]*$/.exec(before);
    if (!m) return null;

    const owner = completionOwner(pos.line);
    const variable = variables
      .filter((v) =>
        v.name === m[1] &&
        v.line <= pos.line &&
        (v.owner === null || (owner !== null && v.owner === owner.name))
      )
      .sort((a, b) => b.line - a.line)[0];
    if (!variable) return null;

    const struct = structForType(variable.type);
    return struct ? { struct, receiver: m[1] } : null;
  }

  function fieldAt(pos) {
    const lineText = lines[pos.line];
    const word = wordAt(lineText, pos.character);
    const receiver = receiverAt(pos);
    if (!word || !receiver) return null;
    const field = receiver.struct.fields.find((f) => f.name === word.name);
    return field ? { field, struct: receiver.struct } : null;
  }

  function definitionOf(name, pos) {
    const decls = declIndex.get(name);
    if (!decls || decls.length === 0) return null;
    if (decls.length === 1) return decls[0];
    // preferir decls locales/cercanas al cursor
    let best = decls[0];
    let bestDist = Infinity;
    for (const d of decls) {
      const dist = Math.abs(d.line - pos.line);
      if (d.kind === 'param' || d.kind === 'var' || d.kind === 'loop') {
        if (d.line <= pos.line && dist < bestDist) { best = d; bestDist = dist; }
      } else if (dist < bestDist) { best = d; bestDist = dist; }
    }
    return best;
  }

  function referencesOf(name) {
    const out = [];
    for (const w of words) {
      if (w.name === name) out.push(w);
    }
    return out;
  }

  function signatureAt(pos) {
    const lineText = lines[pos.line];
    if (lineText === undefined) return null;
    // nombre seguido de "(" con el cursor dentro de los paréntesis
    const before = lineText.slice(0, pos.character);
    let depth = 0;
    let nameStart = -1;
    for (let i = before.length - 1; i >= 0; i--) {
      const c = before[i];
      if (c === ')') depth++;
      else if (c === '(') {
        if (depth === 0) { nameStart = i; break; }
        depth--;
      }
    }
    if (nameStart < 0) return null;
    const head = before.slice(0, nameStart);
    const m = /([A-Za-z_]\w*)\s*$/.exec(head);
    if (!m) return null;
    const decl = declIndex.get(m[1]);
    let fn = null;
    if (decl) fn = decl.find((d) => d.params !== undefined) || null;
    const active = countArgsIn(before.slice(nameStart + 1));
    if (!fn && decl) {
      const struct = decl.find((d) => d.fields !== undefined);
      if (struct) {
        fn = {
          name: struct.name,
          params: struct.fields.map((field) => ({
            name: field.name,
            type: field.type
          })),
          ret: '',
          doc: ''
        };
      }
    }
    if (!fn) return { name: m[1], builtin: true, params: null, active };
    return { name: m[1], builtin: false, fn, active };
  }

  function countArgsIn(s) {
    let depth = 0;
    let commas = 0;
    for (const c of s) {
      if (c === '(' || c === '[' || c === '{') depth++;
      else if (c === ')' || c === ']' || c === '}') depth--;
      else if (c === ',' && depth === 0) commas++;
    }
    return commas;
  }

  function documentSymbols() {
    const out = [];
    for (const s of structs) {
      out.push({
        name: s.name,
        detail: 'struct',
        kind: 23,
        range: {
          start: { line: s.line, character: 0 },
          end: { line: s.endLine, character: lines[s.endLine] ? lines[s.endLine].length : 0 }
        },
        selectionRange: {
          start: { line: s.line, character: s.col },
          end: { line: s.line, character: s.col + s.len }
        },
        children: s.fields.map((field) => ({
          name: field.name,
          detail: field.type,
          kind: 7,
          range: {
            start: { line: field.line, character: 0 },
            end: { line: field.line, character: lines[field.line].length }
          },
          selectionRange: {
            start: { line: field.line, character: field.col },
            end: { line: field.line, character: field.col + field.len }
          }
        }))
      });
    }
    for (const fn of functions) {
      out.push({
        name: fn.name,
        detail: `fn(${fn.params.map((p) => p.name + (p.type ? ':' + p.type : '')).join(', ')})${fn.ret ? ' -> ' + fn.ret : ''}`,
        kind: 12, // Function
        range: {
          start: { line: fn.line, character: 0 },
          end: { line: fn.line, character: lines[fn.line].length }
        },
        selectionRange: {
          start: { line: fn.line, character: fn.col },
          end: { line: fn.line, character: fn.col + fn.len }
        },
        children: []
      });
    }
    for (const v of variables) {
      if (v.kind === 'param') continue;
      out.push({
        name: v.name,
        detail: v.type || v.kind,
        kind: v.kind === 'const' ? 21 : 13, // Constant : Variable
        range: {
          start: { line: v.line, character: 0 },
          end: { line: v.line, character: lines[v.line] ? lines[v.line].length : 0 }
        },
        selectionRange: {
          start: { line: v.line, character: v.col },
          end: { line: v.line, character: v.col + v.len }
        },
        children: []
      });
    }
    for (const im of imports) {
      out.push({
        name: im.alias,
        detail: 'import ' + im.module,
        kind: 9, // Module
        range: {
          start: { line: im.line, character: 0 },
          end: { line: im.line, character: lines[im.line].length }
        },
        selectionRange: {
          start: { line: im.line, character: im.col },
          end: { line: im.line, character: im.col + im.len }
        },
        children: []
      });
    }
    out.sort((a, b) => a.range.start.line - b.range.start.line);
    return out;
  }

  /* ---------------- semantic tokens ---------------- */

  function semanticTokens() {
    const kinds = new Map(); // "line:col" -> {type, decl}
    for (const fn of functions) kinds.set(fn.line + ':' + fn.col, { t: 12, d: true });
    for (const s of structs) {
      kinds.set(s.line + ':' + s.col, { t: 1, d: true });
      for (const field of s.fields) {
        kinds.set(field.line + ':' + field.col, { t: 9, d: true });
      }
    }
    for (const v of variables) {
      const t = v.kind === 'param' ? 7 : 8;
      kinds.set(v.line + ':' + v.col, { t, d: true });
    }
    for (const im of imports) kinds.set(im.line + ':' + im.col, { t: 0, d: true });

    // referencias: nombre declarado -> tipo del decl
    const byName = new Map();
    for (const fn of functions) {
      if (!byName.has(fn.name)) byName.set(fn.name, 12);
    }
    for (const v of variables) {
      const t = v.kind === 'param' ? 7 : 8;
      if (!byName.has(v.name)) byName.set(v.name, t);
      else if (v.kind === 'param') byName.set(v.name, 7);
    }
    for (const im of imports) if (!byName.has(im.alias)) byName.set(im.alias, 0);

    const out = [];
    let lastLine = 0;
    let lastCol = 0;
    for (const w of words) {
      if (KEYWORDS.has(w.name) || TYPES.has(w.name)) continue;
      let k = kinds.get(w.line + ':' + w.col) || null;
      if (!k) {
        if (structs.some((s) => s.name === w.name)) {
          k = { t: 1, d: false };
        }
        const before = (lines[w.line] || '').slice(0, w.col).replace(/\s+$/, '');
        const member = /([A-Za-z_]\w*)(?:\s*\[[^\]]*\])?\s*\.$/.exec(before);
        if (member) {
          const variable = variables
            .filter((v) => v.name === member[1] && v.line <= w.line)
            .sort((a, b) => b.line - a.line)[0];
          const struct = variable && structForType(variable.type);
          if (struct && struct.fields.some((f) => f.name === w.name)) {
            k = { t: 9, d: false };
          }
        }
        // ¿alias.miembro? -> función o propiedad
        const lineText = lines[w.line] || '';
        const prev = lineText.slice(0, w.col).replace(/\s+$/, '');
        if (!k && prev.endsWith('.')) {
          const mm = /([A-Za-z_]\w*)\s*\.$/.exec(prev);
          if (mm && importIndex.has(mm[1])) {
            const after = lineText.slice(w.col + w.len);
            k = { t: /^\s*\(/.test(after) ? 12 : 9, d: false };
          }
        }
        if (!k) {
          const t = byName.get(w.name);
          if (t === undefined) continue;
          k = { t, d: false };
        }
      }
      // Codificación LSP: si deltaLine > 0, deltaStart es absoluto
      // (respecto al inicio de línea); solo se resta en la misma línea.
      const deltaLine = w.line - lastLine;
      const deltaStart = deltaLine === 0 ? w.col - lastCol : w.col;
      out.push(deltaLine, deltaStart, w.len, k.t, k.d ? 1 : 0);
      lastLine = w.line;
      lastCol = w.col;
    }
    return out;
  }

  /* ---------------- búsqueda en el workspace (referencias) ---------------- */

  function locationOfWord(w) {
    return {
      range: {
        start: { line: w.line, character: w.col },
        end: { line: w.line, character: w.col + w.len }
      }
    };
  }

  return {
    lines,
    words,
    imports,
    functions,
    structs,
    variables,
    declIndex,
    importIndex,
    wordAt,
    moduleAt,
    receiverAt,
    fieldAt,
    completeAt,
    definitionOf,
    referencesOf,
    signatureAt,
    documentSymbols,
    semanticTokens,
    locationOf,
    locationOfWord
  };
}

/* Extrae parámetros de `fn nombre(` partiendo del paréntesis de apertura.
 * Soporta firma multilínea: sigue hasta equilibrar paréntesis. */
function extractParams(lines, line, openCol, firstLine) {
  let text = firstLine.slice(openCol + 1);
  let lineIdx = line;
  let depth = 1;
  let guard = 0;
  while (depth > 0 && guard++ < 50) {
    for (let i = 0; i < text.length; i++) {
      if (text[i] === '(') depth++;
      else if (text[i] === ')') { depth--; if (depth === 0) { text = text.slice(0, i); break; } }
    }
    if (depth > 0) {
      lineIdx++;
      if (lineIdx >= lines.length) break;
      text += '\n' + lines[lineIdx];
    }
  }
  const raw = text.replace(/\n/g, ' ').trim();
  const list = [];
  if (raw) {
    for (const part of splitTop(raw)) {
      const p = part.trim();
      if (!p) continue;
      const idx = p.indexOf(':');
      if (idx >= 0) list.push({ name: p.slice(0, idx).trim(), type: p.slice(idx + 1).trim() });
      else list.push({ name: p, type: '' });
    }
  }
  return { list, raw, endOffsetLine: lineIdx };
}

function splitTop(s) {
  const out = [];
  let depth = 0;
  let cur = '';
  for (const c of s) {
    if (c === '<' || c === '(' || c === '[') depth++;
    else if (c === '>' || c === ')' || c === ']') depth--;
    if (c === ',' && depth === 0) { out.push(cur); cur = ''; continue; }
    cur += c;
  }
  if (cur.trim()) out.push(cur);
  return out;
}

function extractReturn(lines, line, openCol, endLine) {
  for (let k = line; k <= Math.min(endLine, line + 20); k++) {
    const src = lines[k];
    if (src === undefined) break;
    const code = stripComment(src);
    const arrow = code.indexOf('->');
    if (arrow !== -1) {
      const m = /^([^:#]+)/.exec(code.slice(arrow + 2));
      if (m && m[1].trim()) return m[1].trim();
    }
    // la firma termina en ":"; a partir de ahí no miramos
    if (code.trim().endsWith(':')) break;
  }
  return '';
}

module.exports = { analyze, KEYWORDS, TYPES };
