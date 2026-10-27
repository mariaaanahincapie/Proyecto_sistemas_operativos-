# 06. Decisiones pendientes y supuestos

Cada punto tiene una **propuesta**. Confirmar con el profesor en la reunión de la primera entrega.

| # | Tema | Propuesta |
|---|---|---|
| 1 | Significado de `-ee` (aparece en la sinopsis de `init` sin explicación) | Capacidad del robot, omisión 50 |
| 2 | Orden requerido para entregar resultados | FIFO: salen en el orden en que terminan los analizadores |
| 3 | Unidades de consumo de reactivo | Cada muestra gasta `q` unidades (su cantidad) |
| 4 | Duración y cantidad por recarga del robot | 3 s de ida + 3 s de vuelta; carga hasta `-ee` unidades; tope 100 |
| 5 | Comportamiento al cerrar el sistema | Ctrl+C en `init`: apagado ordenado, se libera la memoria compartida |
| 6 | Nombre de la memoria compartida: `-n` en `init`/`reg`, `-s` en `ctrl`/`rep` | Se respeta tal cual; en `init`, `-s` es reactivo de piel |
| 7 | No concluyente: `-` en `ctrl`, `?` en `rep` | Internamente `?`; `list reported` imprime `-` |
| 8 | Corchetes en la salida | Se imprimen `[id i k ...]`, un registro por línea |
| 9 | Contenido del archivo `.spl` | Un id por muestra aceptada, en orden |
| 10 | Tiempo máximo de espera de una muestra | 120 s (`EVALUATOR_MAX_WAIT`); al vencer, `?` |
| 11 | Cantidad de analizadores | Uno por tipo (3), según el texto; la figura 1 sugiere más |
| 12 | Comentarios (`#`) en archivos de muestras | `reg` ignora líneas vacías y con `#` |
