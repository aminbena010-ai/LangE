'use strict';

/*
 * Test end-to-end del language server (sin VSCode).
 *
 *   node scripts/test-server.js
 *
 * Cubre: initialize, diagnósticos (sin .o en el proyecto),
 * completion, definition (incl. salto a lib/math.ro), hover,
 * documentSymbol, signatureHelp, semanticTokens y references.
 */

const { spawn } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

const RIVO_ROOT = path.resolve(__dirname, '..', '..', '..');
const SERVER = path.resolve(__dirname, '..', 'server', 'server.js');
const COMPILER = path.join(RIVO_ROOT, 'build', 'rivo');
const FIXTURE_DIR = path.join(os.tmpdir(), 'rivo-lsp-test');
const FIXTURE = path.join(FIXTURE_DIR, 'main.ro');

const SRC = [
  'import math',
  '',
  'fn suma(a:int, b:int) -> int:',
  '    var total:int = a + b',
  '    return total',
  '',
  'fn main():',
  '    var mal:int = 3.14',
  '    print(mal, suma(1, 2), math.square_root(4.0), to_float("2.5"))',
  '',
  'struct Point:',
  '    var x:float',
  '    var y:float',
  '',
  'fn point_demo():',
  '    var point:Point = Point(1.0, 2.0)',
  '    print(point.x)',
  ''
].join('\n');

let failures = 0;
const tmpBefore = new Set(
  fs.readdirSync(os.tmpdir()).filter((f) => f.startsWith('rivo-diag-'))
);
function check(name, ok, extra) {
  if (ok) {
    console.log('PASS  ' + name);
  } else {
    failures++;
    console.log('FAIL  ' + name + (extra !== undefined ? '  [' + extra + ']' : ''));
  }
}

/* ---------------- mini cliente LSP ---------------- */

const child = spawn(process.execPath, [SERVER], {
  stdio: ['pipe', 'pipe', 'inherit']
});

let buffer = Buffer.alloc(0);
const pending = new Map();
const listeners = [];
let nextId = 1;

child.stdout.on('data', (chunk) => {
  buffer = Buffer.concat([buffer, chunk]);
  while (true) {
    const h = buffer.indexOf('\r\n\r\n');
    if (h === -1) return;
    const m = /Content-Length:\s*(\d+)/i.exec(buffer.slice(0, h).toString('utf8'));
    if (!m) { buffer = buffer.slice(h + 4); continue; }
    const len = parseInt(m[1], 10);
    const start = h + 4;
    if (buffer.length < start + len) return;
    const body = buffer.slice(start, start + len).toString('utf8');
    buffer = buffer.slice(start + len);
    let msg;
    try { msg = JSON.parse(body); } catch (e) { continue; }
    if (msg.id !== undefined && msg.method === undefined) {
      const p = pending.get(msg.id);
      if (p) {
        pending.delete(msg.id);
        msg.error ? p.reject(new Error(msg.error.message)) : p.resolve(msg.result);
      }
    } else {
      for (const fn of listeners) fn(msg);
    }
  }
});

function send(method, params, isNotification) {
  const msg = { jsonrpc: '2.0', method, params };
  if (!isNotification) msg.id = nextId++;
  const json = JSON.stringify(msg);
  child.stdin.write('Content-Length: ' + Buffer.byteLength(json) + '\r\n\r\n' + json);
  if (isNotification) return Promise.resolve(null);
  return new Promise((resolve, reject) => {
    pending.set(msg.id, { resolve, reject });
    setTimeout(() => {
      if (pending.has(msg.id)) {
        pending.delete(msg.id);
        reject(new Error('timeout: ' + method));
      }
    }, 15000);
  });
}

function waitForDiagnostics(uri, timeout) {
  return new Promise((resolve) => {
    const t = setTimeout(() => {
      remove();
      resolve(null);
    }, timeout || 15000);
    const fn = (msg) => {
      if (msg.method === 'textDocument/publishDiagnostics' && msg.params.uri === uri) {
        clearTimeout(t);
        remove();
        resolve(msg.params.diagnostics);
      }
    };
    listeners.push(fn);
    function remove() {
      const i = listeners.indexOf(fn);
      if (i >= 0) listeners.splice(i, 1);
    }
  });
}

function posOf(lineText, needle, offset) {
  const i = lineText.indexOf(needle);
  if (i < 0) throw new Error('no se encontró ' + needle + ' en: ' + lineText);
  return i + (offset === undefined ? needle.length : offset);
}

/* ---------------- tests ---------------- */

