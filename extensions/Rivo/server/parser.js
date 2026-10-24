'use strict';

/*
 * Rivo — Parser de la salida del compilador E++.
 *
 * Entrada (formato unificado estilo Python, stderr):
 *
 *   TypeError: se esperaba int pero se encontro string
 *     --> fichero.ro:2:9
 *
 * Salida: lista de Diagnostic de LSP (0-indexed).
 *
 * Maneja también el antiguo formato por si el binario `rivo`
 * es anterior al rediseño:
 *
 *   error[E1003]: mensaje
 *     --> fichero.ro:1:16
 */

const SEVERITY_ERROR = 1;

/* Clase -> severidad/tipo por si en el futuro se quieren
 * mapear algunas clases a warning. Hoy todo es error. */
const KNOWN_CLASSES = new Set([
  'SyntaxError', 'TypeError', 'NameError', 'ValueError',
  'AttributeError', 'ImportError', 'IndexError', 'KeyError',
  'FileNotFoundError', 'MemoryError', 'RuntimeError',
  'SystemError', 'OSError', 'ZeroDivisionError',
]);

class RivoParser {
  parse(output, fallbackFile, text) {
    const diagnostics = [];
    if (!output) return diagnostics;

    const lines = output.split(/\r?\n/);

    /* Estado de la línea actual mientras se recorre */
    let pending = null;

    for (let i = 0; i < lines.length; i++) {
      const raw = lines[i];
      const line = raw.trimEnd();
      if (!line.trim()) continue;

      /* Línea de ubicación: "  --> file:line:col" */
      const loc = /^\s*-->\s+(.+?):(\d+):(\d+)\s*$/.exec(line);
      if (loc && pending) {
        pending.file = loc[1];
        pending.line = parseInt(loc[2], 10);
        pending.column = parseInt(loc[3], 10);
        diagnostics.push(this._finish(pending, fallbackFile, text));
        pending = null;
        continue;
      }

      /* Fin de un bloque sin ubicación (detalle/sugerencia) */
      if (/^\s+=\s/.test(line)) {
        if (pending) {
          diagnostics.push(this._finish(pending, fallbackFile, text));
          pending = null;
        }
        continue;
      }

      /* Cabecera de error: "<Clase>: <mensaje>" */
      const head = this._parseHeader(line);
      if (head) {
        /* Si había un error sin ubicación, cerrarlo antes */
        if (pending) {
          diagnostics.push(this._finish(pending, fallbackFile, text));
        }
        pending = head;
        continue;
      }

      /* Resumen del driver ("N errores") o líneas del snippet: ignorar */
    }

    if (pending) {
      diagnostics.push(this._finish(pending, fallbackFile, text));
    }

    return diagnostics;
  }

  _parseHeader(line) {
    /* Formato nuevo: "TypeError: mensaje"
     * Acepta CUALQUIERA clase terminada en "Error": si no,
     * el diagnóstico se descarta entero (bug de la 0.2.x). */
    const nuevo = /^\s*([A-Za-z]+Error)\s*:\s*(.*)$/.exec(line);
    if (nuevo) {
      return {
        className: nuevo[1],
        message: nuevo[2].trim() || nuevo[1],
        file: null, line: null, column: null,
      };
    }

    /* Formato antiguo: "error[E1003]: mensaje" */
    const viejo = /^\s*error\[E\d+\]\s*:\s*(.*)$/.exec(line);
    if (viejo) {
      return {
        className: 'Error',
        message: viejo[1].trim(),
        file: null, line: null, column: null,
      };
    }

    return null;
  }

  _finish(pending, fallbackFile, text) {
    /* LSP es 0-indexed; el compilador reporta 1-indexed */
    const line = pending.line !== null ? Math.max(0, pending.line - 1) : 0;
    const col = pending.column !== null ? Math.max(0, pending.column - 1) : 0;

    /* Subraya el token real (identificador/literal que empieza
     * en la columna marcada), no solo 1 carácter. */
    let endCol = col + 1;
    if (text && pending.line !== null && pending.column !== null) {
      const srcLines = text.split(/\r?\n/);
      const src = srcLines[pending.line - 1];
      if (src !== undefined && col <= src.length) {
        const rest = src.slice(col);
        const m = /^[A-Za-z0-9_.]+/.exec(rest);
        endCol = m ? col + m[0].length : Math.min(col + 1, src.length);
      }
    }

    const range = {
      start: { line: line, character: col },
      end: { line: line, character: Math.max(col + 1, endCol) },
    };

    return {
      range: range,
      severity: SEVERITY_ERROR,
      code: pending.className,
      source: 'Rivo',
      message: pending.className + ': ' + pending.message,
    };
  }
}

module.exports = { RivoParser };
