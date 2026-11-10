# evaluador — Simulación de un sistema paralelo de evaluación de exámenes médicos

Práctica de Sistemas Operativos (ST0257, 2026-2). Varios **procesos** independientes (`reg`, `ctrl`, `rep`) se comunican con el proceso **evaluador** (`init`) a través de **una única región de memoria compartida** que contiene las colas de entrada, la cola de salida, los semáforos y las variables de control. Dentro de `init` trabajan **hilos**: uno por cola de entrada, uno por tipo de muestra, un robot de recarga y un vigilante.

> Simulación académica: los resultados son aleatorios y no tienen validez clínica.

## Integrantes

- Mariana Hincapié Henao
- Martin Caballero

## Lenguaje, herramientas y bibliotecas

| | |
|---|---|
| Lenguaje | C++17 (solo biblioteca estándar) |
| Linux | g++ ≥ 9 o clang++ ≥ 10, `make` (o CMake ≥ 3.16). Usa POSIX: `shm_open`, `mmap`, `sem_init` (pshared), `pthread` |
| Windows | Visual Studio 2019+ (MSVC) o MinGW-w64 + CMake ≥ 3.16. Usa Win32: `CreateFileMapping`, `CreateSemaphore` con nombre |
| Bibliotecas externas | ninguna |

## Compilar y probar (Linux)

```sh
cd evaluador
make            # deja en bin/ el wrapper y los 4 programas reales
make test       # 43 pruebas automáticas (≈ 1 min)
```

En Windows y otras instalaciones ver [Install.md](Install.md).

## Programas

El usuario siempre usa el **envoltorio** `evaluator`, que ejecuta el programa real (guía §3):

| Comando | Programa real | Qué hace |
|---|---|---|
| `evaluator init [-i n] [-ie n] [-oe n] [-n nombre] [-b n] [-d n] [-ee n] [-s n] [-q n]` | `evaluator-init` | Crea la memoria compartida, arranca los hilos y ocupa la consola hasta Ctrl+C |
| `evaluator reg [-n nombre] { archivo ... \| - }` | `evaluator-reg` | Registra muestras `<cola> <B\|D\|S> <cantidad>`; devuelve ids únicos |
| `evaluator ctrl [-s nombre]` | `evaluator-ctrl` | Interactivo: `list [processing\|waiting\|reported\|reactive\|all]`, `update {B\|D\|S} n` |
| `evaluator rep [-s nombre] { -i segundos \| -m cantidad }` | `evaluator-rep` | Retira resultados de la salida e imprime `[id i k r]` |

Valores por omisión de `init`: `-i 5`, `-ie 6`, `-oe 10`, `-n evaluator`, `-b/-d/-s 100` (máximo 100), `-q 6`, `-ee 50` (capacidad del robot).

## Uso rápido (4 terminales)

```sh
# Terminal 1: el evaluador (queda ocupada, muestra el registro de actividad)
bin/evaluator init -i 3 -b 10

# Terminal 2: registrar
bin/evaluator reg examples/muestras.txt         # crea examples/muestras.spl con los ids
bin/evaluator reg -                             # interactivo: escriba "0 B 2" y Enter; Ctrl+D termina

# Terminal 3: inspeccionar y recargar
bin/evaluator ctrl
> list all
> update B 20

# Terminal 4: reportes
bin/evaluator rep -m 3       # espera hasta obtener 3 resultados
bin/evaluator rep -i 30      # durante 30 s imprime lo que vaya saliendo
```

Para una demostración rápida se puede acelerar la simulación: `export EVALUATOR_SPEED=5` (los tiempos se dividen por 5; los tiempos reportados no cambian).

## Variables de ambiente

| Variable | Uso | Omisión |
|---|---|---|
| `EVALUATOR_SHM` | nombre de la memoria compartida si no se da `-n`/`-s` | `evaluator` |
| `EVALUATOR_BIN` | carpeta donde el wrapper busca los programas reales | carpeta del wrapper |
| `EVALUATOR_MAX_WAIT` | segundos máximos que puede esperar una muestra (después se reporta `?`) | `120` |
| `EVALUATOR_SPEED` | factor de aceleración de la simulación (pruebas y demostraciones) | `1` |

## Estructura

```text
evaluador/
├── Makefile, CMakeLists.txt, Readme.md, Install.md
├── src/
│   ├── main/          evaluator.cpp (wrapper), ctrl.cpp
│   ├── evaluator/     init.cpp (main de init), evaluator.{hpp,cpp} (hilos)
│   ├── registration/  reg.cpp
│   ├── output/        rep.cpp, format.hpp (formatos de salida)
│   ├── queues/        ring.{hpp,cpp} (colas compartidas), internal_queue.hpp (colas internas)
│   ├── concurrency/   semaphore.{hpp,cpp} (semáforos entre procesos Linux/Windows)
│   └── shared/        model.hpp, shm.{hpp,cpp} (memoria compartida), platform.*, cli.hpp
├── bin/               ejecutables (no se suben al repositorio)
├── doc/               diseño en Markdown (00 a 07)
├── examples/          archivos de muestras
└── tests/             run_tests.sh
```

## Documentación

[00 Matriz de requisitos](doc/00-matriz-requisitos.md) ·
[01 Arquitectura](doc/01-arquitectura.md) ·
[02 Interfaz de comandos](doc/02-interfaz-comandos.md) ·
[03 Colas y estructuras](doc/03-colas-y-estructuras.md) ·
[04 Concurrencia](doc/04-concurrencia.md) ·
[05 Procesamiento y resultados](doc/05-procesamiento-y-resultados.md) ·
[06 Decisiones](doc/06-decisions.md) ·
[07 Guía de sustentación](doc/07-guia-sustentacion.md)

## Estado de verificación

| Plataforma | Estado |
|---|---|
| Linux (g++ 13) | Compila sin advertencias (`-Wall -Wextra -Wpedantic`), pasa las 43 pruebas, también con AddressSanitizer/UBSan |
| Windows | Compila sin advertencias con MinGW-w64 (`make OS_WIN=1 CXX=x86_64-w64-mingw32-g++-posix`). Falta probar la ejecución en una máquina Windows real |

## Limitaciones conocidas

- Si un proceso cliente muere justo dentro de una sección crítica (microsegundos) deja el semáforo tomado; hay que reiniciar `init`.
- Al cerrar `init` se descartan las muestras que estaban en el sistema.
- Un analizador (hilo) por tipo de muestra, como dice el texto de la guía.
