# 07. Guía de sustentación

Resumen para explicar el proyecto de principio a fin y responder preguntas. Cada respuesta indica dónde está en el código.

---

## Parte A. La idea en 1 minuto

"Es un laboratorio simulado. Varios procesos `reg` meten muestras en **colas de entrada** que están en **memoria compartida**. El proceso `init` tiene un **hilo por cola de entrada** que saca las muestras y las manda a una **cola interna** según su tipo (B, D o S). Hay **un hilo analizador por tipo** que gasta reactivo, 'procesa' durmiendo un tiempo aleatorio, genera un resultado aleatorio y lo deja en la **cola de salida** compartida. Los procesos `rep` retiran los resultados. `ctrl` deja ver todo y recargar reactivos. Cuando un analizador se queda sin reactivo, un **robot único** va al almacén y lo recarga. Todo se coordina con **semáforos**: productor-consumidor en cada cola y mutex para las variables compartidas."

## Parte B. Recorrido de una muestra (contarlo con el diagrama de doc/01)

1. `evaluator reg -` → el wrapper hace `execv` de `evaluator-reg`.
2. `reg` abre la región (`shm_open` + `mmap`), valida la línea `0 B 2`.
3. `wait(empty)` de la cola 0 → si está llena se bloquea aquí.
4. `wait(mutex)` de la cola 0 → toma `mutexId`, `id = nextId++` → escribe en `casillas[tail]` → `post(mutex)` → `post(full)` → imprime el id.
5. El hilo de entrada 0 estaba dormido en `wait(full)`; despierta, saca la muestra (bajo el mutex de la cola), la marca "en mano", `post(empty)`.
6. La pone en la cola interna B (`std::mutex` + `condition_variable`); si está llena, espera.
7. El analizador B mira la primera de su cola interna, toma `machine[B]` y `mutexReactive`, resta `q` unidades. Si no alcanza, pide el robot y duerme en `recharged[B]`.
8. La saca de la cola interna, la marca en `processing[B]`, duerme `p` segundos (1..7), sortea P/N/?.
9. `wait(empty)` de la salida → si está llena espera → escribe bajo el mutex → `post(full)`.
10. `rep -m 1` estaba en `wait(full)`; despierta, retira el resultado bajo el mutex, `post(empty)`, imprime `[id 0 B P]`.

## Parte C. Preguntas frecuentes

### Diseño general

**¿Por qué procesos e hilos y no solo hilos?**
La guía exige que `reg`, `ctrl` y `rep` sean comandos independientes que se ejecutan en otras terminales (procesos distintos) y que el evaluador use hilos internos. Entre procesos se comparte con memoria compartida y semáforos; entre hilos, con la memoria del proceso y `std::mutex`.

**¿Qué es el wrapper y para qué sirve?** (`src/main/evaluator.cpp`)
Es el único programa que usa el usuario. Mira el primer argumento y ejecuta `evaluator-init/reg/ctrl/rep` con el resto de argumentos. En Linux usa `execv`, que reemplaza la imagen del proceso: mismo pid, misma terminal, y Ctrl+C llega directo al programa real. En Windows usa `_spawnv` con `_P_WAIT` y devuelve el código del hijo.

**¿Dónde busca el wrapper los programas reales?**
En `EVALUATOR_BIN` o, si no existe, en su propia carpeta (`/proc/self/exe` en Linux, `GetModuleFileName` en Windows). Así funciona instalado en cualquier directorio.

**¿Qué hace `init` exactamente?** (`src/evaluator/init.cpp`)
Valida argumentos, calcula el tamaño de la región, la crea (o detecta que ya hay un sistema vivo), inicializa cabecera, colas, reactivos y 25 semáforos (con `i=5`), marca `ready=1`, crea los hilos y se queda en la consola. Con Ctrl+C hace el apagado ordenado.

### Memoria compartida

