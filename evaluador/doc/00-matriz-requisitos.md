# 00. Matriz de requisitos

Cada requisito de la guía, dónde está implementado y cómo se verifica (`tests/run_tests.sh`, sección entre paréntesis).

| ID | Requisito (guía) | Implementación | Verificación |
|---|---|---|---|
| R-01 | Programa `evaluator` que invoca a los programas reales (§3) | `src/main/evaluator.cpp` (`execv` / `_spawnv`) | todas las pruebas usan el wrapper (1) |
| R-02 | `init` con `-i -ie -oe -n -b -d -ee -s -q` y valores por omisión (§3.1.1) | `src/evaluator/init.cpp` | (1), (2) |
| R-03 | `init` crea el sistema si no está creado (§3.1.1) | `SharedRegion::create` | segundo `init` rechazado (2); recuperación tras `kill -9` (10) |
| R-04 | Una única región con colas de entrada, salida, semáforos y variables (§3.2.1) | `src/shared/shm.*`, doc/03 | existe `/dev/shm/<nombre>` (2) |
| R-05 | `reg` interactivo (`-`) o con varios archivos (§3.1.2) | `src/registration/reg.cpp` | (2) |
| R-06 | Formato `<int> {B\|D\|S} <int>`, cola `0..i-1`, cantidad `1..5` (§3.1.2) | `parseLine` | 4 líneas inválidas rechazadas (2) |
| R-07 | Id único por muestra; `.spl` para archivos, id en pantalla para interactivo (§3.1.2, §3.2.5) | `mutexId` + `nextId` | `m.spl` = "1 2"; 4 `reg` concurrentes → 120 ids distintos (2), (4) |
| R-08 | Archivo inexistente se ignora sin mensaje (§3.1.2) | `reg.cpp` | (2) |
| R-09 | Si la cola de entrada está llena, `reg` espera (§3.2.2) | `ringPush` (`wait(empty)`) | `reg` bloqueado y luego completa (5) |
| R-10 | Varias instancias de `reg` (§3.2.2) | semáforos entre procesos | (4) |
| R-11 | Cada cola de entrada es un buffer productor-consumidor con sus semáforos (§3.2.3) | `RingHeader` (`mutex`, `empty`, `full`) | (4), (5) |
| R-12 | `init` ocupa la consola; hilos de entrada (uno por cola) y de proceso (uno por tipo) (§3.2.4) | `Evaluator::start` | log de `init` (6) |
| R-13 | Colas internas privadas de `init`, compartidas entre sus hilos (§3.2.4) | `InternalQueue` | (5) |
| R-14 | Despachar al analizador del tipo; si no está disponible, la muestra espera (§2) | `inputWorker`, `analyzerWorker` | (5) |
| R-15 | Una muestra no espera para siempre (§2) | `MAX_WAIT` + vigilante | se reporta `?` y el vigilante avisa (8) |
| R-16 | Reactivo limitado; robot único, un tipo por viaje, capacidad limitada (§2) | `robotWorker`, `-ee` | nivel final 47 y registro del robot (6) |
| R-17 | Mientras se carga reactivo el analizador no procesa (§2) | `std::mutex machine[t]` | doc/04 §4 |
| R-18 | `ctrl` interactivo hasta EOF con `list` y `update` (§3.1.3) | `src/main/ctrl.cpp` | (5), (6), (7) |
| R-19 | Formatos `[id i k q p]`, `[id i k q]`, `[id i k r]` (§3.1.3) | `src/output/format.hpp` | (5) |
| R-20 | `update` suma a la carga actual (§3.1.3) | `ctrl.cpp: update` | 47 + 10 = 57 (6); despierta al analizador (7) |
| R-21 | `rep -i` (tiempo) y `rep -m` (cantidad, espera hasta completar) (§3.1.4) | `src/output/rep.cpp` | (3), (9) |
| R-22 | `rep` libera los reportes que toma (§3.1.4) | `ringPop` + `post(empty)` | `list reported` vacío tras `rep` (3); dos `rep` no comparten resultados (9) |
| R-23 | La salida mantiene el orden y nunca excede su capacidad (§2, §3.2.5) | cola FIFO con `empty` | nunca > `oe`; orden 1..8 (5) |
| R-24 | Tiempos B 1–7, D 5–20, S 8–25 (§3.2.7) | `kProcMin/kProcMax`, `randomProcTime` | revisión de código |
| R-25 | Resultados P 50 %, N 35 %, ? 15 % (§3.2.8) | `randomResult` | revisión de código |
| R-26 | Jerarquía `evaluador/{Makefile,Readme.md,Install.md,src,bin,doc,examples}` (§3.3) | este repositorio | — |
| R-27 | Linux y Windows (§4.5) | capa `platform`/`semaphore`/`shm` | Linux: 43 pruebas; Windows: compilación cruzada MinGW |
| R-28 | Lenguaje permitido (§4.4) | C++17 | — |
