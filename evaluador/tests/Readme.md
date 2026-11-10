# Pruebas

`make test` (o `bash tests/run_tests.sh`) ejecuta 43 comprobaciones automáticas en Linux, en unos 60 s.
Usan `EVALUATOR_SPEED=20` para acelerar la simulación y un nombre de segmento propio (`prueba_<pid>`),
así que no interfieren con un sistema que esté corriendo.

En Windows (MSYS2 UCRT64 o Git Bash) `make test` usa `tests/run_tests_windows.sh`: las mismas pruebas salvo las que dependen de Linux (`/dev/shm`, señales); allí `init` se cierra con `taskkill`.

| Sección (Linux) | Qué verifica |
|---|---|
| 1 | Wrapper y validación de argumentos; `reg`/`rep` sin sistema → código 2 |
| 2 | `init` crea `/dev/shm/<nombre>`; segundo `init` rechazado; `reg` por archivo (`.spl`), líneas inválidas, archivo inexistente, modo interactivo, `EVALUATOR_SHM` |
| 3 | `rep -m`, formato `[id i k r]`, `rep` retira los resultados; Ctrl+C libera la memoria |
| 4 | 4 `reg` simultáneos → 120 ids únicos |
| 5 | Cola de entrada llena: `reg` espera; la salida nunca supera `oe`; `list processing/waiting`; orden FIFO |
| 6 | Robot: reactivo insuficiente y recarga (nivel exacto 47); `update` y tope 100; comandos inválidos de `ctrl` |
| 7 | `update` despierta al analizador que esperaba reactivo |
| 8 | Espera máxima: la muestra se reporta `?`; el vigilante avisa |
| 9 | `rep -i` termina por tiempo; dos `rep` simultáneos no comparten resultados |
| 10 | `reg` bloqueado termina al cerrar `init`; detección de `init` muerto (`kill -9`) y recuperación |