**¿Cómo se crea?** (`src/shared/shm.cpp`)
Linux: `shm_open("/evaluator", O_CREAT|O_EXCL|O_RDWR)`, `ftruncate(tamaño)`, `mmap(MAP_SHARED)`. Queda visible como `/dev/shm/evaluator`. Windows: `CreateFileMapping(INVALID_HANDLE_VALUE, ..., "Local\evaluator")` + `MapViewOfFile`.

**¿Por qué `O_EXCL`?**
Para que la creación sea atómica: si dos `init` arrancan a la vez, solo uno la crea; el otro recibe `EEXIST` y revisa si el dueño sigue vivo.

**¿Cómo saben los demás procesos dónde está cada cola si el tamaño cambia?**
`init` calcula **offsets** y los guarda en la cabecera. Cada proceso suma el offset a la dirección donde él mapeó la región. No se guardan punteros porque cada proceso mapea en una dirección distinta.

**¿Por qué no `std::string` ni `std::vector` en la región?**
Guardan punteros a memoria del heap del proceso que los creó; en otro proceso esos punteros no valen nada.

**¿Cómo sabe `reg` que la región está lista y es la correcta?**
Comprueba `magic = "EVAL"`, `version = 3` y `ready = 1` (atómico, se pone al final de `init`).

**¿Cuánto ocupa?**
Depende de `-i -ie -oe -q`; con los valores por omisión unos 4 KB. `init` lo imprime al arrancar.

### Semáforos y sincronización

**¿Qué semáforos hay y para qué?** (doc/04 §5)
Por cada cola (entrada y salida): `mutex` (exclusión), `empty` (casillas libres), `full` (casillas ocupadas). Además `mutexId` (id único), `mutexReactive` (reactivos y robot), `mutexBoard` (tablero que lee ctrl), `robotReq` (pedidos al robot) y `recharged[3]` (avisar al analizador que ya hay reactivo).

**¿Por qué tres semáforos por cola y no uno?**
`empty` y `full` cuentan y bloquean: el productor duerme si no hay espacio, el consumidor si no hay elementos, sin espera activa. El `mutex` protege `head/tail/count` porque hay varios productores (`reg`) y lectores (`ctrl`) al mismo tiempo.

**¿Por qué primero `wait(empty)` y luego `wait(mutex)`? ¿Qué pasa al revés?**
Al revés hay interbloqueo: un `reg` tomaría el mutex con la cola llena y se dormiría en `empty` sosteniéndolo; el consumidor nunca podría tomar el mutex para sacar una muestra y hacer `post(empty)`.

**¿Dónde están los semáforos?**
En Linux **dentro de la región** (`sem_t` inicializado con `sem_init(&s, 1, valor)`; el `1` es *pshared*: compartido entre procesos). En Windows un semáforo del kernel no puede vivir en memoria, así que se crea con nombre `Local\evaluator_sem<k>` y en la región solo se guarda `k`. Ambos casos se esconden detrás de `SemSlot` + `SemContext` (`src/concurrency/semaphore.*`).

**¿Por qué no usar `std::mutex` entre procesos?**
`std::mutex` solo garantiza exclusión entre hilos de un mismo proceso. Entre procesos se necesitan objetos del sistema operativo compartidos (semáforos POSIX pshared o semáforos con nombre de Windows).

**¿Y dentro de init sí se usan `std::mutex`?**
Sí, en las colas internas (con `condition_variable`) y en `machine[t]`, porque solo las usan hilos del mismo proceso. Es más barato y es lo que dice la guía (nota 3: la cola interna no se comparte con procesos externos).

**¿Por qué las esperas tienen tiempo límite de 200 ms?**
Para que nadie quede bloqueado para siempre si `init` se cierra o muere: al vencer cada intervalo revisan `shuttingDown` y si el pid de `init` sigue vivo. Mientras tanto no consumen CPU (es una espera bloqueante, no un ciclo ocupado).

**¿Cómo se garantiza que los ids son únicos con varios `reg`?**
`nextId` está en la región y solo se lee-incrementa con `mutexId` tomado, dentro de la sección crítica de la cola. Prueba (4): 4 `reg` simultáneos × 30 muestras = 120 ids distintos.