async function main() {
  fs.mkdirSync(FIXTURE_DIR, { recursive: true });
  fs.rmSync(FIXTURE + '.o', { force: true });
  fs.writeFileSync(FIXTURE, SRC);
  const uri = 'file://' + FIXTURE;

  const diagPromise = waitForDiagnostics(uri);

  await send('initialize', {
    processId: process.pid,
    rootUri: 'file://' + RIVO_ROOT,
    workspaceFolders: [{ uri: 'file://' + RIVO_ROOT, name: 'E++' }],
    capabilities: {},
    initializationOptions: {
      compilerPath: COMPILER,
      diagnostics: true,
      semanticTokens: true
    }
  });
  await send('initialized', {}, true);
  await send('textDocument/didOpen', {
    textDocument: { uri, languageId: 'rivo', version: 1, text: SRC }
  }, true);

  /* 1. diagnósticos en vivo + cero .o en el proyecto */
  const diags = await diagPromise;
  check('diagnósticos: error de tipo detectado',
    Array.isArray(diags) && diags.length >= 1 &&
    diags.some((d) => /se esperaba int/.test(d.message)),
    JSON.stringify(diags));
  const stray = fs.readdirSync(FIXTURE_DIR).filter((f) => f.endsWith('.o'));
  check('diagnósticos: sin .o en el directorio del proyecto', stray.length === 0, stray.join(','));

  const lines = SRC.split('\n');

  /* 2. completion tras "math." */
  {
    const line = 8;
    const character = posOf(lines[line], 'math.');
    const r = await send('textDocument/completion', {
      textDocument: { uri }, position: { line, character }
    });
    const labels = (r.items || r || []).map((i) => i.label);
    check('completion de módulo (math. → square_root)', labels.includes('square_root'), labels.slice(0, 8).join(','));
    const sqrtItem = (r.items || r).find((i) => i.label === 'square_root');
    check('completion de módulo con firma', sqrtItem && /square_root\(/.test(sqrtItem.detail || ''), JSON.stringify(sqrtItem && sqrtItem.detail));
  }

  /* 3. completion en ámbito local */
  {
    const r = await send('textDocument/completion', {
      textDocument: { uri }, position: { line: 8, character: 4 }
    });
    const labels = (r.items || r || []).map((i) => i.label);
    check('completion local: variable del ámbito', labels.includes('mal'), labels.slice(0, 12).join(','));
    check('completion local: builtin presente', labels.includes('to_float'));
    check('completion local: keyword presente', labels.includes('while'));
    check('completion local: NO variable de otra función', !labels.includes('total'));
    const mal = (r.items || r).find((i) => i.label === 'mal');
    check('completion local: detalle de tipo', mal && mal.detail === 'int', mal && mal.detail);
    const tf = (r.items || r).find((i) => i.label === 'to_float');
    check('completion: documentación de builtin', tf && /analiza float/.test(tf.documentation && tf.documentation.value || ''), JSON.stringify(tf && tf.documentation));
  }

  /* 4. definición */
  {
    const line = 8;
    const char = posOf(lines[line], 'suma');
    const r = await send('textDocument/definition', {
      textDocument: { uri }, position: { line, character: char }
    });
    const loc = Array.isArray(r) ? r[0] : r;
    check('definition: función local → su decl', loc && loc.uri === uri && loc.range.start.line === 2, JSON.stringify(loc));

    const char2 = posOf(lines[line], 'square_root');
    const r2 = await send('textDocument/definition', {
      textDocument: { uri }, position: { line, character: char2 }
    });
    const loc2 = Array.isArray(r2) ? r2[0] : r2;
    check('definition: math.square_root → lib/math.ro',
      loc2 && /lib\/math\.ro$/.test(loc2.uri),
      JSON.stringify(loc2 && loc2.uri));
  }

  /* 5. hover */
  {
    const line = 8;
    const char = posOf(lines[line], 'to_float');
    const r = await send('textDocument/hover', {
      textDocument: { uri }, position: { line, character: char }
    });
    check('hover: builtin con documentación',
      r && /analiza float/.test(r.contents.value),
      r && r.contents.value);

    const char2 = posOf(lines[line], 'suma');
    const r2 = await send('textDocument/hover', {
      textDocument: { uri }, position: { line, character: char2 }
    });
    check('hover: función local con firma',
      r2 && /fn suma\(a:int, b:int\) -> int/.test(r2.contents.value),
      r2 && r2.contents.value);

    const r3 = await send('textDocument/hover', {
      textDocument: { uri }, position: { line: 0, character: 8 }
    });
    check('hover: alias de módulo', r3 && /módulo/.test(r3.contents.value), r3 && r3.contents.value);
  }

  /* 6. outline */
  {
    const r = await send('textDocument/documentSymbol', { textDocument: { uri } });
    const names = (r || []).map((s) => s.name);
    check('documentSymbol: funciones y variables', names.includes('suma') && names.includes('main') && names.includes('total') && names.includes('mal'), names.join(','));
  }

  /* 7. signatureHelp */
  {
    const line = 8;
    const char = posOf(lines[line], 'suma(');
    const r = await send('textDocument/signatureHelp', {
      textDocument: { uri }, position: { line, character: char }
    });
    check('signatureHelp: firma de función local',
      r && r.signatures && /fn suma\(a:int, b:int\) -> int/.test(r.signatures[0].label),
      JSON.stringify(r && r.signatures && r.signatures[0] && r.signatures[0].label));
    check('signatureHelp: activo en 1er parámetro', r && r.activeParameter === 0, r && r.activeParameter);

    const char2 = posOf(lines[line], 'suma(1,');
    const r2 = await send('textDocument/signatureHelp', {
      textDocument: { uri }, position: { line, character: char2 }
    });
    check('signatureHelp: 2º parámetro activo', r2 && r2.activeParameter === 1, r2 && r2.activeParameter);
  }

  /* 8. semanticTokens */
  {
    const r = await send('textDocument/semanticTokens/full', { textDocument: { uri } });
    check('semanticTokens: datos múltiplos de 5 y no vacíos',
      r && Array.isArray(r.data) && r.data.length >= 25 && r.data.length % 5 === 0,
      r && r.data && r.data.length);
    // decodificar y comprobar tipos esperados
    if (r && r.data) {
      let line = 0, col = 0;
      const found = [];
      for (let i = 0; i < r.data.length; i += 5) {
        const dl = r.data[i];
        line += dl;
        col = dl === 0 ? col + r.data[i + 1] : r.data[i + 1];
        found.push({ line, col, len: r.data[i + 2], type: r.data[i + 3], decl: r.data[i + 4] === 1 });
      }
      const ns = found.find((t) => t.line === 0 && t.type === 0);
      const fn = found.find((t) => t.line === 2 && t.type === 12 && t.decl);
      const param = found.find((t) => t.type === 7 && t.decl);
      const memberFn = found.find((t) => t.line === 8 && t.type === 12 && !t.decl);
      const structLine = lines.findIndex((line) => line === 'struct Point:');
      const fieldLine = lines.findIndex((line) => line === '    var x:float');
      const structType = found.find((t) =>
        t.line === structLine && t.type === 1 && t.decl
      );
      const field = found.find((t) =>
        t.line === fieldLine && t.type === 9 && t.decl
      );
      check('semanticTokens: namespace de import', !!ns, JSON.stringify(found.slice(0, 4)));
      check('semanticTokens: declaración de función', !!fn);
      check('semanticTokens: parámetros', !!param);
      check('semanticTokens: miembro de módulo como función', !!memberFn, JSON.stringify(found.filter((t) => t.line === 8)));
      check('semanticTokens: tipo struct', !!structType);
      check('semanticTokens: campo struct', !!field);
    }
  }

  /* 9. references */
  {
    const char = posOf(lines[3], 'total');
    const r = await send('textDocument/references', {
      textDocument: { uri },
      position: { line: 3, character: char },
      context: { includeDeclaration: true }
    });
    const inFile = (r || []).filter((l) => l.uri === uri);
    check('references: decl + uso en el fichero', inFile.length >= 2, inFile.length);
    check('references: saltos a otros .ro del workspace',
      (r || []).some((l) => l.uri !== uri),
      (r || []).length);
  }

  /* 10. hover de palabras reservadas y operadores */
  {
    const r = await send('textDocument/hover', {
      textDocument: { uri }, position: { line: 2, character: 1 }
    });
    check('hover: palabra reservada fn con doc y ejemplo',
      r && /palabra clave/.test(r.contents.value) && /fn suma/.test(r.contents.value),
      r && r.contents.value);

    const charPlus = posOf(lines[3], ' + ', 1);
    const r2 = await send('textDocument/hover', {
      textDocument: { uri }, position: { line: 3, character: charPlus + 1 }
    });
    check('hover: operador + con doc', r2 && /operador/.test(r2.contents.value), r2 && r2.contents.value);

    const r3 = await send('textDocument/hover', {
      textDocument: { uri }, position: { line: 0, character: 0 }
    });
    check('hover: keyword import con doc', r3 && /palabra clave/.test(r3.contents.value), r3 && r3.contents.value);
  }

  /* 11. keywords nuevas (async/await/task) */
  {
    const r = await send('textDocument/completion', {
      textDocument: { uri }, position: { line: 8, character: 4 }
    });
    const items = r.items || r || [];
    const asyncItem = items.find((i) => i.label === 'async');
    const taskItem = items.find((i) => i.label === 'task');
    check('completion: keyword async con documentación',
      asyncItem && /palabra clave/.test(asyncItem.detail || '') &&
      /task</.test((asyncItem.documentation && asyncItem.documentation.value) || ''),
      JSON.stringify(asyncItem && asyncItem.detail));
    check('completion: tipo task con documentación',
      taskItem && /task</.test((taskItem.documentation && taskItem.documentation.value) || ''),
      JSON.stringify(taskItem && taskItem.documentation));
  }

  /* 12. limpieza de temporales */
  {
    const pointLine = lines.findIndex((line) => line.includes('print(point.x)'));
    const fieldDeclLine = lines.findIndex((line) => line === '    var x:float');
    const structLine = lines.findIndex((line) => line === 'struct Point:');

    const completion = await send('textDocument/completion', {
      textDocument: { uri },
      position: {
        line: pointLine,
        character: posOf(lines[pointLine], 'point.', 0) + 'point.'.length
      }
    });
    const fieldItems = completion.items || completion || [];
    check('completion: campos del struct tras punto.',
      fieldItems.some((item) => item.label === 'x') &&
      fieldItems.some((item) => item.label === 'y'),
      fieldItems.map((item) => item.label).join(','));

    const definition = await send('textDocument/definition', {
      textDocument: { uri },
      position: { line: pointLine, character: posOf(lines[pointLine], 'x') }
    });
    check('definition: acceso a campo → declaración',
      definition && definition.uri === uri &&
      definition.range.start.line === fieldDeclLine,
      JSON.stringify(definition));

    const hoverField = await send('textDocument/hover', {
      textDocument: { uri },
      position: { line: pointLine, character: posOf(lines[pointLine], 'x') }
    });
    check('hover: campo tipado',
      hoverField && /Point\.x/.test(hoverField.contents.value) &&
      /float/.test(hoverField.contents.value),
      hoverField && hoverField.contents.value);

    const hoverStruct = await send('textDocument/hover', {
      textDocument: { uri },
      position: {
        line: pointLine - 1,
        character: posOf(lines[pointLine - 1], 'Point')
      }
    });
    check('hover: tipo struct', hoverStruct && /struct.*Point/.test(hoverStruct.contents.value),
      hoverStruct && hoverStruct.contents.value);

    const signature = await send('textDocument/signatureHelp', {
      textDocument: { uri },
      position: {
        line: pointLine - 1,
        character: posOf(lines[pointLine - 1], 'Point(') + 'Point('.length
      }
    });
    check('signatureHelp: constructor de struct',
      signature && signature.signatures &&
      /Point\(x:float, y:float\)/.test(signature.signatures[0].label),
      signature && signature.signatures && signature.signatures[0].label);

    const outline = await send('textDocument/documentSymbol', { textDocument: { uri } });
    const point = (outline || []).find((symbol) => symbol.name === 'Point');
    check('documentSymbol: struct con campos',
      point && point.range.start.line === structLine &&
      point.children.some((field) => field.name === 'x'),
      JSON.stringify(point));

    const items = await send('textDocument/completion', {
      textDocument: { uri }, position: { line: pointLine, character: 4 }
    });
    check('completion: tipos struct declarados',
      (items.items || []).some((item) => item.label === 'Point'));
  }

  /* 13. limpieza de temporales */
  await new Promise((r) => setTimeout(r, 700));
  const tmpLeft = fs.readdirSync(os.tmpdir()).filter(
    (f) => f.startsWith('rivo-diag-') && !tmpBefore.has(f)
  );
  check('temporales de diagnóstico eliminados', tmpLeft.length === 0, tmpLeft.join(','));

  await send('shutdown', {}, false).catch(() => {});
  await send('exit', {}, true).catch(() => {});
  child.kill();

  console.log('----------------------------------------');
  if (failures === 0) {
    console.log('SERVER TESTS: PASS');
    process.exit(0);
  } else {
    console.log('SERVER TESTS: ' + failures + ' fallan');
    process.exit(1);
  }
}

main().catch((e) => {
  console.error('ERROR:', e);
  try { child.kill(); } catch (x) {}
  process.exit(1);
});
