# 06. Decisiones y supuestos

La guía deja puntos abiertos. Cada uno se resolvió así (en la primera entrega eran propuestas; aquí queda lo implementado).

| # | Tema | Decisión implementada |
|---|---|---|
| D-01 | Significado de `-ee` (está en la sinopsis de `init` sin explicación) | Capacidad del robot: unidades que lleva por viaje. Omisión 50, rango 1..100 |
| D-02 | Orden de la salida | FIFO: los resultados salen en el orden en que los analizadores los depositan |
| D-03 | Consumo de reactivo | Cada muestra gasta `q` unidades (su cantidad) |
| D-04 | Viaje del robot | 3 s al almacén + 3 s de regreso + 1 s de carga; el almacén no se agota; el nivel no pasa de 100 |
| D-05 | Cierre del sistema | Ctrl+C en `init`: apagado ordenado, resumen y borrado de la memoria compartida |
| D-06 | Nombre de la memoria: `-n` en `init`/`reg`, `-s` en `ctrl`/`rep` | Se respeta tal cual la guía; en `init`, `-s` es el reactivo de piel. También `EVALUATOR_SHM` |
| D-07 | No concluyente: `-` en `ctrl`, `?` en `rep` | Internamente `?`. `list reported` imprime `positive`/`negative`/`-` (§3.1.3); `rep` imprime `P`/`N`/`?` (§3.2.6) |
| D-08 | Corchetes en la salida | Se imprimen `[id i k ...]`, un registro por línea |
| D-09 | Contenido del `.spl` | Un id por muestra aceptada, en orden; se escribe a medida que se aceptan |
| D-10 | Tiempo máximo de espera de una muestra | 120 s (`EVALUATOR_MAX_WAIT`); al vencer se reporta `?` sin gastar reactivo |
| D-11 | Cantidad de analizadores | Uno (un hilo) por tipo, como dice el texto §3.2.4 |
| D-12 | Comentarios en archivos de muestras | `reg` ignora líneas vacías y las que empiezan con `#` |
| D-13 | Repetir los no concluyentes | No se repiten automáticamente; el usuario decide registrar de nuevo |
| D-14 | Valor máximo de reactivo | 100: `init` acepta 0..100 y `update` suma con tope en 100 (avisa si recorta) |
| D-15 | `rep -i` | Durante los segundos indicados imprime cada resultado que sale; termina al vencer el tiempo |
| D-16 | Muestra terminada con la salida llena | Sigue en `list processing` (el analizador está ocupado) hasta que haya espacio |
| D-17 | `init` con un nombre ya en uso | Si el `init` dueño vive: error (código 1). Si murió: se limpia el segmento y se crea uno nuevo |

## Cambios respecto al diseño de la primera entrega

| Diseño (entrega 1) | Implementación (entrega 2) | Motivo |
|---|---|---|
| Robot: `-ee` por omisión 50, trayectos de 3 s | Igual, más 1 s de **carga** durante la cual el analizador no puede procesar (candado `machine[t]`) | La guía exige que mientras se carga el reactivo el analizador no procese otra muestra |
| El analizador sacaba la muestra de la cola interna antes de pedir reactivo | La mira sin sacarla (`front`) y la saca al empezar a procesar | Así la muestra sigue visible en `list waiting` mientras espera reactivo |
| `list reported` con `P`/`N`/`-` | `positive`/`negative`/`-` | Texto literal de §3.1.3 |
| Semáforos en Windows "con nombre `<shm>_<semáforo>`" | `Local\<nombre>_sem<k>`, con `k` guardado en la región | Una sola abstracción (`SemSlot`) para los dos sistemas |
| `EVALUATOR_SPEED` no existía | Variable para acelerar la simulación | Pruebas automáticas y demostraciones rápidas |