**¿Hay condiciones de carrera en `ctrl list`?**
No: cada lista se lee con su candado. `list waiting` toma todos los mutex de entrada en orden y luego `mutexBoard`, así obtiene una foto consistente: una muestra que pasa de la cola de entrada a la mano del hilo, o de la mano a la cola interna, cambia de lugar dentro de una misma sección crítica, por lo que nunca se ve dos veces ni desaparece.

### Interbloqueo e inanición

**¿Cómo evitan el deadlock?** (doc/04 §6)
1) Orden global fijo de candados (máquina → colas de entrada → salida → interna → `mutexId` → `mutexReactive` → `mutexBoard`): sin ciclos de espera. 2) Nunca se duerme sosteniendo un mutex (primero el contador, luego el mutex; el analizador suelta `machine[t]` antes de esperar reactivo o espacio en la salida). 3) El reactivo siempre se puede reponer: el robot no necesita nada que tenga un analizador ocupado esperando.

**¿Cuáles son las 4 condiciones de Coffman y cuál rompen?**
Exclusión mutua, retención y espera, no expropiación, espera circular. Se rompe la **espera circular** con el orden global, y en buena parte la **retención y espera** (no se espera un evento sosteniendo un mutex).

**¿Una muestra puede esperar para siempre?**
No. Las colas son FIFO y el robot atiende por turno (no hay inanición). Además, si al llegar al analizador lleva más de `MAX_WAIT` (120 s) se reporta `?` sin gastar reactivo, y el vigilante avisa en la consola las que pasan de la mitad. Prueba (8).

**¿Y si nadie ejecuta `rep`?**
La salida se llena, los analizadores se quedan esperando con su resultado y por contrapresión se llenan las internas y las de entrada; `reg` espera. Es lo correcto: la guía prohíbe exceder la capacidad de la salida. Al ejecutar `rep` todo continúa (prueba 5).

### Robot y reactivos

**¿Cómo funciona el robot?** (`robotWorker`)
Duerme en `robotReq`. Cuando un analizador no tiene reactivo suficiente marca `reqPending[t]` con un turno (`reqTicket`) y hace `post(robotReq)` (una sola vez por tipo). El robot elige el pedido más antiguo, va al almacén (3 s), vuelve con `-ee` unidades (3 s), toma `machine[t]`, carga (1 s), suma hasta el tope 100, libera y hace `post(recharged[t])`.

**¿Cómo garantizan que mientras se carga el analizador no procesa?**
Con `std::mutex machine[t]`: el analizador lo tiene mientras procesa una muestra y el robot lo necesita para cargar. Si el analizador está procesando, el robot espera a que termine esa muestra; mientras el robot carga, el analizador no puede empezar otra.

**¿Por qué el robot solo lleva un tipo?**
Así lo pide la guía; por eso hay un solo hilo robot y atiende un pedido (un tipo) por viaje. Si B y S piden a la vez, va primero por el que pidió antes.

**¿Qué hace `update`?** (`ctrl.cpp`)
Suma la cantidad bajo `mutexReactive` (tope 100) y hace `post(recharged[t])` para que un analizador que esperaba vuelva a revisar. Prueba (7).

**¿Cuánto reactivo gasta una muestra?**
Su cantidad `q` (1..5). Ejemplo verificado (6): `-b 3 -ee 50` y tres muestras `B 2`: 3−2 = 1 → no alcanza → robot +50 = 51 → −2 −2 = 47.

### Procesamiento y resultados

**¿Cómo se generan tiempos y resultados?** (`evaluator.cpp`)
`std::uniform_int_distribution` sobre un `std::mt19937` por hilo: tiempo en [1,7], [5,20] o [8,25]; para el resultado un entero en [0,99]: <50 P, <85 N, resto ?.

