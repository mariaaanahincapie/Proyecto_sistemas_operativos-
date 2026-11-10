# 01. Arquitectura

**Lenguaje:** C++17 · **Linux:** POSIX · **Windows:** Win32 (misma lógica, capa de plataforma distinta).

## 1. Qué es el sistema

Simula un laboratorio con analizadores en paralelo. Recibe **muestras** (sangre `B`, detritos `D`, piel `S`), las procesa y entrega **resultados**. Está formado por **cinco programas** que se comunican por **una sola región de memoria compartida**; el programa central (`init`) tiene **varios hilos**.

| Componente (guía §2) | Qué hace | Implementación |
|---|---|---|
| Registro | Envía muestras y les da un id único | proceso `evaluator-reg` (pueden correr varios a la vez) |
| Entrada | `i` colas circulares de `ie` posiciones | memoria compartida (`RingHeader` + casillas) |
| Evaluador | Despacha por tipo y procesa | proceso `evaluator-init`: hilos de entrada, analizadores, robot, vigilante |
| Salida | Cola circular de `oe` resultados, en orden | memoria compartida |
| Reporte | Entrega y libera resultados | proceso `evaluator-rep` |
| Control | Inspecciona y recarga reactivo | proceso `evaluator-ctrl` |
| Envoltorio | Interfaz única con el usuario | `evaluator` (ejecuta el programa real) |

## 2. Diagrama

```
 PROCESOS reg (varios)         MEMORIA COMPARTIDA (una sola región)          PROCESO init (hilos)
 ┌────────┐               ┌─────────────────────────────────┐
 │ reg #1 │──┐  id único  │ Cabecera: config, next_id,      │
 └────────┘  ├──────────► │  reactivos, robot, tablero,     │
 ┌────────┐  │            │  semáforos (sem_t)              │
 │ reg #2 │──┘            │ Cola entrada 0 [ie] ────────────┼──► Hilo entrada 0 ─┐
 └────────┘               │ Cola entrada 1 [ie] ────────────┼──► Hilo entrada 1 ─┤ despacha
 ┌────────┐               │ ...                             │    ...             │ por tipo
 │ ctrl   │◄─ lee/escribe─│ Cola entrada i-1 [ie] ──────────┼──► Hilo entrada i-1┘
 └────────┘               │ "En mano" de cada hilo entrada  │        │ (memoria propia de init)
                          │ Espejo de colas internas        │        ▼
 ┌────────┐               │                                 │  Cola interna B [q] ─► Analizador B ─┐
 │ rep    │◄─ retira ─────│ Cola de salida [oe] ◄───────────┼─────────────────────────────────────┤
 └────────┘               └─────────────────────────────────┘  Cola interna D [q] ─► Analizador D ─┤
                                                               Cola interna S [q] ─► Analizador S ─┘
                                                               Robot (único) ──► recarga reactivo
                                                               Vigilante ──► avisa esperas largas
```

## 3. Procesos e hilos

| Quién | Tipo | Cantidad | Rol productor/consumidor |
|---|---|---|---|
| `reg` | proceso | varios | productor de una cola de entrada |
| `init` | proceso | 1 | dueño de la región; contiene los hilos |
| hilo de entrada `k` | hilo de `init` | `i` | consumidor de la cola de entrada `k`, productor de las colas internas |
| analizador `t` (B, D, S) | hilo de `init` | 3 | consumidor de su cola interna, productor de la salida |
| robot | hilo de `init` | 1 | atiende pedidos de recarga |
| vigilante | hilo de `init` | 1 | cada 5 s avisa muestras que llevan mucho esperando |
| hilo principal de `init` | hilo | 1 | espera Ctrl+C y hace el apagado ordenado |
| `ctrl` | proceso | varios | lector del estado; `update` escribe reactivos |
| `rep` | proceso | varios | consumidor de la cola de salida |

## 4. Código fuente

| Carpeta / archivo | Contenido |
|---|---|
| `src/main/evaluator.cpp` | envoltorio: elige y ejecuta el programa real |
| `src/main/ctrl.cpp` | programa `evaluator-ctrl` |
| `src/registration/reg.cpp` | programa `evaluator-reg` |
| `src/output/rep.cpp`, `format.hpp` | programa `evaluator-rep` y formatos `[id i k ...]` |
| `src/evaluator/init.cpp` | programa `evaluator-init` (argumentos, creación, apagado) |
| `src/evaluator/evaluator.{hpp,cpp}` | los hilos: entrada, analizador, robot, vigilante |
| `src/queues/ring.{hpp,cpp}` | operaciones productor-consumidor sobre las colas compartidas |
| `src/queues/internal_queue.hpp` | cola interna acotada (`std::mutex` + `std::condition_variable`) |
| `src/concurrency/semaphore.{hpp,cpp}` | semáforos entre procesos (Linux `sem_t` / Windows con nombre) |
| `src/shared/shm.{hpp,cpp}` | creación, apertura y distribución de la memoria compartida |
| `src/shared/model.hpp` | `Sample`, constantes, valores por omisión |
| `src/shared/platform.{hpp,cpp}` | pid, ¿proceso vivo?, Ctrl+C, carpeta del ejecutable |
| `src/shared/cli.hpp` | validación de enteros y resolución del nombre del segmento |

## 5. Portabilidad Linux / Windows

Todo lo que depende del sistema operativo está en `src/concurrency/semaphore.cpp`, `src/shared/shm.cpp`, `src/shared/platform.cpp` y el wrapper. El resto (colas, hilos, lógica) es C++17 estándar idéntico en los dos sistemas.

| Concepto | Linux | Windows |
|---|---|---|
| Memoria compartida | `shm_open("/<nombre>")` + `ftruncate` + `mmap` (queda en `/dev/shm/<nombre>`) | `CreateFileMapping("Local\<nombre>")` + `MapViewOfFile` |
| Semáforo entre procesos | `sem_t` **dentro** de la región, `sem_init(&s, 1, v)` | `CreateSemaphore("Local\<nombre>_sem<k>")`; en la región solo se guarda `k` |
| Espera con tiempo límite | `sem_timedwait` | `WaitForSingleObject` |
| Hilos, mutex, variables de condición | `std::thread`, `std::mutex`, `std::condition_variable` | iguales |
| ¿init sigue vivo? | `kill(pid, 0)` | `OpenProcess` + `WaitForSingleObject(h, 0)` |
| Ctrl+C | `sigaction(SIGINT/SIGTERM)` | `SetConsoleCtrlHandler` |
| Wrapper → programa real | `execv` (reemplaza el proceso) | `_spawnv(_P_WAIT)` y devuelve el código |
| Borrado del segmento | `shm_unlink` al cerrar `init` | automático cuando se cierra el último handle |
