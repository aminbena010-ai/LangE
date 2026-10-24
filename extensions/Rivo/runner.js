'use strict';

/*
 * Rivo — compilación y ejecución.
 *
 * Todo artefacto (binario, .o) va a <workspace>/.rivo/ (configurable):
 * el directorio de los .ro queda limpio. Ejecución en una terminal
 * integrada reutilizable (stdin interactivo funciona).
 */

const vscode = require('vscode');
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

let channel = null;
let terminal = null;
let terminalCwd = null;

function outputChannel() {
  if (!channel) channel = vscode.window.createOutputChannel('Rivo');
  return channel;
}

function config() {
  return vscode.workspace.getConfiguration('rivo');
}

function resolveCompiler(file) {
  const raw = config().get('compilerPath', 'rivo');
  if (raw.includes('/') || raw.includes('\\')) {
    const folder = vscode.workspace.getWorkspaceFolder(vscode.Uri.file(file));
    const base = folder ? folder.uri.fsPath : process.cwd();
    return path.isAbsolute(raw) ? raw : path.resolve(base, raw);
  }
  return raw;
}

function buildDirFor(file) {
  const rel = config().get('buildDir', '.rivo');
  const folder = vscode.workspace.getWorkspaceFolder(vscode.Uri.file(file));
  const base = folder ? folder.uri.fsPath : path.dirname(file);
  return path.isAbsolute(rel) ? rel : path.join(base, rel);
}

