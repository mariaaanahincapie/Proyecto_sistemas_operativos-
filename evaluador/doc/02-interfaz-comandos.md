# 02. Interfaz de comandos

## 1. Envoltorio (wrapper)

El usuario siempre ejecuta **`evaluator`** (guía §3). El wrapper lee el primer argumento y ejecuta el programa real con el resto de argumentos:

| Comando | Programa real |
|---|---|
| `evaluator init ...` | `evaluator-init` |
| `evaluator reg ...` | `evaluator-reg` |
| `evaluator ctrl ...` | `evaluator-ctrl` |
| `evaluator rep ...` | `evaluator-rep` |

Los programas reales se buscan en `EVALUATOR_BIN` o, si no está definida, en la carpeta del wrapper. En Linux se usa `execv` (el programa real **reemplaza** al wrapper, conserva el pid y la consola); en Windows `_spawnv(_P_WAIT)` y se devuelve su código de salida. Comando desconocido o ausente: imprime el uso y sale con código 1.

## 2. Variables de ambiente

| Variable | Significado | Omisión |
|---|---|---|
| `EVALUATOR_SHM` | nombre de la memoria compartida | `evaluator` |
| `EVALUATOR_BIN` | carpeta de los programas reales | carpeta del wrapper |
| `EVALUATOR_MAX_WAIT` | segundos máximos de espera de una muestra (la lee `init`) | `120` |
| `EVALUATOR_SPEED` | divide los tiempos reales de espera (pruebas/demostración) | `1` |

La opción `-n` (o `-s` en `ctrl`/`rep`) **gana** sobre `EVALUATOR_SHM`. Nombre válido: letras, dígitos, `_` o `-`, 1 a 40 caracteres.

## 3. Códigos de salida

| Código | Significado |
|---|---|
| 0 | correcto |
| 1 | argumentos inválidos (también: `reg` encontró líneas inválidas; `init` con nombre ya en uso) |
| 2 | el sistema no está iniciado, o se detuvo mientras el comando esperaba |
| 3 | error interno (no se pudo crear la memoria, ejecutar el programa real, etc.) |

## 4. `evaluator init`

```
evaluator init [-i <int>] [-ie <int>] [-oe <int>] [-n <string>] [-b <int>] [-d <int>]
               [-ee <int>] [-s <int>] [-q <int>]
```

| Opción | Significado | Omisión | Rango aceptado |
|---|---|---|---|
| `-i` | número de colas de entrada | 5 | 1..64 |
| `-ie` | posiciones de cada cola de entrada | 6 | 1..1024 |
| `-oe` | posiciones de la cola de salida | 10 | 1..1024 |
| `-n` | nombre de la memoria compartida | `evaluator` | ver §2 |
| `-b` | reactivo inicial de sangre | 100 | 0..100 |
| `-d` | reactivo inicial de detritos | 100 | 0..100 |
| `-s` | reactivo inicial de piel | 100 | 0..100 |
| `-q` | tamaño de cada cola interna | 6 | 1..1024 |
| `-ee` | capacidad del robot (unidades por viaje) | 50 | 1..100 |

Pasos:
1. Validar argumentos (cualquier error → mensaje, uso y código 1).
2. Calcular el tamaño exacto de la región a partir de `-i`, `-ie`, `-oe`, `-q`.
3. Crear la región. Si el nombre ya existe: si su `init` sigue vivo → "ya está iniciado" y código 1 sin tocar nada; si está muerto (huérfano) → se borra y se crea de nuevo.
4. Escribir la cabecera, inicializar los semáforos, colas y reactivos; marcar `ready = 1`.
5. Crear los hilos: `i` de entrada, 3 analizadores, robot y vigilante.
6. Imprimir `Sistema "<nombre>" listo ...` y quedarse ocupando la consola mostrando la actividad.
7. Con Ctrl+C: apagado ordenado (doc/04 §8), resumen y liberación del segmento.

## 5. `evaluator reg`

```
evaluator reg [-n <string>] { <archivo> ... | - }
```

Cada línea: `<cola> {B|D|S} <cantidad>` con cola en `0..i-1` y cantidad en `1..5`.

- Se aceptan varios archivos y `-` (entrada estándar) mezclados, en orden.
- Líneas vacías o que empiezan con `#` se ignoran.
- Línea inválida: mensaje en `stderr` con archivo y número de línea; se omite y se sigue. Al final el código es 1.
- Archivo inexistente: se ignora **sin mensaje** (guía §3.1.2).
- Si la cola indicada está llena: **espera** a que se libere una posición (guía §3.2.2).
- Salida: interactivo → imprime el id de cada muestra aceptada; archivo `x.txt` → crea `x.spl` con un id por línea (si el archivo ya terminaba en `.spl`, se escribe `x.spl.spl`).
- Si `init` se detiene mientras `reg` espera: mensaje y código 2.

## 6. `evaluator ctrl`

```
evaluator ctrl [-s <string>]
```

Modo interactivo (prompt `> ` solo si la entrada es una terminal). Termina con fin de archivo (Ctrl+D; en Windows Ctrl+Z y Enter).

| Comando | Salida |
|---|---|
| `list processing` | `Processing:` y `[id i k q p]` por cada analizador ocupado |
| `list waiting` | `Waiting:` y `[id i k q]`: colas de entrada + muestras "en mano" de los hilos de entrada + colas internas, ordenadas por id |
| `list reported` | `Reported:` y `[id i k r]`, con `r` = `positive`, `negative` o `-`. Son los resultados en la salida que ningún `rep` ha tomado (no se retiran) |
| `list reactive` | `Reactive:`, una línea `B n`, `D n`, `S n` (con "(recarga solicitada)" si aplica) y el estado del robot |
| `list all` o `list` | los cuatro en ese orden |
| `update {B\|D\|S} <n>` | suma `n > 0` al reactivo (tope 100) e imprime `Reactive B: antes -> después`; despierta al analizador si esperaba |

Errores (en `stderr`, `ctrl` sigue): `error: valor inválido ...`, `error: comando desconocido ...`.

## 7. `evaluator rep`

```
evaluator rep [-s <string>] { -i <segundos> | -m <cantidad> }
```

- **`-m n`**: obtiene exactamente `n` resultados; si no hay, espera hasta completar la cifra.
- **`-i s`**: modo interactivo durante `s` segundos: imprime cada resultado apenas sale y termina al vencer el tiempo (aunque no haya llegado ninguno).
- Formato: `[id i k r]` con `r` ∈ {`P`, `N`, `?`}.
- Cada resultado tomado se **retira** de la salida (se libera su casilla). Varios `rep` simultáneos son seguros: cada resultado lo recibe exactamente uno.