**¿Por qué un generador por hilo?**
`std::mt19937` no es seguro para hilos; uno por hilo evita candados y carreras.

**¿Qué significa `?`/`-`?**
No concluyente: hay que repetir el examen. `ctrl` lo muestra como `-` (y `positive`/`negative` para los otros, §3.1.3); `rep` como `?` (`P`/`N`, §3.2.6).

**¿En qué orden salen los resultados?**
FIFO de la cola de salida: en el orden en que los analizadores terminaron. Por eso una sangre registrada después puede salir antes que una piel (dura menos).

### Comandos

**¿Qué diferencia hay entre `list reported` y `rep`?**
`list reported` solo **mira** la cola de salida (no retira). `rep` **retira** (consume) los resultados y libera la casilla.

**¿Qué pasa si dos `rep` corren a la vez?**
Cada uno hace `wait(full)` + mutex: cada resultado lo recibe exactamente uno. Prueba (9).

**¿`rep -i 10` qué hace?**
Durante 10 s imprime cada resultado apenas sale; a los 10 s termina aunque no haya llegado nada.

**¿Qué pasa si registro en una cola que no existe o con cantidad 7?**
Mensaje en `stderr` con archivo y línea, se omite esa línea y se continúa; el código final es 1.

**¿Qué pasa si el archivo no existe?**
Se ignora sin mensaje, como dice la guía.

**¿Qué es el `.spl`?**
Archivo con el mismo nombre y extensión `.spl` con los ids de las muestras aceptadas, uno por línea.

### Ciclo de vida

**¿Qué pasa al hacer Ctrl+C en `init`?**
El manejador solo pone una bandera atómica (lo único seguro dentro de una señal). El hilo principal pone `shuttingDown=1` en la región (los clientes terminan con código 2), detiene y despierta los hilos, hace `join`, imprime el resumen y borra la región (`shm_unlink`). No queda nada en `/dev/shm`.

**¿Y si `init` muere con `kill -9`?**
Los clientes lo detectan con `kill(pid, 0)` y terminan. El siguiente `init` encuentra el segmento, ve que el pid guardado ya no existe y lo borra antes de crear uno nuevo. Prueba (10).

**¿Pueden correr dos sistemas a la vez?**
Sí, con nombres distintos: `init -n lab1` y `init -n lab2`; luego `reg -n lab2`, `ctrl -s lab2`, `rep -s lab2`.

### Windows

**¿Qué cambia en Windows?**
Solo la capa de plataforma: `CreateFileMapping/MapViewOfFile` en vez de `shm_open/mmap`, semáforos con nombre (`CreateSemaphore/OpenSemaphore`, `WaitForSingleObject`) en vez de `sem_t`, `SetConsoleCtrlHandler` para Ctrl+C, `OpenProcess` para saber si `init` vive y `_spawnv` en el wrapper. Los hilos, colas y la lógica son el mismo código C++17.

**¿Por qué en Windows no se borra la región al salir?**
Los objetos con nombre de Windows se destruyen solos cuando el último proceso cierra su handle.

## Parte D. Demostración sugerida (5 minutos)

```sh
make
export EVALUATOR_SPEED=2                         # opcional: todo el doble de rápido
# T1
bin/evaluator init -i 3 -ie 3 -oe 3 -b 6 -ee 30
# T2
bin/evaluator reg examples/muestras_mixtas.txt ; cat examples/muestras_mixtas.spl
bin/evaluator reg examples/muestras_invalidas.txt   # ver los rechazos
# T3
bin/evaluator ctrl
> list all          # waiting / processing / robot pidiendo B
> list reactive
> update B 20
# T4
bin/evaluator rep -i 20
# Volver a T1 y mostrar el registro (robot, analizadores) y Ctrl+C -> resumen, ls /dev/shm
```

Puntos a señalar: el robot sale cuando B no alcanza (−b 6); con `-oe 3` y sin `rep` la salida no pasa de 3 (`list reported`); `rep` libera y todo continúa; `make test` corre las 43 pruebas.
