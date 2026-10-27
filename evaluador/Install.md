# Instalación

## Requisitos

- Linux (o Windows, ver `doc/01-arquitectura.md`)
- g++ con C++17 (versión 9 o superior) o clang++
- `make` (o CMake 3.16+)

## Compilar en el directorio local

```
make
```
Los binarios quedan en `bin/`.

## Instalar en un directorio distinto

```
make install PREFIX=/ruta/destino
```
Copia los programas a `/ruta/destino/bin`. Para usarlos, agrega esa carpeta al `PATH`:

```
export PATH=/ruta/destino/bin:$PATH
```
El wrapper `evaluator` busca los programas reales en su propia carpeta, o en la indicada por `EVALUATOR_BIN`.

## Con CMake

```
cmake -S . -B build
cmake --build build
```

## Estado actual

Primera entrega (diseño): todavía no hay código compilable.
