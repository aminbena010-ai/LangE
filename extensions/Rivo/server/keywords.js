'use strict';

/*
 * Rivo — documentación de palabras reservadas, tipos y operadores.
 * Usado por hover y autocompletado del language server.
 */

const KEYWORDS = [
  // declaración
  { name: 'fn', group: 'Declaración',
    doc: 'Declara una función. El bloque va indentado tras `:` y el tipo de retorno es opcional.',
    example: 'fn suma(a:int, b:int) -> int:\n    return a + b' },
  { name: 'struct', group: 'Declaración',
    doc: 'Declara un tipo de datos con campos tipados. Su constructor recibe los valores en el orden declarado.',
    example: 'struct Punto:\n    var x:float\n    var y:float\n\nvar p:Punto = Punto(1.0, 2.0)' },
  { name: 'var', group: 'Declaración',
    doc: 'Declara una variable mutable con tipo explícito. Solo dentro de funciones (no hay globales).',
    example: 'var contador:int = 0' },
  { name: 'const', group: 'Declaración',
    doc: 'Declara una constante no reasignable con tipo explícito.',
    example: 'const PI_APROX:int = 3' },
  { name: 'return', group: 'Declaración',
    doc: 'Devuelve un valor desde una función. El tipo debe coincidir con el declarado en la firma.',
    example: 'return a + b' },
  { name: 'external', group: 'Declaración',
    doc: 'Declara una función externa implementada en C (FFI). Solo tipos escalares, sin cuerpo y a nivel global.',
    example: 'external "lib/c/math.c" fn sqrt_c(x:float) -> float' },

  // concurrencia
  { name: 'async', group: 'Concurrencia',
    doc: 'Prefijo obligatorio de `fn` para funciones asíncronas, que devuelven `task<T>`. No puede usarse en `main` ni con `external`.',
    example: 'async fn tarea() -> task<int>:\n    return 42' },
  { name: 'await', group: 'Concurrencia',
    doc: 'Espera el resultado de una `task<T>` y produce su valor `T`. Puede usarse en cualquier función.',
    example: 'var v:int = await tarea()' },

  // control de flujo
  { name: 'if', group: 'Control de flujo',
    doc: 'Condición con bloque por indentación terminado en `:`.',
    example: 'if edad >= 18:\n    print("mayor")' },
  { name: 'else', group: 'Control de flujo',
    doc: 'Rama alternativa de `if` o `elif`.',
    example: 'else:\n    print("menor")' },
  { name: 'elif', group: 'Control de flujo',
    doc: 'Rama condicional alternativa de un `if`; puede encadenarse antes de `else`.',
    example: 'if nota >= 90:\n    print("A")\nelif nota >= 80:\n    print("B")' },
  { name: 'while', group: 'Control de flujo',
    doc: 'Bucle condicional: se repite mientras la condición sea verdadera.',
    example: 'while contador < 10:\n    contador += 1' },
  { name: 'for', group: 'Control de flujo',
    doc: 'Bucle de iteración sobre un rango o colección: `for variable in iterable:`.',
    example: 'for i in range(5):\n    print(i)' },
  { name: 'in', group: 'Control de flujo',
    doc: 'Separa la variable del iterable dentro de un `for`.',
    example: 'for c in "hola":\n    print(c)' },
  { name: 'break', group: 'Control de flujo',
    doc: 'Sale del bucle más interno (solo dentro de bucles).',
    example: 'while true:\n    break' },
  { name: 'continue', group: 'Control de flujo',
    doc: 'Pasa a la siguiente iteración del bucle actual.',
    example: 'for i in range(10):\n    if i % 2 == 0:\n        continue' },

  // bloques y errores
  { name: 'pass', group: 'Bloques',
    doc: 'Marcador de lugar sin operación, para bloques aún vacíos.',
    example: 'fn pendiente():\n    pass' },
  { name: 'try', group: 'Errores',
    doc: 'Bloque protegido frente a errores en tiempo de ejecución. Exige exactamente un `catch` y no permite `return`/`break`/`continue` dentro.',
    example: 'try:\n    var v:int = to_int("x")\ncatch e:\n    print(e)' },
  { name: 'catch', group: 'Errores',
    doc: 'Captura el error como `string` con su mensaje (clases como SyntaxError, TypeError, NameError...). No existe `throw`.',
    example: 'catch error:\n    print(error)' },

  // módulos
  { name: 'import', group: 'Módulos',
    doc: 'Importa un módulo completo; se usa calificado: `math.square_root(4.0)`.',
    example: 'import math' },
  { name: 'from', group: 'Módulos',
    doc: 'Importación selectiva de nombres concretos de un módulo.',
    example: 'from math import abs' },
  { name: 'as', group: 'Módulos',
    doc: 'Asigna un alias a un módulo importado.',
    example: 'import math as m' },

  // literales
  { name: 'true', group: 'Literales',
    doc: 'Literal booleano verdadero.',
    example: 'var ok:bool = true' },
  { name: 'false', group: 'Literales',
    doc: 'Literal booleano falso.',
    example: 'var ok:bool = false' },
  { name: 'null', group: 'Literales',
    doc: 'Valor/tipo especial nulo.' },

  // lógica
  { name: 'and', group: 'Lógica',
    doc: 'Y lógico con evaluación cortocircuitada.',
    example: 'if a > 0 and b > 0:\n    pass' },
  { name: 'or', group: 'Lógica',
    doc: 'O lógico con evaluación cortocircuitada.',
    example: 'if a == 0 or b == 0:\n    pass' },
  { name: 'not', group: 'Lógica',
    doc: 'Negación lógica (unario).',
    example: 'if not fin:\n    pass' }
];

