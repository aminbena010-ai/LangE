# Gestor de paquetes

Rivo incluye un gestor de dependencias integrado en el binario `rivo`. Hay dos
vías de resolución:

- **local** (v0.5): rutas y versiones servidas desde `.rivo/deps` o `vendor`;
- **registro Git estático** (v0.6): un repositorio Git con un índice de
  paquetes; cada paquete vive en su propio repositorio, fijado por tag.

No hay servidor propio ni descargas HTTP directas: todo pasa por Git, que ya
aporta transporte y verificación de integridad.

## Flujo rápido

```sh
cd mi-app
rivo install            # resuelve y escribe rivo.lock
rivo tree               # muestra el grafo
rivo main.ro --run      # compila usando las rutas del lock
```

## Declarar dependencias

En `rivo.toml`:

```toml
[dependencias]
# Ruta local
utils = "../libs/utils"

# Version (se busca en .rivo/deps, vendor o el registro)
jsonlib = "^1.2.0"
mgl = { version = ">=0.3, <0.4" }

# Repositorio Git con tag fijo
saludo = { git = "https://github.com/usuario/saludo", tag = "v1.0.0" }
```

Formatos de versión admitidos: `1.2.3` (equivale a `^1.2.3`), `^`, `~`, `=`,
`>=`, `>`, `<=`, `<`, `*` y combinaciones separadas por comas. Las
dependencias Git exigen `tag` para que la compilación sea reproducible.

## Comandos

| Comando | Efecto |
| --- | --- |
| `rivo install` | Resuelve dependencias y escribe `rivo.lock` |
| `rivo update` | Igual que `install`, actualizando antes el índice del registro |
| `rivo add nombre@version` | Añade una dependencia de versión |
| `rivo add ruta/al/paquete` | Añade una dependencia de ruta |
| `rivo add <url-git>#<tag>` | Clona el repo, lee su `rivo.toml` y añade la dependencia Git |
| `rivo remove nombre` | Elimina la dependencia de `rivo.toml` |
| `rivo tree` | Muestra el árbol completo de dependencias |
| `rivo list` | Lista las entradas del lock (`nombre version origen`) |
| `rivo search <texto>` | Busca por nombre o descripción en el índice |
| `rivo info <paquete>` | Lista las versiones publicadas y su origen |
| `rivo publish <tag>` | Publica la versión actual en el índice del registro |

## Almacen local

Un paquete de versión ya instalado se reutiliza desde:

```text
mi-app/
├── .rivo/deps/<nombre>/rivo.toml
└── vendor/<nombre>/rivo.toml
```

El paquete debe tener un `rivo.toml` con `nombre` y `version`. `vendor/`
permite fijar una copia en el repositorio del proyecto; `.rivo/deps/` es la
zona gestionada por `rivo`.

Las dependencias Git y las descargadas del registro se clonan en
`.rivo/deps/<nombre>`, también dentro del proyecto.

## Índice estático (v0.6)

Configura el registro en `rivo.toml`:

```toml
[registro]
url = "https://github.com/usuario/rivo-packages"
```

El repositorio del índice contiene un archivo por paquete y versión:

```text
paquetes/
└── saludo/
    └── 0.1.0.toml
```

```toml
# paquetes/saludo/0.1.0.toml
[paquete]
nombre = "saludo"
version = "0.1.0"
git = "https://github.com/usuario/saludo"
tag = "v0.1.0"
descripcion = "Saludos para programas Rivo"
```

`rivo install` clona el índice en `.rivo/registro` (la primera vez) y busca
allí las versiones que cumplan la restricción. `rivo update` ejecuta
`git pull --ff-only` sobre esa copia antes de resolver. Si un paquete ya está
en `.rivo/deps`, no se vuelve a clonar.

## Publicar

```sh
cd mi-paquete
git tag v1.0.0
git remote add origin https://github.com/usuario/saludo   # si falta
rivo publish v1.0.0
```

`rivo publish` comprueba que el tag exista, lee `remote.origin.url`, crea
`paquetes/<nombre>/<version>.toml` en una copia del índice, hace commit y
push. Requiere permisos de escritura sobre el repositorio del registro y que
`[registro]` esté en el `rivo.toml` del paquete.

## `rivo.lock`

```toml
version = 1

[[paquete]]
nombre = "saludo"
version = "0.1.0"
origen = "ruta:./.rivo/deps/saludo"
deps = []
```

- Se genera con `rivo install` y se regenera con `rivo update`.
- Fija la versión y el origen exactos.
- Las entradas se ordenan por nombre para que el archivo sea estable.
- El compilador añade al include path todos los orígenes `ruta:` del lock.

El lock no guarda la URL Git: la fuente de verdad sigue siendo `rivo.toml`.
La reproducibilidad del contenido la garantiza el tag fijado en el índice o
en la dependencia Git.

## Resolución transitiva

`rivo install` recorre las dependencias de cada paquete. Las rutas se
resuelven respecto al manifiesto que las declara; las versiones se buscan en
el almacén del proyecto y después en el registro. Los ciclos se cortan al
encontrar un paquete ya resuelto y la profundidad máxima es 32.

## Pendiente

- `.rpkg` y verificación SHA-256/GPG (registro con descargas HTTP).
- Almacén de varias versiones del mismo paquete (`<nombre>-<version>`).
- Yanking de versiones desde el registro.
