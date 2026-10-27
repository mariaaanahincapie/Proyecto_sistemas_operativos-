# 01. Arquitectura

**Lenguaje:** C++17 · **Sistema principal:** Linux (POSIX) · **Portabilidad:** Windows (ver sección 5).

## 1. Qué es el sistema

Simula un laboratorio con analizadores en paralelo. Recibe **muestras** (sangre `B`, detritos `D`, piel `S`), las procesa y entrega **resultados**. Se construye con varios **programas** que se comunican por **una sola memoria compartida**, y un programa central (`init`) con **varios hilos**.

| Componente | Qué hace | Quién lo implementa |
|---|---|---|
| Registro | Envía muestras y les da un id único | comando `reg` (varias instancias a la vez) |
| Entrada | `i` colas donde esperan las muestras | memoria compartida |
| Evaluador | Clasifica y procesa muestras | proceso `init` con hilos |
| Salida | Cola de resultados terminados | memoria compartida |
| Reporte | Muestra resultados al usuario | comando `rep` |
| Control | Inspecciona y recarga reactivo | comando `ctrl` |

## 2. Diagrama

```
 PROCESOS reg (varios)        MEMORIA COMPARTIDA                       PROCESO init (hilos)
 ┌────────┐              ┌──────────────────────────┐
 │ reg #1 │──┐           │  Cola entrada 0 [ie]     │──► Hilo entrada 0 ─┐
 └────────┘  ├─ id ───►  │  Cola entrada 1 [ie]     │──► Hilo entrada 1 ─┤  clasifica
 ┌────────┐  │           │  ...                     │──► ...             │  por tipo
 │ reg #2 │──┘           │  Cola entrada i-1 [ie]   │──► Hilo entrada i-1┘
 └────────┘              │                          │          │
                         │  Reactivos B, D, S       │          ▼
 ┌────────┐              │  Tablero de estado       │   Cola interna B ─► Analizador B ─┐
 │ ctrl   │◄── lee/escr ─│  Contador de ids         │   Cola interna D ─► Analizador D ─┤
 └────────┘              │                          │   Cola interna S ─► Analizador S ─┤
                         │  Cola de salida [oe]     │◄──────────────────────────────────┘
 ┌────────┐              └──────────────────────────┘          ▲
 │ rep    │◄── toma resultados ──┘                       Hilo robot ──► recarga reactivo
 └────────┘                                               Hilo vigilante (timeouts/log)
```

## 3. Hilos dentro de `init`

- `i` hilos de **entrada** (uno por cola de entrada).
- 3 hilos **analizador** (uno por tipo: B, D, S).
- 1 hilo **robot**.
- 1 hilo **vigilante** (avisos de espera excesiva).

## 4. Estructura de directorios

| Carpeta | Contenido |
|---|---|
| `src/main/` | `evaluator.cpp` (wrapper) y `ctrl.cpp` |
| `src/registration/` | `reg.cpp` |
| `src/queues/` | colas circulares (entrada, salida, internas) |
| `src/evaluator/` | `init.cpp`, hilos de entrada, analizadores, robot |
| `src/output/` | `rep.cpp` y formato de salida |
| `src/concurrency/` | envoltorio de semáforos y mutex |
| `src/shared/` | estructuras de memoria compartida y constantes |
| `bin/` | binarios (no se suben a git) |
| `doc/` | diseño en Markdown |
| `examples/` | archivos de muestras |
| `tests/` | pruebas |

## 5. Portabilidad Linux / Windows

Todo lo que depende del sistema operativo se aísla en `src/concurrency/` y `src/shared/` (archivos `platform.*`).

| Concepto | Linux | Windows |
|---|---|---|
| Memoria compartida | `shm_open` / `mmap` | `CreateFileMapping` / `MapViewOfFile` |
| Semáforo entre procesos | `sem_t` (pshared) | `CreateSemaphore` con nombre |
| Hilos | `std::thread` | `std::thread` |
| Dormir | `std::this_thread::sleep_for` | igual |
| Señal de apagado | `SIGINT` | `SetConsoleCtrlHandler` |

En Windows los semáforos no viven dentro del bloque: se crean con nombre `<shm>_<semáforo>`. Si el trabajo se hace en solitario, se implementa en el sistema que defina el profesor.