function ensureGitignore(file, outDir) {
  if (!config().get('manageGitignore', true)) return;
  const folder = vscode.workspace.getWorkspaceFolder(vscode.Uri.file(file));
  if (!folder) return;
  const root = folder.uri.fsPath;
  const gitDir = path.join(root, '.git');
  const giPath = path.join(root, '.gitignore');
  if (!fs.existsSync(gitDir) && !fs.existsSync(giPath)) return;

  const relOut = path.relative(root, outDir).replace(/\\/g, '/').replace(/^\.\//, '');
  if (!relOut || relOut.startsWith('..')) return;
  const entry = relOut.replace(/\/+$/, '') + '/';

  let content = '';
  try { content = fs.readFileSync(giPath, 'utf8'); } catch (e) {}
  const lines = content.split(/\r?\n/).map((l) => l.trim());
  const bare = entry.replace(/\/$/, '');
  if (lines.includes(entry) || lines.includes(bare) || lines.includes('/' + entry)) return;

  const prefix = content && !content.endsWith('\n') ? '\n' : '';
  try {
    fs.writeFileSync(giPath, content + prefix + entry + '\n');
    outputChannel().appendLine(`gitignore: añadida ${entry}`);
  } catch (e) {}
}

async function activeRoFile() {
  const ed = vscode.window.activeTextEditor;
  if (!ed) {
    vscode.window.showWarningMessage('Rivo: abre un fichero .ro primero');
    return null;
  }
  const fsPath = ed.document.uri.fsPath;
  if (!fsPath || !fsPath.endsWith('.ro')) {
    vscode.window.showWarningMessage('Rivo: el fichero activo no tiene extensión .ro');
    return null;
  }
  if (ed.document.isDirty) await ed.document.save();
  return fsPath;
}

/*
 * Bibliotecas del sistema que exige el módulo `mgl`
 * (ventanas X11+Xft): sin ellas el enlace falla con
 * símbolos de X11 sin resolver.
 */
const MGL_LINK_FLAGS = ['-l', 'X11', '-l', 'Xft', '-l', 'fontconfig'];

function stripComments(text) {
  return text
    .replace(/#![\s\S]*?!#/g, '')
    .split(/\r?\n/)
    .map((l) => l.replace(/#.*$/, ''))
    .join('\n');
}

/*
 * ¿El programa (o un módulo local que importa) usa `import mgl`?
 *
 * Los imports se resuelven como el compilador: un módulo es un
 * .ro situado junto al fichero que lo importa. Solo se recorren
 * esos ficheros locales; el resto (math, text...) vive en la
 * biblioteca estándar y no necesita flags de enlace.
 */
function usesMgl(file, seen) {
  if (!seen) seen = new Set();

  const key = path.resolve(file);
  if (seen.has(key)) return false;
  seen.add(key);

  let code;
  try {
    code = stripComments(fs.readFileSync(file, 'utf8'));
  } catch (e) {
    return false;
  }

  if (/(?:^|\n)\s*import\s+mgl\b/.test(code)) return true;
  if (/(?:^|\n)\s*from\s+mgl\s+import\b/.test(code)) return true;

  const importRe = /(?:^|\n)\s*(?:import|from)\s+([A-Za-z_]\w*)/g;
  let m;
  while ((m = importRe.exec(code))) {
    const local = path.join(path.dirname(file), m[1] + '.ro');
    if (fs.existsSync(local) && usesMgl(local, seen)) return true;
  }

  return false;
}

function compileFile(file, log) {
  return new Promise((resolve) => {
    const compiler = resolveCompiler(file);
    const outDir = buildDirFor(file);
    try {
      fs.mkdirSync(outDir, { recursive: true });
    } catch (e) {
      log.appendLine(`No se pudo crear ${outDir}: ${e.message}`);
      resolve(null);
      return;
    }
    const bin = path.join(outDir, path.basename(file, '.ro'));
    const relIn = path.relative(path.dirname(file), file);
    const relOut = path.relative(path.dirname(file), bin);

    const args = [file, '-o', bin];
    const mgl = usesMgl(file);

    if (mgl) args.push(...MGL_LINK_FLAGS);

    log.appendLine(
      `> ${compiler} ${relIn} -o ${relOut}` +
        (mgl ? ` ${MGL_LINK_FLAGS.join(' ')}` : '')
    );

    const child = spawn(compiler, args, {
      cwd: path.dirname(file),
      stdio: ['ignore', 'pipe', 'pipe']
    });
    child.stdout.on('data', (d) => log.append(d.toString('utf8')));
    child.stderr.on('data', (d) => log.append(d.toString('utf8')));
    child.on('error', (e) => {
      log.appendLine(`No se pudo lanzar '${compiler}': ${e.message}`);
      vscode.window
        .showErrorMessage(
          "Rivo: no se encontró el compilador ('" + compiler + "').",
          'Configurar rivo.compilerPath'
        )
        .then((choice) => {
          if (choice) {
            vscode.commands.executeCommand(
              'workbench.action.openSettings',
              'rivo.compilerPath'
            );
          }
        });
      resolve(null);
    });
    child.on('close', (code) => {
      if (code === 0) {
        ensureGitignore(file, outDir);
        resolve(bin);
      } else {
        log.show(true);
        vscode.window
          .showErrorMessage(
            'Rivo: errores de compilación (detalle en el canal "Rivo").',
            'Ver panel Problems'
          )
          .then((choice) => {
            if (choice) {
              vscode.commands.executeCommand(
                'workbench.actions.view.problems'
              );
            }
          });
        resolve(null);
      }
    });
  });
}

async function compileOnly() {
  const file = await activeRoFile();
  if (!file) return;
  const log = outputChannel();
  log.show(true);
  const bin = await vscode.window.withProgress(
    { location: vscode.ProgressLocation.Notification, title: 'Rivo: compilando…' },
    () => compileFile(file, log)
  );
  if (bin) {
    vscode.window.showInformationMessage(
      `Rivo: compilado → ${path.relative(
        vscode.workspace.getWorkspaceFolder(vscode.Uri.file(file))?.uri.fsPath || path.dirname(file),
        bin
      )}`
    );
  }
}

async function runFile() {
  const file = await activeRoFile();
  if (!file) return;
  const log = outputChannel();
  const bin = await vscode.window.withProgress(
    { location: vscode.ProgressLocation.Notification, title: 'Rivo: compilando…' },
    () => compileFile(file, log)
  );
  if (!bin) return;

  const cwd = path.dirname(file);
  if (!terminal || terminalCwd !== cwd) {
    if (terminal) {
      try { terminal.dispose(); } catch (e) {}
    }
    terminal = vscode.window.createTerminal({ name: 'Rivo', cwd });
    terminalCwd = cwd;
  }
  terminal.show();
  terminal.sendText(`"${bin}"`);
}

async function formatFile() {
  const file = await activeRoFile();
  if (!file) return;
  const compiler = resolveCompiler(file);
  const log = outputChannel();
  log.show(true);

  const child = spawn(compiler, ['fmt', file], {
    cwd: path.dirname(file),
    stdio: ['ignore', 'pipe', 'pipe']
  });
  child.stdout.on('data', (d) => log.append(d.toString('utf8')));
  child.stderr.on('data', (d) => log.append(d.toString('utf8')));
  child.on('error', (e) => {
    vscode.window.showErrorMessage(`Rivo: no se pudo ejecutar '${compiler}': ${e.message}`);
  });
  child.on('close', (code) => {
    if (code === 0) {
      vscode.window.showInformationMessage('Rivo: fichero formateado');
    } else if (code === 1) {
      vscode.window.showWarningMessage('Rivo: el fichero necesita formato');
    } else {
      vscode.window.showErrorMessage('Rivo: error al formatear (detalle en el canal "Rivo").');
    }
  });
}

function dispose() {
  if (terminal) {
    try { terminal.dispose(); } catch (e) {}
    terminal = null;
    terminalCwd = null;
  }
}

module.exports = { runFile, compileOnly, formatFile, dispose, outputChannel };
