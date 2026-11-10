# 05. Procesamiento y resultados

## 1. Tiempo de procesamiento (guía §3.2.7)

Por cada muestra se elige un entero **uniforme** entre el mínimo y el máximo de su tipo:

| Tipo | Letra | Tiempo (s) |
|---|---|---|
| Sangre (Blood) | `B` | 1 a 7 |
| Detritos (Detritus) | `D` | 5 a 20 |
| Piel (Skin) | `S` | 8 a 25 |

Cada analizador solo procesa su tipo. Cada hilo tiene su propio generador `std::mt19937` sembrado con `random_device`, la hora, el pid y el número de hilo (no comparten estado, así que no necesitan candado).

## 2. Consumo de reactivo

Cada muestra gasta tantas unidades como su cantidad `q` (1 a 5). El nivel de cada reactivo está entre 0 y 100 (el máximo de la guía). Si no alcanza, el analizador pide el robot (doc/04 §4). Una muestra vencida (espera máxima) no gasta reactivo.

## 3. Resultado (guía §3.2.8)

Se genera `r` uniforme en `[0, 99]`:

| `r` | Resultado | Probabilidad |
|---|---|---|
| 0–49 | `P` positivo (presenta una enfermedad) | 50 % |
| 50–84 | `N` negativo (no presenta peligro) | 35 % |
| 85–99 | `?` no concluyente (hay que repetir el examen) | 15 % |

El sistema no repite automáticamente los no concluyentes: se reportan y el usuario puede volver a registrar la muestra (decisión D-13).

## 4. Formatos de salida

| Origen | Formato | Valores de `r` |
|---|---|---|
| `reg` interactivo | `<id>` | — |
| `reg` con archivo `x.txt` | archivo `x.spl` con un `<id>` por línea | — |
| `ctrl list processing` | `Processing:` + `[id i k q p]` | — |
| `ctrl list waiting` | `Waiting:` + `[id i k q]` | — |
| `ctrl list reported` | `Reported:` + `[id i k r]` | `positive`, `negative`, `-` (guía §3.1.3) |
| `ctrl list reactive` | `Reactive:` + `B n` / `D n` / `S n` + `Robot: ...` | — |
| `rep` | `[id i k r]` | `P`, `N`, `?` (guía §3.2.6) |

`id` = identificador, `i` = cola de entrada, `k` = tipo, `q` = cantidad, `p` = segundos de procesamiento, `r` = resultado. Cada registro va en su propia línea.

Ejemplo de sesión:

```
$ evaluator ctrl
> list all
Processing:
[1 0 B 2 4]
[3 2 S 3 17]
Waiting:
[4 1 B 1]
[5 0 D 4]
Reported:
[2 1 D negative]
Reactive:
B 98
D 96
S 97
Robot: libre

$ evaluator rep -m 1
[2 1 D N]
```
