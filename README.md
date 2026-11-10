# Simulador de Evaluación de Exámenes Médicos

**Práctica de Sistemas Operativos — ST0257, 2026-2**

| | |
|---|---|
| Lenguaje | C++17 |
| Sistemas operativos | Linux y Windows |
| Integrantes | Mariana Hincapié Henao · Martin Caballero |
| Estado | Segunda entrega: implementación completa (`init`, `reg`, `ctrl`, `rep`), 43 pruebas automáticas |

Simulación de un sistema **paralelo** de exámenes médicos: varios procesos registran muestras en colas de entrada que viven en **memoria compartida**; el proceso evaluador las despacha con **hilos** a analizadores especializados (sangre, detritos, piel) que consumen **reactivos** recargados por un **robot único**; los resultados quedan en una cola de salida hasta que se solicitan.

> Los resultados son aleatorios y no tienen validez clínica.

## Inicio rápido (Linux)

```sh
cd evaluador
make                                   # compila en evaluador/bin/
bin/evaluator init                     # terminal 1 (Ctrl+C para detener)
bin/evaluator reg examples/carga.txt   # terminal 2
bin/evaluator ctrl                     # terminal 3: list all, update B 20, Ctrl+D
bin/evaluator rep -i 30                # terminal 4
```

## Contenido

Todo el proyecto está en [`evaluador/`](evaluador/), con la jerarquía que pide la guía:

| | |
|---|---|
| [evaluador/Readme.md](evaluador/Readme.md) | descripción, herramientas, uso |
| [evaluador/Install.md](evaluador/Install.md) | compilación en Linux y Windows e instalación en otro directorio |
| [evaluador/doc/](evaluador/doc/) | diseño: requisitos, arquitectura, comandos, colas, concurrencia, procesamiento, decisiones, guía de sustentación |
| [evaluador/src/](evaluador/src/) | código fuente |
| [evaluador/examples/](evaluador/examples/) | archivos de muestras |
| [evaluador/tests/](evaluador/tests/) | pruebas automáticas |