const TYPES = [
  { name: 'int', doc: 'Entero con signo de 64 bits (i64).', example: 'var n:int = 42' },
  { name: 'float', doc: 'Número de punto flotante de doble precisión (IEEE 754).', example: 'var x:float = 2.5' },
  { name: 'string', doc: 'Cadena de texto gestionada por la arena del runtime.', example: 'var s:string = "hola"' },
  { name: 'bool', doc: 'Booleano: `true` o `false` (i1).', example: 'var ok:bool = true' },
  { name: 'char', doc: 'Carácter de un byte (i8).', example: 'var c:char = \'a\'' },
  { name: 'list', doc: 'Lista dinámica genérica `list<T>`; los índices empiezan en 0 y `+` concatena.', example: 'var xs:list<int> = [1, 2, 3]' },
  { name: 'map', doc: 'Mapa genérico clave-valor `map<K, V>`; literales con `[...]` o `{...}`.', example: 'var m:map<string, int> = {"a": 1}' },
  { name: 'task', doc: 'Tarea asíncrona `task<T>` producida por una función `async`; se consume con `await`.', example: 'var t:task<int> = tarea()' }
];

const OPERATORS = [
  { sym: '//', doc: '**División entera.** Trunca el resultado hacia cero.', example: '7 // 2   # 3' },
  { sym: '%', doc: '**Módulo.** Resto de la división entera.', example: '7 % 2    # 1' },
  { sym: '++', doc: '**Incremento.** Postfijo (`x++`) o como asignación compuesta.', example: 'contador += 1\ncontador++' },
  { sym: '--', doc: '**Decremento.** Postfijo (`x--`) o como asignación compuesta.', example: 'contador -= 1\ncontador--' },
  { sym: '->', doc: '**Tipo de retorno** en la firma de una función.', example: 'fn doble(n:int) -> int:\n    return n * 2' },
  { sym: '+=', doc: 'Suma y asigna.', example: 'total += 1' },
  { sym: '-=', doc: 'Resta y asigna.', example: 'total -= 1' },
  { sym: '*=', doc: 'Multiplica y asigna.', example: 'total *= 2' },
  { sym: '/=', doc: 'Divide y asigna.', example: 'total /= 2' },
  { sym: '==', doc: 'Igualdad.', example: 'if a == b:\n    pass' },
  { sym: '!=', doc: 'Desigualdad.', example: 'if a != b:\n    pass' },
  { sym: '>=', doc: 'Mayor o igual.', example: 'if a >= b:\n    pass' },
  { sym: '<=', doc: 'Menor o igual.', example: 'if a <= b:\n    pass' },
  { sym: '>', doc: 'Mayor que.', example: 'if a > b:\n    pass' },
  { sym: '<', doc: 'Menor que.', example: 'if a < b:\n    pass' },
  { sym: '+', doc: 'Suma de enteros/flotantes o concatenación de listas y strings.', example: '"a" + "b"' },
  { sym: '-', doc: 'Resta o negación unaria.', example: 'var n:int = -5' },
  { sym: '*', doc: 'Multiplicación.', example: 'var n:int = 3 * 4' },
  { sym: '/', doc: 'División (usa `//` para división entera).', example: 'var x:float = 7.0 / 2.0' },
  { sym: ':', doc: 'Abre un bloque indentado (`if cond:`) o separa nombre y tipo (`x:int`).', example: 'fn main():\n    pass' }
];

const KEYWORD_DOCS = {};
for (const k of KEYWORDS) KEYWORD_DOCS[k.name] = k;

const TYPE_DOCS = {};
for (const t of TYPES) TYPE_DOCS[t.name] = t;

const OPERATOR_DOCS = {};
for (const o of OPERATORS) OPERATOR_DOCS[o.sym] = o;

module.exports = { KEYWORDS, TYPES, OPERATORS, KEYWORD_DOCS, TYPE_DOCS, OPERATOR_DOCS };
