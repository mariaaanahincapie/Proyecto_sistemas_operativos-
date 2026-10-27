# Pruebas

Plan de pruebas para la segunda entrega:

1. `init` con valores por omisión y propios; `init` duplicado.
2. `reg` interactivo y por archivo; `examples/muestras_invalidas.txt`; archivo inexistente; sistema no iniciado.
3. Cola de entrada llena: `reg` espera y continúa al liberarse espacio.
4. Dos o más `reg` simultáneos: ids únicos.
5. Reactivo bajo (`-b 3`): el analizador espera y el robot recarga.
6. Salida llena (`-oe 2`, sin `rep`): los analizadores se bloquean y se reanudan con `rep`.
7. `ctrl list ...` en distintos momentos y `ctrl update`.
8. `rep -m` y `rep -i`, con uno y con varios procesos.
9. Ctrl+C en `init`: no queda memoria compartida huérfana.
