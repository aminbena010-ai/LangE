# Concurrencia: `async`, `await` y `task<T>`

Rivo permite declarar funciones asíncronas y esperar su resultado. Desde
la versión 0.8 el modelo es **cooperativo y determinista**: cada tarea corre
en una fibra y un planificador FIFO de una sola hebra decide cuál avanza.

## Declarar una función async

```rivo
async fn doble(n:int) -> int:
    return n * 2
```

Una `async fn` se comporta como una función normal por dentro, pero **su
llamada no devuelve `int`: devuelve `task<int>`**.

```rivo
fn main():
    var t:task<int> = doble(21)
    print(await t)          # 42
```

Sin tipo de retorno, la tarea es `task<null>`:

```rivo
async fn saluda():
    print("hola")

fn main():
    await saluda()
```

## `await`

`await` solo acepta un `task<T>` y produce un valor de tipo `T`:

```rivo
var t:task<string> = nombre()
var s:string = await t
```

Se puede usar dentro de una expresión:

```rivo
print(await doble(3) + 100)   # 106
```

También se permite en `main` y en funciones síncronas. Cada `await` es un
punto de intercalado: la tarea actual cede el control al planificador y se
reanuda cuando la tarea esperada termina.

## Anidar tareas

```rivo
async fn doble(n:int) -> int:
    return n * 2

async fn doble_mas_uno(n:int) -> int:
    var t:task<int> = doble(n)
    return await t + 1
```

`task<T>` puede contener cualquier tipo oficial: `int`, `float`, `bool`,
`char`, `string`, `list<T>` y `map<K, V>`.

## Reglas

- `async fn main()` es un error semántico: `main` siempre es síncrono.
- `await` sobre algo que no es `task<T>` es un error semántico.
- Una tarea puede guardarse en una variable, pasarse como parámetro o no
  esperarse nunca (fire-and-forget).
- `task<T>` no admite `null`.

## Modelo de ejecución desde 0.8

- Llamar a una `async fn` solo crea la tarea y la encola; el cuerpo no se
  ejecuta todavía.
- Cada tarea tiene su propia pila (fibra `ucontext` en POSIX) y avanza hasta
  el siguiente `await`, que la suspende y cede el turno al planificador.
- El planificador es FIFO, cooperativo y de una sola hebra: no hay carreras
  de datos, pero una operación bloqueante detiene a todas las tareas.
- Al terminar `main` se drenan las tareas que nadie esperó, en orden de
  encolado, antes de salir del programa.
- Cada fibra conserva su propia pila de `try`/`catch`: un error tras un
  `await` salta al `catch` de esa tarea, no al de otra. Un error no capturado
  dentro de una tarea termina el programa; no se propaga al `await`.
- Las tareas viven en el heap del runtime; sus pilas y argumentos se liberan
  al terminar el programa.

En Windows no hay `ucontext`: se conserva la ejecución ansiosa como
respaldo, de modo que las tareas se completan al llamarlas. Esta es una
limitación conocida y documentada en la
[matriz de soporte](../development/support.md).

Hilos para operaciones bloqueantes y un planificador con varias hebras
quedan para una versión posterior.
