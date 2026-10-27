# 02. Interfaz de comandos

## 1. Envoltorio (wrapper)

El usuario siempre ejecuta **`evaluator`**. Lee el primer argumento y ejecuta el programa real:

| Comando | Programa real |
|---|---|
| `evaluator init ...` | `bin/evaluator-init` |
| `evaluator reg ...`  | `bin/evaluator-reg` |
| `evaluator ctrl ...` | `bin/evaluator-ctrl` |
| `evaluator rep ...`  | `bin/evaluator-rep` |

Si el comando no existe: imprime el uso y sale con código 1.

## 2. Variables de ambiente

| Variable | Significado | Omisión |
|---|---|---|
| `EVALUATOR_SHM` | Nombre de la memoria compartida | `evaluator` |
| `EVALUATOR_BIN` | Carpeta donde el wrapper busca los programas reales | carpeta del wrapper |
| `EVALUATOR_MAX_WAIT` | Segundos máximos de espera de una muestra | `120` |

Si el usuario pasa `-n` (o `-s` en `ctrl`/`rep`), eso **gana** sobre la variable.

## 3. Códigos de salida

`0` bien · `1` argumentos inválidos · `2` el sistema no está iniciado · `3` error interno.

## 4. `evaluator init`

```
evaluator init [-i <int>] [-ie <int>] [-oe <int>] [-n <string>]
               [-b <int>] [-d <int>] [-s <int>] [-ee <int>] [-q <int>]
```

| Opción | Significado | Omisión |
|---|---|---|
| `-i`  | número de colas de entrada | 5 |
| `-ie` | posiciones por cola de entrada | 6 |
| `-oe` | posiciones de la cola de salida | 10 |
| `-n`  | nombre de la memoria compartida | `evaluator` |
| `-b`  | reactivo inicial de sangre | 100 |
| `-d`  | reactivo inicial de detritos | 100 |
| `-s`  | reactivo inicial de piel | 100 |
| `-q`  | tamaño de cada cola interna | 6 |
| `-ee` | capacidad del robot (supuesto, ver 06) | 50 |

Todos deben ser enteros positivos; si no, error y código 1.

Pasos de `init`:
1. Validar argumentos y calcular el tamaño de la región.
2. ¿Ya existe el sistema? Abrir el segmento, leer `init_pid`, probar con `kill(pid, 0)`. Si vive: avisar "ya iniciado" y salir sin crear nada. Si no vive (huérfano): `shm_unlink` y crear uno nuevo.
3. `shm_open(O_CREAT|O_EXCL)`, `ftruncate`, `mmap`.
4. Escribir cabecera, inicializar semáforos (`pshared=1`), colas y reactivos.
5. Crear hilos: `i` de entrada, 3 analizadores, robot, vigilante.
6. Imprimir `Sistema "<nombre>" listo` y quedarse en la consola hasta Ctrl+C (apagado ordenado, ver 04).

## 5. `evaluator reg`

```
evaluator reg [-n <string>] { <archivo> ... | - }
```
`-` = modo interactivo (lee de la entrada estándar).

Cada línea: `<cola> <B|D|S> <cantidad>`
- cola entera en `0..i-1`; tipo exactamente `B`, `D` o `S`; cantidad entera en `1..5`.
- Las líneas vacías y las que empiezan con `#` se ignoran.
- Línea inválida: mensaje a `stderr` con el número de línea; se omite y se continúa.
- Archivo inexistente: se ignora en silencio.
- Si la cola está llena: **espera** hasta que haya espacio.
- Si el sistema no está iniciado: código 2.

Salida del id:
- **Interactivo:** imprime el id en una línea.
- **Archivo `nombre.ext`:** crea `nombre.spl` con un id por línea, en orden de las muestras aceptadas.

## 6. `evaluator ctrl`

```
evaluator ctrl [-s <string>]
```
Modo interactivo con prompt `> `. Termina con fin de archivo (Ctrl+D). Cada lectura toma el mutex de la estructura consultada.

| Comando | Salida |
|---|---|
| `list processing` | `Processing:` y una línea `[id i k q p]` por muestra |
| `list waiting` | `Waiting:` y `[id i k q]` (colas de entrada + muestra "en mano" + colas internas) |
| `list reported` | `Reported:` y `[id i k r]` (contenido actual de la salida, sin extraerlo) |
| `list reactive` | `Reactive:` y líneas `B 85`, `D 100`, `S 40` |
| `list all` o `list` | los cuatro, en ese orden |
| `update <B\|D\|S> <n>` | suma `n` al reactivo; despierta al analizador si esperaba |

Errores: `error: valor inválido`, `error: comando desconocido`.
En `list reported`, el resultado no concluyente se imprime como `-`.

## 7. `evaluator rep`

```
evaluator rep [-s <string>] { -i <segundos> | -m <cantidad> }
```
- **`-m n`:** repite `n` veces: espera un resultado (se bloquea si no hay), lo toma bajo mutex y lo imprime.
- **`-i s`:** hora límite = ahora + `s`. Toma y imprime resultados mientras lleguen; al vencer el tiempo termina.
- Formato: `[id i k r]` con `r` ∈ {`N`, `P`, `?`}.
- Varios `rep` a la vez son seguros: cada resultado lo recibe un solo `rep`.
