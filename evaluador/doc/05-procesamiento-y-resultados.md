# 05. Procesamiento y resultados

## 1. Tipos de muestra y tiempo de procesamiento

Por cada muestra se toma un valor **aleatorio uniforme** entre el mínimo y el máximo:

| Tipo | Letra | Tiempo (s) |
|---|---|---|
| Sangre (Blood) | `B` | 1 a 7 |
| Detritos (Detritus) | `D` | 5 a 20 |
| Piel (Skin) | `S` | 8 a 25 |

Cada analizador solo procesa su tipo. Cada hilo usa su propia semilla (`std::mt19937` sembrada con `time` y `pid`), para que no se pisen.

## 2. Consumo de reactivo

Cada muestra gasta tantas unidades como su cantidad `q` (1 a 5). Si no alcanza, el analizador pide el robot (ver 04).

## 3. Resultado

Se genera `r` entero en `[0, 99]`:

| `r` | Resultado | Probabilidad |
|---|---|---|
| 0–49  | `P` positivo | 50 % |
| 50–84 | `N` negativo | 35 % |
| 85–99 | `?` no concluyente (hay que repetir) | 15 % |

## 4. Formatos de salida

| Origen | Formato |
|---|---|
| `reg` interactivo | `<id>` |
| `reg` con archivo | archivo `.spl` con un `<id>` por línea |
| `ctrl list processing` | `[id i k q p]` |
| `ctrl list waiting` | `[id i k q]` |
| `ctrl list reported` | `[id i k r]` (no concluyente como `-`) |
| `ctrl list reactive` | `B n`, `D n`, `S n` |
| `rep` | `[id i k r]` con `r` en `N`, `P`, `?` |

Donde `id` = identificador, `i` = cola de entrada, `k` = tipo, `q` = cantidad, `p` = segundos de procesamiento, `r` = resultado.
