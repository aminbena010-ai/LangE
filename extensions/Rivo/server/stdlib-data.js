'use strict';

/* GENERADO por scripts/gen-stdlib-data.js — NO editar a mano.
 * Fuentes: include/builtins.h, lib/*.ro */

module.exports = {
  "builtins": {
    "random": {
      "doc": "[min, max] incluidos; min > max aborta",
      "sig": "random(min:int, max:int) -> int"
    },
    "random_float": {
      "doc": "[0.0, 1.0)",
      "sig": "random_float() -> float"
    },
    "seed": {
      "doc": "fija la semilla (reproducible)",
      "sig": "seed(valor:int)"
    },
    "time_ms": {
      "doc": "ms monótonos desde la primera llamada",
      "sig": "time_ms() -> int"
    },
    "epoch_ms": {
      "doc": "ms desde la época",
      "sig": "epoch_ms() -> int"
    },
    "concat": {
      "doc": "igual que a + b",
      "sig": "concat(a:string, b:string) -> string"
    },
    "split": {
      "doc": "separador vacío: un carácter por elemento",
      "sig": "split(texto:string, separador:string) -> list<string>"
    },
    "join": {
      "doc": "",
      "sig": "join(lista:list<string>, separador:string) -> string"
    },
    "upper": {
      "doc": "",
      "sig": "upper(texto:string) -> string"
    },
    "lower": {
      "doc": "",
      "sig": "lower(texto:string) -> string"
    },
    "trim": {
      "doc": "espacios por delante y por detrás",
      "sig": "trim(texto:string) -> string"
    },
    "substring": {
      "doc": "",
      "sig": "substring(texto:string, inicio:int, longitud:int) -> string"
    },
    "replace": {
      "doc": "",
      "sig": "replace(texto:string, viejo:string, nuevo:string) -> string"
    },
    "contains": {
      "doc": "",
      "sig": "contains(texto:string, parte:string) -> bool"
    },
    "starts_with": {
      "doc": "",
      "sig": "starts_with(texto:string, prefijo:string) -> bool"
    },
    "ends_with": {
      "doc": "",
      "sig": "ends_with(texto:string, sufijo:string) -> bool"
    },
    "find": {
      "doc": "posición o -1",
      "sig": "find(texto:string, parte:string) -> int"
    },
    "ord": {
      "doc": "código del primer carácter; \"\" aborta",
      "sig": "ord(texto:string) -> int"
    },
    "chr": {
      "doc": "carácter del código; rango 1–255 (0 no representable)",
      "sig": "chr(codigo:int) -> caracter"
    },
    "to_int": {
      "doc": "analiza entero; texto no válido aborta (ValueError)",
      "sig": "to_int(texto:string) -> int"
    },
    "to_float": {
      "doc": "analiza float; texto no válido aborta (ValueError)",
      "sig": "to_float(texto:string) -> float"
    },
    "bin": {
      "doc": "\"0b...\"",
      "sig": "bin(valor:int) -> string"
    },
    "oct": {
      "doc": "\"0o...\"",
      "sig": "oct(valor:int) -> string"
    },
    "hex": {
      "doc": "\"0x...\" (minúsculas)",
      "sig": "hex(valor:int) -> string"
    },
    "round": {
      "doc": "entero más cercano (la mitad se aleja de cero)",
      "sig": "round(valor:float) -> int"
    },
    "pow": {
      "doc": "potencia entera; exponente negativo aborta (ValueError)",
      "sig": "pow(base:int, exponente:int) -> int"
    },
    "abs": {
      "doc": "valor absoluto",
      "sig": "abs(valor:int) -> int"
    },
    "read_file": {
      "doc": "error de runtime si no existe",
      "sig": "read_file(ruta:string) -> string"
    },
    "write_file": {
      "doc": "",
      "sig": "write_file(ruta:string, contenido:string) -> bool"
    },
    "file_exists": {
      "doc": "",
      "sig": "file_exists(ruta:string) -> bool"
    },
    "clear": {
      "doc": "borra la pantalla (ANSI)",
      "sig": "clear()"
    },
    "move": {
      "doc": "cursor, mínimo 1 (ANSI)",
      "sig": "move(fila:int, columna:int)"
    },
    "sleep_ms": {
      "doc": "espera; negativos no hacen nada",
      "sig": "sleep_ms(milisegundos:int)"
    },
    "read_key": {
      "doc": "una tecla sin Intro; EOF devuelve el carácter 0",
      "sig": "read_key() -> caracter"
    },
    "print": {
      "doc": "Imprime uno o más escalares separados por espacios y añade una nueva línea.",
      "sig": "print(escalares...)"
    },
    "input": {
      "doc": "Lee una línea de la consola; en fin de entrada devuelve cadena vacía.",
      "sig": "input(prompt?:string) -> string"
    },
    "len": {
      "doc": "Longitud de un string o de una lista.",
      "sig": "len(valor) -> int"
    },
    "str": {
      "doc": "int|float|bool|caracter|string a texto",
      "sig": "str(valor) -> string"
    },
    "range": {
      "doc": "[inicio, fin)",
      "sig": "range(inicio:int, fin:int) -> list<int>"
    },
    "divmod": {
      "doc": "[a/b, a%b]; b == 0 aborta (ZeroDivisionError)",
      "sig": "divmod(a:int, b:int) -> list<int>"
    },
    "sorted": {
      "doc": "copia ordenada ascendente",
      "sig": "sorted(lista:list<int>) -> list<int>"
    },
    "reversed": {
      "doc": "copia con el orden invertido",
      "sig": "reversed(lista:list<T>) -> list<T>"
    },
    "all": {
      "doc": "todos ciertos; lista vacía -> true",
      "sig": "all(lista:list<bool>) -> bool"
    },
    "any": {
      "doc": "alguno cierto; lista vacía -> false",
      "sig": "any(lista:list<bool>) -> bool"
    }
  },
  "modules": {
    "base64": {
      "functions": {
        "encode": {
          "sig": "encode(texto:string) -> string",
          "doc": ""
        },
        "decode": {
          "sig": "decode(texto:string) -> string",
          "doc": ""
        }
      }
    },
    "calendar": {
      "functions": {
        "leap_year": {
          "sig": "leap_year(year:int) -> bool",
          "doc": ""
        },
        "month_length": {
          "sig": "month_length(year:int, month:int) -> int",
          "doc": ""
        },
        "month_label": {
          "sig": "month_label(month:int) -> string",
          "doc": ""
        },
        "weekday_index": {
          "sig": "weekday_index(iso_date:string) -> int",
          "doc": ""
        },
        "weekday_label": {
          "sig": "weekday_label(iso_date:string) -> string",
          "doc": ""
        }
      }
    },
    "codecs": {
      "functions": {
        "_base64_tabla": {
          "sig": "_base64_tabla() -> string",
          "doc": ""
        },
        "_hex_mayus": {
          "sig": "_hex_mayus() -> string",
          "doc": ""
        },
        "_hex_minus": {
          "sig": "_hex_minus() -> string",
          "doc": ""
        },
        "base64_encode": {
          "sig": "base64_encode(t:string) -> string",
          "doc": ""
        },
        "base64_decode": {
          "sig": "base64_decode(t:string) -> string",
          "doc": ""
        },
        "hex_encode": {
          "sig": "hex_encode(t:string) -> string",
          "doc": ""
        },
        "hex_decode": {
          "sig": "hex_decode(t:string) -> string",
          "doc": ""
        },
        "url_quote": {
          "sig": "url_quote(t:string) -> string",
          "doc": ""
        },
        "url_unquote": {
          "sig": "url_unquote(t:string) -> string",
          "doc": ""
        },
        "rot13": {
          "sig": "rot13(t:string) -> string",
          "doc": ""
        }
      }
    },
    "color": {
      "functions": {
        "color_colorize": {
          "sig": "color_colorize(codigo:int, texto:string) -> string",
          "doc": ""
        },
        "color_no_color": {
          "sig": "color_no_color(texto:string) -> string",
          "doc": ""
        },
        "color_colorize_256": {
          "sig": "color_colorize_256(codigo:int, plano:int, texto:string) -> string",
          "doc": ""
        },
        "color_colorize_rgb": {
          "sig": "color_colorize_rgb(r:int, g:int, b:int, plano:int, texto:string) -> string",
          "doc": ""
        },
        "colorize": {
          "sig": "colorize(texto:string, codigo:int) -> string",
          "doc": ""
        },
        "red": {
          "sig": "red(texto:string) -> string",
          "doc": ""
        },
        "green": {
          "sig": "green(texto:string) -> string",
          "doc": ""
        },
        "yellow": {
          "sig": "yellow(texto:string) -> string",
          "doc": ""
        },
        "blue": {
          "sig": "blue(texto:string) -> string",
          "doc": ""
        },
        "magenta": {
          "sig": "magenta(texto:string) -> string",
          "doc": ""
        },
        "cyan": {
          "sig": "cyan(texto:string) -> string",
          "doc": ""
        },
        "white": {
          "sig": "white(texto:string) -> string",
          "doc": ""
        },
        "black": {
          "sig": "black(texto:string) -> string",
          "doc": ""
        },
        "gray": {
          "sig": "gray(texto:string) -> string",
          "doc": ""
        },
        "bold": {
          "sig": "bold(texto:string) -> string",
          "doc": ""
        },
        "underline": {
          "sig": "underline(texto:string) -> string",
          "doc": ""
        },
        "invert": {
          "sig": "invert(texto:string) -> string",
          "doc": ""
        },
        "reset": {
          "sig": "reset(texto:string) -> string",
          "doc": ""
        },
        "color_256": {
          "sig": "color_256(texto:string, codigo:int) -> string",
          "doc": ""
        },
        "color_256_fondo": {
          "sig": "color_256_fondo(texto:string, codigo:int) -> string",
          "doc": ""
        },
        "color_rgb": {
          "sig": "color_rgb(texto:string, r:int, g:int, b:int) -> string",
          "doc": ""
        },
        "color_rgb_fondo": {
          "sig": "color_rgb_fondo(texto:string, r:int, g:int, b:int) -> string",
          "doc": ""
        },
        "no_color": {
          "sig": "no_color(texto:string) -> string",
          "doc": ""
        }
      }
    },
    "csv": {
      "functions": {
        "_encode_field": {
          "sig": "_encode_field(campo:string) -> string",
          "doc": ""
        },
        "_needs_quotes": {
          "sig": "_needs_quotes(campo:string) -> bool",
          "doc": ""
        },
        "quote": {
          "sig": "quote(campo:string) -> string",
          "doc": ""
        },
        "unquote": {
          "sig": "unquote(campo:string) -> string",
          "doc": ""
        },
        "parse": {
          "sig": "parse(texto_csv:string) -> list<list<string>>",
          "doc": ""
        },
        "row_to_string": {
          "sig": "row_to_string(fila:list<string>) -> string",
          "doc": ""
        },
        "format": {
          "sig": "format(filas:list<list<string>>) -> string",
          "doc": ""
        },
        "columns": {
          "sig": "columns(filas:list<list<string>>) -> int",
          "doc": ""
        }
      }
    },
    "date": {
      "functions": {
        "current_date": {
          "sig": "current_date() -> string",
          "doc": ""
        },
        "current_time": {
          "sig": "current_time() -> string",
          "doc": ""
        },
        "current_datetime": {
          "sig": "current_datetime() -> string",
          "doc": ""
        },
        "epoch_to_date": {
          "sig": "epoch_to_date(segundos:int) -> string",
          "doc": ""
        },
        "date_to_epoch": {
          "sig": "date_to_epoch(fecha:string) -> int",
          "doc": ""
        },
        "add_days": {
          "sig": "add_days(fecha:string, dias:int) -> string",
          "doc": ""
        },
        "days_between": {
          "sig": "days_between(inicio:string, fin:string) -> int",
          "doc": ""
        },
        "is_leap_year": {
          "sig": "is_leap_year(anio:int) -> bool",
          "doc": ""
        },
        "is_valid_date": {
          "sig": "is_valid_date(fecha:string) -> bool",
          "doc": ""
        },
        "day_of_week": {
          "sig": "day_of_week(fecha:string) -> int",
          "doc": ""
        },
        "day_of_week_name": {
          "sig": "day_of_week_name(fecha:string) -> string",
          "doc": ""
        },
        "days_in_month": {
          "sig": "days_in_month(anio:int, mes:int) -> int",
          "doc": ""
        },
        "month_name": {
          "sig": "month_name(mes:int) -> string",
          "doc": ""
        }
      }
    },
    "datetime": {
      "functions": {
        "now": {
          "sig": "now() -> string",
          "doc": ""
        },
        "today": {
          "sig": "today() -> string",
          "doc": ""
        },
        "from_timestamp": {
          "sig": "from_timestamp(segundos:int) -> string",
          "doc": ""
        },
        "timestamp": {
          "sig": "timestamp(fecha:string) -> int",
          "doc": ""
        },
        "shift_date": {
          "sig": "shift_date(fecha:string, dias:int) -> string",
          "doc": ""
        },
        "days_between_dates": {
          "sig": "days_between_dates(inicio:string, fin:string) -> int",
          "doc": ""
        },
        "valid_date": {
          "sig": "valid_date(fecha:string) -> bool",
          "doc": ""
        },
        "leap_year": {
          "sig": "leap_year(anio:int) -> bool",
          "doc": ""
        },
        "weekday": {
          "sig": "weekday(fecha:string) -> int",
          "doc": ""
        }
      }
    },
    "difflib": {
      "functions": {
        "_lcs_len": {
          "sig": "_lcs_len(a:string, b:string) -> int",
          "doc": ""
        },
        "ratio": {
          "sig": "ratio(a:string, b:string) -> float",
          "doc": ""
        },
        "_edit_script": {
          "sig": "_edit_script(a:list<string>, b:list<string>) -> list<list<string>>",
          "doc": ""
        },
        "_normalize_order": {
          "sig": "_normalize_order(script:list<list<string>>) -> list<list<string>>",
          "doc": ""
        },
        "unified_diff": {
          "sig": "unified_diff(a:list<string>, b:list<string>, desde:string, hasta:string) -> list<string>",
          "doc": ""
        },
        "_range": {
          "sig": "_range(inicio:int, cuenta:int) -> string",
          "doc": ""
        },
        "_itoa": {
          "sig": "_itoa(v:int) -> string",
          "doc": ""
        },
        "get_close_matches": {
          "sig": "get_close_matches(palabra:string, posibilidades:list<string>, n:int, corte:float) -> list<string>",
          "doc": ""
        }
      }
    },
    "files": {
      "functions": {
        "file_size": {
          "sig": "file_size(ruta:string) -> int",
          "doc": ""
        },
        "file_exists": {
          "sig": "file_exists(ruta:string) -> bool",
          "doc": ""
        },
        "delete_file": {
          "sig": "delete_file(ruta:string) -> bool",
          "doc": ""
        },
        "create_directory": {
          "sig": "create_directory(ruta:string) -> bool",
          "doc": ""
        },
        "file_rename": {
          "sig": "file_rename(origen:string, destino:string) -> bool",
          "doc": ""
        },
        "read": {
          "sig": "read(ruta:string) -> string",
          "doc": ""
        },
        "write": {
          "sig": "write(ruta:string, contenido:string) -> bool",
          "doc": ""
        },
        "exists": {
          "sig": "exists(ruta:string) -> bool",
          "doc": ""
        },
        "read_lines": {
          "sig": "read_lines(ruta:string) -> list<string>",
          "doc": ""
        },
        "write_lines": {
          "sig": "write_lines(ruta:string, lineas:list<string>) -> bool",
          "doc": ""
        }
      }
    },
    "fnmatch": {
      "functions": {
        "matches": {
          "sig": "matches(nombre:string, patron:string) -> bool",
          "doc": ""
        }
      }
    },
    "html": {
      "functions": {
        "escape": {
          "sig": "escape(texto:string) -> string",
          "doc": ""
        },
        "unescape": {
          "sig": "unescape(texto:string) -> string",
          "doc": ""
        }
      }
    },
    "json": {
      "functions": {
        "encode": {
          "sig": "encode(m:map<string, string>) -> string",
          "doc": ""
        },
        "decode": {
          "sig": "decode(texto:string) -> map<string, string>",
          "doc": ""
        },
        "escape": {
          "sig": "escape(texto:string) -> string",
          "doc": ""
        },
        "unescape": {
          "sig": "unescape(texto:string) -> string",
          "doc": ""
        },
        "_closest": {
          "sig": "_closest(a:int, b:int) -> int",
          "doc": ""
        },
        "_next_escape": {
          "sig": "_next_escape(texto:string) -> int",
          "doc": ""
        },
        "_escape_for": {
          "sig": "_escape_for(letra:string) -> string",
          "doc": ""
        },
        "_unescape_for": {
          "sig": "_unescape_for(escapado:string) -> string",
          "doc": ""
        },
        "_invalid": {
          "sig": "_invalid() -> map<string, string>",
          "doc": ""
        }
      }
    },
    "math": {
      "functions": {
        "square_root": {
          "sig": "square_root(x:float) -> float",
          "doc": ""
        },
        "power": {
          "sig": "power(x:float, exponente:float) -> float",
          "doc": ""
        },
        "sine": {
          "sig": "sine(x:float) -> float",
          "doc": ""
        },
        "cosine": {
          "sig": "cosine(x:float) -> float",
          "doc": ""
        },
        "tangent": {
          "sig": "tangent(x:float) -> float",
          "doc": ""
        },
        "ln": {
          "sig": "ln(x:float) -> float",
          "doc": ""
        },
        "log_10": {
          "sig": "log_10(x:float) -> float",
          "doc": ""
        },
        "exponential": {
          "sig": "exponential(x:float) -> float",
          "doc": ""
        },
        "pi": {
          "sig": "pi() -> float",
          "doc": ""
        },
        "euler": {
          "sig": "euler() -> float",
          "doc": ""
        },
        "round_to_int": {
          "sig": "round_to_int(x:float) -> int",
          "doc": ""
        },
        "floor_to_int": {
          "sig": "floor_to_int(x:float) -> int",
          "doc": ""
        },
        "ceil_to_int": {
          "sig": "ceil_to_int(x:float) -> int",
          "doc": ""
        },
        "is_integer": {
          "sig": "is_integer(x:float) -> bool",
          "doc": ""
        },
        "abs": {
          "sig": "abs(valor:int) -> int",
          "doc": ""
        },
        "abs_float": {
          "sig": "abs_float(valor:float) -> float",
          "doc": ""
        },
        "min": {
          "sig": "min(a:int, b:int) -> int",
          "doc": ""
        },
        "max": {
          "sig": "max(a:int, b:int) -> int",
          "doc": ""
        },
        "clamp": {
          "sig": "clamp(valor:int, limite_inferior:int, limite_superior:int) -> int",
          "doc": ""
        },
        "is_even": {
          "sig": "is_even(valor:int) -> bool",
          "doc": ""
        },
        "is_odd": {
          "sig": "is_odd(valor:int) -> bool",
          "doc": ""
        },
        "sign": {
          "sig": "sign(valor:int) -> int",
          "doc": ""
        },
        "integer_power": {
          "sig": "integer_power(base:int, exponente:int) -> int",
          "doc": ""
        },
        "factorial": {
          "sig": "factorial(n:int) -> int",
          "doc": ""
        },
        "gcd": {
          "sig": "gcd(a:int, b:int) -> int",
          "doc": ""
        },
        "lcm": {
          "sig": "lcm(a:int, b:int) -> int",
          "doc": ""
        },
        "average": {
          "sig": "average(a:float, b:float) -> float",
          "doc": ""
        },
        "celsius_to_fahrenheit": {
          "sig": "celsius_to_fahrenheit(grados:float) -> float",
          "doc": ""
        },
        "fahrenheit_to_celsius": {
          "sig": "fahrenheit_to_celsius(grados:float) -> float",
          "doc": ""
        }
      }
    },
    "mgl": {
      "functions": {
        "mgl_available": {
          "sig": "mgl_available() -> bool",
          "doc": ""
        },
        "mgl_window": {
          "sig": "mgl_window(titulo:string, ancho:int, alto:int) -> int",
          "doc": ""
        },
        "mgl_title": {
          "sig": "mgl_title(ventana:int, titulo:string)",
          "doc": ""
        },
        "mgl_geometry": {
          "sig": "mgl_geometry(ventana:int, ancho:int, alto:int)",
          "doc": ""
        },
        "mgl_resizable": {
          "sig": "mgl_resizable(ventana:int, ancho:int, alto:int)",
          "doc": ""
        },
        "mgl_background": {
          "sig": "mgl_background(ventana:int, r:int, g:int, b:int)",
          "doc": ""
        },
        "mgl_destroy": {
          "sig": "mgl_destroy(ventana:int) -> int",
          "doc": ""
        },
        "mgl_open_windows": {
          "sig": "mgl_open_windows() -> int",
          "doc": ""
        },
        "mgl_label": {
          "sig": "mgl_label(ventana:int, texto:string, x:int, y:int) -> int",
          "doc": ""
        },
        "mgl_button": {
          "sig": "mgl_button(ventana:int, texto:string, x:int, y:int, ancho:int, alto:int) -> int",
          "doc": ""
        },
        "mgl_entry": {
          "sig": "mgl_entry(ventana:int, x:int, y:int, ancho:int, alto:int, largo_max:int) -> int",
          "doc": ""
        },
        "mgl_checkbutton": {
          "sig": "mgl_checkbutton(ventana:int, texto:string, x:int, y:int) -> int",
          "doc": ""
        },
        "mgl_frame": {
          "sig": "mgl_frame(ventana:int, x:int, y:int, ancho:int, alto:int) -> int",
          "doc": ""
        },
        "mgl_config": {
          "sig": "mgl_config(id:int, opcion:string, valor:string) -> int",
          "doc": ""
        },
        "mgl_get": {
          "sig": "mgl_get(id:int) -> string",
          "doc": ""
        },
        "mgl_insert": {
          "sig": "mgl_insert(id:int, texto:string)",
          "doc": ""
        },
        "mgl_delete": {
          "sig": "mgl_delete(id:int, inicio:int, fin:int)",
          "doc": ""
        },
        "mgl_font": {
          "sig": "mgl_font(nombre:string, tamano:int)",
          "doc": ""
        },
        "mgl_update": {
          "sig": "mgl_update() -> int",
          "doc": ""
        },
        "mgl_wait": {
          "sig": "mgl_wait() -> int",
          "doc": ""
        },
        "mgl_event_type": {
          "sig": "mgl_event_type() -> int",
          "doc": ""
        },
        "mgl_event_widget": {
          "sig": "mgl_event_widget() -> int",
          "doc": ""
        },
        "mgl_event_text": {
          "sig": "mgl_event_text() -> string",
          "doc": ""
        },
        "mgl_event_key": {
          "sig": "mgl_event_key() -> int",
          "doc": ""
        },
        "mgl_event_x": {
          "sig": "mgl_event_x() -> int",
          "doc": ""
        },
        "mgl_event_y": {
          "sig": "mgl_event_y() -> int",
          "doc": ""
        },
        "available": {
          "sig": "available() -> bool",
          "doc": ""
        },
        "window": {
          "sig": "window(titulo:string, ancho:int, alto:int) -> int",
          "doc": ""
        },
        "title": {
          "sig": "title(ventana:int, titulo:string)",
          "doc": ""
        },
        "geometry": {
          "sig": "geometry(ventana:int, ancho:int, alto:int)",
          "doc": ""
        },
        "resizable": {
          "sig": "resizable(ventana:int, ancho:bool, alto:bool)",
          "doc": ""
        },
        "background": {
          "sig": "background(ventana:int, r:int, g:int, b:int)",
          "doc": ""
        },
        "destroy": {
          "sig": "destroy(ventana:int) -> bool",
          "doc": ""
        },
        "windows_open": {
          "sig": "windows_open() -> int",
          "doc": ""
        },
        "label": {
          "sig": "label(ventana:int, texto:string, x:int, y:int) -> int",
          "doc": ""
        },
        "button": {
          "sig": "button(ventana:int, texto:string, x:int, y:int, ancho:int, alto:int) -> int",
          "doc": ""
        },
        "entry": {
          "sig": "entry(ventana:int, x:int, y:int, ancho:int, alto:int, largo_max:int) -> int",
          "doc": ""
        },
        "checkbutton": {
          "sig": "checkbutton(ventana:int, texto:string, x:int, y:int) -> int",
          "doc": ""
        },
        "frame": {
          "sig": "frame(ventana:int, x:int, y:int, ancho:int, alto:int) -> int",
          "doc": ""
        },
        "config": {
          "sig": "config(id:int, opcion:string, valor:string) -> bool",
          "doc": ""
        },
        "get": {
          "sig": "get(id:int) -> string",
          "doc": ""
        },
        "insert": {
          "sig": "insert(id:int, texto:string)",
          "doc": ""
        },
        "delete": {
          "sig": "delete(id:int, inicio:int, fin:int)",
          "doc": ""
        },
        "font": {
          "sig": "font(nombre:string, tamano:int)",
          "doc": ""
        },
        "update": {
          "sig": "update() -> bool",
          "doc": ""
        },
        "wait": {
          "sig": "wait() -> bool",
          "doc": ""
        },
        "event_type": {
          "sig": "event_type() -> int",
          "doc": ""
        },
        "event_widget": {
          "sig": "event_widget() -> int",
          "doc": ""
        },
        "event_text": {
          "sig": "event_text() -> string",
          "doc": ""
        },
        "event_key": {
          "sig": "event_key() -> int",
          "doc": ""
        },
        "event_x": {
          "sig": "event_x() -> int",
          "doc": ""
        },
        "event_y": {
          "sig": "event_y() -> int",
          "doc": ""
        },
        "event_none": {
          "sig": "event_none() -> int",
          "doc": ""
        },
        "event_click": {
          "sig": "event_click() -> int",
          "doc": ""
        },
        "event_keypress": {
          "sig": "event_keypress() -> int",
          "doc": ""
        },
        "event_return": {
          "sig": "event_return() -> int",
          "doc": ""
        },
        "event_check": {
          "sig": "event_check() -> int",
          "doc": ""
        },
        "event_close": {
          "sig": "event_close() -> int",
          "doc": ""
        }
      }
    },
    "os": {
      "functions": {
        "working_directory": {
          "sig": "working_directory() -> string",
          "doc": ""
        },
        "process_id": {
          "sig": "process_id() -> int",
          "doc": ""
        },
        "environment_value": {
          "sig": "environment_value(nombre:string) -> string",
          "doc": ""
        },
        "environment_is_set": {
          "sig": "environment_is_set(nombre:string) -> bool",
          "doc": ""
        },
        "update_environment": {
          "sig": "update_environment(nombre:string, valor:string) -> bool",
          "doc": ""
        },
        "path_exists": {
          "sig": "path_exists(ruta:string) -> bool",
          "doc": ""
        },
        "path_size": {
          "sig": "path_size(ruta:string) -> int",
          "doc": ""
        },
        "delete_path": {
          "sig": "delete_path(ruta:string) -> bool",
          "doc": ""
        },
        "rename_path": {
          "sig": "rename_path(origen:string, destino:string) -> bool",
          "doc": ""
        },
        "make_directory": {
          "sig": "make_directory(ruta:string) -> bool",
          "doc": ""
        }
      }
    },
    "pathlib": {
      "functions": {
        "Path": {
          "sig": "Path(ruta:string) -> string",
          "doc": ""
        },
        "join_path": {
          "sig": "join_path(first:string, second:string) -> string",
          "doc": ""
        },
        "file_name": {
          "sig": "file_name(ruta:string) -> string",
          "doc": ""
        },
        "parent_directory": {
          "sig": "parent_directory(ruta:string) -> string",
          "doc": ""
        },
        "extension_of": {
          "sig": "extension_of(ruta:string) -> string",
          "doc": ""
        },
        "file_stem": {
          "sig": "file_stem(ruta:string) -> string",
          "doc": ""
        },
        "extensions_of": {
          "sig": "extensions_of(ruta:string) -> list<string>",
          "doc": ""
        },
        "path_is_absolute": {
          "sig": "path_is_absolute(ruta:string) -> bool",
          "doc": ""
        },
        "path_parts": {
          "sig": "path_parts(ruta:string) -> list<string>",
          "doc": ""
        }
      }
    },
    "paths": {
      "functions": {
        "_last_index": {
          "sig": "_last_index(t:string, sub:string) -> int",
          "doc": ""
        },
        "_without_last": {
          "sig": "_without_last(lista:list<string>) -> list<string>",
          "doc": ""
        },
        "join": {
          "sig": "join(primera:string, segunda:string) -> string",
          "doc": ""
        },
        "name": {
          "sig": "name(ruta:string) -> string",
          "doc": ""
        },
        "directory": {
          "sig": "directory(ruta:string) -> string",
          "doc": ""
        },
        "extension": {
          "sig": "extension(ruta:string) -> string",
          "doc": ""
        },
        "without_extension": {
          "sig": "without_extension(ruta:string) -> string",
          "doc": ""
        },
        "name_without_extension": {
          "sig": "name_without_extension(ruta:string) -> string",
          "doc": ""
        },
        "is_absolute": {
          "sig": "is_absolute(ruta:string) -> bool",
          "doc": ""
        },
        "components": {
          "sig": "components(ruta:string) -> list<string>",
          "doc": ""
        },
        "normalize": {
          "sig": "normalize(ruta:string) -> string",
          "doc": ""
        }
      }
    },
    "platform": {
      "functions": {
        "system_name": {
          "sig": "system_name() -> string",
          "doc": ""
        },
        "node": {
          "sig": "node() -> string",
          "doc": ""
        },
        "machine": {
          "sig": "machine() -> string",
          "doc": ""
        },
        "processor_count": {
          "sig": "processor_count() -> int",
          "doc": ""
        },
        "user_name": {
          "sig": "user_name() -> string",
          "doc": ""
        }
      }
    },
    "random": {
      "functions": {
        "set_seed": {
          "sig": "set_seed(valor:int)",
          "doc": ""
        },
        "integer": {
          "sig": "integer(minimo:int, maximo:int) -> int",
          "doc": ""
        },
        "uniform": {
          "sig": "uniform() -> float",
          "doc": ""
        },
        "chance": {
          "sig": "chance(p:float) -> bool",
          "doc": ""
        }
      }
    },
    "re": {
      "functions": {
        "_dig": {
          "sig": "_dig() -> string",
          "doc": ""
        },
        "_wor": {
          "sig": "_wor() -> string",
          "doc": ""
        },
        "_spa": {
          "sig": "_spa() -> string",
          "doc": ""
        },
        "_atoi": {
          "sig": "_atoi(t:string) -> int",
          "doc": ""
        },
        "_itoa": {
          "sig": "_itoa(v:int) -> string",
          "doc": ""
        },
        "_word_char": {
          "sig": "_word_char(c:string) -> bool",
          "doc": ""
        },
        "_peek": {
          "sig": "_peek(s:string, st:list<int>) -> string",
          "doc": ""
        },
        "_adv": {
          "sig": "_adv(st:list<int>)",
          "doc": ""
        },
        "_pint": {
          "sig": "_pint(s:string, st:list<int>) -> int",
          "doc": ""
        },
        "_nuevo": {
          "sig": "_nuevo(nodos:list<list<string>>, campos:list<string>) -> int",
          "doc": ""
        },
        "_parse_class": {
          "sig": "_parse_class(s:string, st:list<int>, nodos:list<list<string>>) -> int",
          "doc": ""
        },
        "_parse_escape": {
          "sig": "_parse_escape(s:string, st:list<int>, nodos:list<list<string>>) -> int",
          "doc": ""
        },
        "_parse_atom": {
          "sig": "_parse_atom(s:string, st:list<int>, nodos:list<list<string>>) -> int",
          "doc": ""
        },
        "_parse_term": {
          "sig": "_parse_term(s:string, st:list<int>, nodos:list<list<string>>) -> int",
          "doc": ""
        },
        "_parse_seq": {
          "sig": "_parse_seq(s:string, st:list<int>, nodos:list<list<string>>) -> int",
          "doc": ""
        },
        "_parse_regex": {
          "sig": "_parse_regex(s:string, st:list<int>, nodos:list<list<string>>) -> int",
          "doc": ""
        },
        "_compile": {
          "sig": "_compile(pattern:string) -> list<list<string>>",
          "doc": ""
        },
        "_boundary": {
          "sig": "_boundary(text:string, ti:int) -> bool",
          "doc": ""
        },
        "_mnode": {
          "sig": "_mnode(nodos:list<list<string>>, idx:int, text:string, ti:int, caps:list<string>) -> int",
          "doc": ""
        },
        "_mseq": {
          "sig": "_mseq(nodos:list<list<string>>, idx:int, text:string, ti:int, caps:list<string>) -> int",
          "doc": ""
        },
        "_ml": {
          "sig": "_ml(nodos:list<list<string>>, idxs:list<int>, ii:int, text:string, ti:int, caps:list<string>) -> int",
          "doc": ""
        },
        "_run": {
          "sig": "_run(pattern:string, text:string, desde:int, anclado:bool) -> list<string>",
          "doc": ""
        },
        "match": {
          "sig": "match(pattern:string, text:string) -> list<string>",
          "doc": ""
        },
        "search": {
          "sig": "search(pattern:string, text:string) -> list<string>",
          "doc": ""
        },
        "test": {
          "sig": "test(pattern:string, text:string) -> bool",
          "doc": ""
        },
        "findall": {
          "sig": "findall(pattern:string, text:string) -> list<string>",
          "doc": ""
        },
        "_expand": {
          "sig": "_expand(rep:string, caps:list<string>) -> string",
          "doc": ""
        },
        "sub": {
          "sig": "sub(pattern:string, rep:string, text:string) -> string",
          "doc": ""
        },
        "split": {
          "sig": "split(pattern:string, text:string) -> list<string>",
          "doc": ""
        }
      }
    },
    "shutil": {
      "functions": {
        "copy_text": {
          "sig": "copy_text(source:string, destination:string) -> bool",
          "doc": ""
        },
        "move_path": {
          "sig": "move_path(source:string, destination:string) -> bool",
          "doc": ""
        },
        "delete_path": {
          "sig": "delete_path(path:string) -> bool",
          "doc": ""
        },
        "make_directory": {
          "sig": "make_directory(path:string) -> bool",
          "doc": ""
        },
        "path_size": {
          "sig": "path_size(path:string) -> int",
          "doc": ""
        }
      }
    },
    "statistics": {
      "functions": {
        "_sort": {
          "sig": "_sort(valores:list<float>) -> list<float>",
          "doc": ""
        },
        "sum": {
          "sig": "sum(valores:list<float>) -> float",
          "doc": ""
        },
        "product": {
          "sig": "product(valores:list<float>) -> float",
          "doc": ""
        },
        "mean": {
          "sig": "mean(valores:list<float>) -> float",
          "doc": ""
        },
        "min_list": {
          "sig": "min_list(valores:list<float>) -> float",
          "doc": ""
        },
        "max_list": {
          "sig": "max_list(valores:list<float>) -> float",
          "doc": ""
        },
        "range": {
          "sig": "range(valores:list<float>) -> float",
          "doc": ""
        },
        "median": {
          "sig": "median(valores:list<float>) -> float",
          "doc": ""
        },
        "mode": {
          "sig": "mode(valores:list<float>) -> float",
          "doc": ""
        },
        "variance": {
          "sig": "variance(valores:list<float>) -> float",
          "doc": ""
        },
        "standard_deviation": {
          "sig": "standard_deviation(valores:list<float>) -> float",
          "doc": ""
        },
        "percentile": {
          "sig": "percentile(valores:list<float>, p:float) -> float",
          "doc": ""
        }
      }
    },
    "string": {
      "functions": {
        "digits": {
          "sig": "digits() -> string",
          "doc": ""
        },
        "ascii_lowercase": {
          "sig": "ascii_lowercase() -> string",
          "doc": ""
        },
        "ascii_uppercase": {
          "sig": "ascii_uppercase() -> string",
          "doc": ""
        },
        "ascii_letters": {
          "sig": "ascii_letters() -> string",
          "doc": ""
        },
        "hexdigits": {
          "sig": "hexdigits() -> string",
          "doc": ""
        },
        "punctuation": {
          "sig": "punctuation() -> string",
          "doc": ""
        },
        "whitespace": {
          "sig": "whitespace() -> string",
          "doc": ""
        },
        "title_words": {
          "sig": "title_words(texto:string) -> string",
          "doc": ""
        },
        "swap_case": {
          "sig": "swap_case(texto:string) -> string",
          "doc": ""
        }
      }
    },
    "strings": {
      "functions": {
        "digits": {
          "sig": "digits() -> string",
          "doc": ""
        },
        "ascii_lowercase": {
          "sig": "ascii_lowercase() -> string",
          "doc": ""
        },
        "ascii_uppercase": {
          "sig": "ascii_uppercase() -> string",
          "doc": ""
        },
        "ascii_letters": {
          "sig": "ascii_letters() -> string",
          "doc": ""
        },
        "hex_digits": {
          "sig": "hex_digits() -> string",
          "doc": ""
        },
        "punctuation": {
          "sig": "punctuation() -> string",
          "doc": ""
        },
        "whitespace": {
          "sig": "whitespace() -> string",
          "doc": ""
        },
        "lstrip": {
          "sig": "lstrip(t:string) -> string",
          "doc": ""
        },
        "rstrip": {
          "sig": "rstrip(t:string) -> string",
          "doc": ""
        },
        "swapcase": {
          "sig": "swapcase(t:string) -> string",
          "doc": ""
        },
        "capwords": {
          "sig": "capwords(t:string) -> string",
          "doc": ""
        }
      }
    },
    "system": {
      "functions": {
        "pid": {
          "sig": "pid() -> int",
          "doc": ""
        },
        "current_directory": {
          "sig": "current_directory() -> string",
          "doc": ""
        },
        "hostname": {
          "sig": "hostname() -> string",
          "doc": ""
        },
        "os": {
          "sig": "os() -> string",
          "doc": ""
        },
        "architecture": {
          "sig": "architecture() -> string",
          "doc": ""
        },
        "get_env": {
          "sig": "get_env(nombre:string) -> string",
          "doc": ""
        },
        "has_env": {
          "sig": "has_env(nombre:string) -> bool",
          "doc": ""
        },
        "set_env": {
          "sig": "set_env(nombre:string, valor:string) -> bool",
          "doc": ""
        },
        "username": {
          "sig": "username() -> string",
          "doc": ""
        },
        "cpu_count": {
          "sig": "cpu_count() -> int",
          "doc": ""
        },
        "total_memory_mb": {
          "sig": "total_memory_mb() -> int",
          "doc": ""
        },
        "available_memory_mb": {
          "sig": "available_memory_mb() -> int",
          "doc": ""
        },
        "uptime": {
          "sig": "uptime() -> int",
          "doc": ""
        }
      }
    },
    "text": {
      "functions": {
        "to_upper": {
          "sig": "to_upper(t:string) -> string",
          "doc": ""
        },
        "to_lower": {
          "sig": "to_lower(t:string) -> string",
          "doc": ""
        },
        "strip": {
          "sig": "strip(t:string) -> string",
          "doc": ""
        },
        "parts": {
          "sig": "parts(t:string, separador:string) -> list<string>",
          "doc": ""
        },
        "combine": {
          "sig": "combine(lista:list<string>, separador:string) -> string",
          "doc": ""
        },
        "substitute": {
          "sig": "substitute(t:string, viejo:string, nuevo:string) -> string",
          "doc": ""
        },
        "includes": {
          "sig": "includes(t:string, fragmento:string) -> bool",
          "doc": ""
        },
        "has_prefix": {
          "sig": "has_prefix(t:string, prefijo:string) -> bool",
          "doc": ""
        },
        "has_suffix": {
          "sig": "has_suffix(t:string, sufijo:string) -> bool",
          "doc": ""
        },
        "index_of": {
          "sig": "index_of(t:string, fragmento:string) -> int",
          "doc": ""
        },
        "slice": {
          "sig": "slice(t:string, inicio:int, cantidad:int) -> string",
          "doc": ""
        },
        "is_empty": {
          "sig": "is_empty(t:string) -> bool",
          "doc": ""
        },
        "repeat": {
          "sig": "repeat(t:string, veces:int) -> string",
          "doc": ""
        },
        "count": {
          "sig": "count(t:string, fragmento:string) -> int",
          "doc": ""
        },
        "capitalize": {
          "sig": "capitalize(t:string) -> string",
          "doc": ""
        },
        "reverse": {
          "sig": "reverse(t:string) -> string",
          "doc": ""
        },
        "collapse_spaces": {
          "sig": "collapse_spaces(t:string) -> string",
          "doc": ""
        }
      }
    },
    "textwrap": {
      "functions": {
        "_leading_spaces": {
          "sig": "_leading_spaces(linea:string) -> int",
          "doc": ""
        },
        "_normalize": {
          "sig": "_normalize(t:string) -> string",
          "doc": ""
        },
        "wrap": {
          "sig": "wrap(t:string, ancho:int) -> list<string>",
          "doc": ""
        },
        "fill": {
          "sig": "fill(t:string, ancho:int) -> string",
          "doc": ""
        },
        "indent": {
          "sig": "indent(t:string, prefijo:string) -> string",
          "doc": ""
        },
        "dedent": {
          "sig": "dedent(t:string) -> string",
          "doc": ""
        },
        "_last_space": {
          "sig": "_last_space(t:string) -> int",
          "doc": ""
        },
        "shorten": {
          "sig": "shorten(t:string, ancho:int, placeholder:string) -> string",
          "doc": ""
        }
      }
    },
    "time": {
      "functions": {
        "now_ms": {
          "sig": "now_ms() -> int",
          "doc": ""
        },
        "unix_ms": {
          "sig": "unix_ms() -> int",
          "doc": ""
        },
        "ms_to_seconds": {
          "sig": "ms_to_seconds(milisegundos:int) -> int",
          "doc": ""
        },
        "sleep": {
          "sig": "sleep(milisegundos:int)",
          "doc": ""
        },
        "sleep_seconds": {
          "sig": "sleep_seconds(segundos:int)",
          "doc": ""
        }
      }
    },
    "validation": {
      "functions": {
        "_digit": {
          "sig": "_digit(c:string) -> bool",
          "doc": ""
        },
        "_letter": {
          "sig": "_letter(c:string) -> bool",
          "doc": ""
        },
        "not_empty": {
          "sig": "not_empty(t:string) -> bool",
          "doc": ""
        },
        "min_length": {
          "sig": "min_length(t:string, minimo:int) -> bool",
          "doc": ""
        },
        "max_length": {
          "sig": "max_length(t:string, maximo:int) -> bool",
          "doc": ""
        },
        "in_range": {
          "sig": "in_range(valor:int, limite_inferior:int, limite_superior:int) -> bool",
          "doc": ""
        },
        "numeric": {
          "sig": "numeric(t:string) -> bool",
          "doc": ""
        },
        "is_float": {
          "sig": "is_float(t:string) -> bool",
          "doc": ""
        },
        "letters": {
          "sig": "letters(t:string) -> bool",
          "doc": ""
        },
        "alphanumeric": {
          "sig": "alphanumeric(t:string) -> bool",
          "doc": ""
        },
        "is_email": {
          "sig": "is_email(t:string) -> bool",
          "doc": ""
        }
      }
    }
  }
};
