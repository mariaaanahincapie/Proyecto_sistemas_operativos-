# Instalación

## Requisitos

| Herramienta | Linux | Windows |
|---|---|---|
| Compilador C++17 | g++ ≥ 9 o clang++ ≥ 10 | Visual Studio 2019+ (MSVC) o MinGW-w64 |
| Construcción | `make` (o CMake ≥ 3.16) | CMake ≥ 3.16 |
| Git | cualquiera | cualquiera |

No se usan bibliotecas externas.

## Linux (Ubuntu / Debian)

```sh
sudo apt update && sudo apt install -y git g++ make cmake
git clone https://github.com/mariaaanahincapie/Proyecto_sistemas_operativos-.git
cd Proyecto_sistemas_operativos-/evaluador
make              # bin/evaluator, bin/evaluator-init, -reg, -ctrl, -rep
make test         # opcional: pruebas automáticas
```

Con CMake: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build` (los ejecutables quedan igualmente en `bin/`).

## Windows con MSYS2 (terminal UCRT64) — recomendado

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc make git
git clone https://github.com/mariaaanahincapie/Proyecto_sistemas_operativos-.git
cd Proyecto_sistemas_operativos-/evaluador
make              # detecta Windows (OS=Windows_NT) y genera bin/evaluator.exe y los 4 programas
make test         # tests/run_tests_windows.sh
./bin/evaluator.exe init -i 3
```

Abra una ventana UCRT64 por comando. Para terminar `ctrl` o `reg -` use **Ctrl+Z y Enter** (si su terminal no lo toma, Ctrl+D). `init` se detiene con **Ctrl+C**.

## Windows con CMake (PowerShell / Visual Studio)

Desde la carpeta `evaluador`, en una terminal con el compilador disponible ("Developer PowerShell for VS" si usa MSVC):

```powershell
cmake -S . -B build
cmake --build build --config Release
.\bin\evaluator.exe init -i 3
```

Con MinGW: `cmake -S . -B build -G "MinGW Makefiles"` y luego `cmake --build build`.
Abra una terminal por cada comando (`init`, `reg`, `ctrl`, `rep`). En `ctrl` y en `reg -`, el fin de archivo es **Ctrl+Z y Enter**.

## Instalar en un directorio distinto al local

Linux:
```sh
make install PREFIX=/opt/evaluador        # copia los 5 programas a /opt/evaluador/bin
export PATH=/opt/evaluador/bin:$PATH
evaluator init
```

Con CMake (Linux o Windows):
```sh
cmake --install build --prefix /ruta/destino
```

El wrapper `evaluator` busca los programas reales en **su propia carpeta**; si se instalan en carpetas distintas, indique dónde están con `EVALUATOR_BIN=/ruta/a/los/programas`.

## Si algo falla

| Síntoma | Causa / solución |
|---|---|
| `init: el sistema "evaluator" ya está iniciado` | Hay otro `init` vivo con ese nombre. Ciérrelo (Ctrl+C) o use `-n otro`. |
| `reg/ctrl/rep: no está iniciado` (código 2) | Primero `evaluator init` (con el mismo `-n`). |
| `init` murió con `kill -9` | En Linux basta ejecutar `init` de nuevo: detecta que el pid ya no existe y borra el segmento huérfano. A mano: `rm /dev/shm/evaluator`. |
| Windows: "quedan procesos conectados" | Cierre los `reg`/`ctrl`/`rep` que sigan abiertos con ese nombre. |
