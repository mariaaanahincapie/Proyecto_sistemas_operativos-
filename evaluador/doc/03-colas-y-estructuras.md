# 03. Colas y estructuras

## 1. Memoria compartida (guía §3.2.1)

Una única región creada por `init` con el tamaño **exacto** que piden `-i`, `-ie`, `-oe`, `-q`. Contiene las colas de entrada y de salida **y** los semáforos y variables auxiliares. Linux: `shm_open("/<nombre>")` + `ftruncate` + `mmap`. Windows: `CreateFileMapping("Local\<nombre>")`.

Solo se guardan tipos simples (enteros, `char`, arreglos fijos): nada de `std::string`, `std::vector` ni punteros, porque cada proceso mapea la región en una dirección distinta. Por eso las partes se ubican con **offsets** (desplazamientos desde el inicio) que `init` calcula y guarda en la cabecera.

```
 offset 0          ┌──────────────────────────────┐
                   │ SharedHeader                 │  config, ids, reactivos, robot, tablero, semáforos
 offInRings        ├──────────────────────────────┤
                   │ RingHeader x i               │  una cabecera por cola de entrada
 offInSlots        ├──────────────────────────────┤
                   │ Sample x (i * ie)            │  casillas de las colas de entrada
 offOutRing        ├──────────────────────────────┤
                   │ RingHeader                   │  cabecera de la salida
 offOutSlots       ├──────────────────────────────┤
                   │ Sample x oe                  │  casillas de la salida
 offHolding        ├──────────────────────────────┤
                   │ HoldingSlot x i              │  muestra "en mano" de cada hilo de entrada
 offInternal       ├──────────────────────────────┤
                   │ Sample x (3 * q)             │  espejo de las colas internas (para ctrl)
                   └──────────────────────────────┘   (cada parte alineada a 64 bytes)
```

Ejemplo: con los valores por omisión la región ocupa unos 4 KB (lo imprime `init`).

## 2. Estructuras (src/shared/model.hpp y shm.hpp)

```cpp
struct Sample {                 // viaja por todas las colas
    uint64_t id;                // identificador único (>= 1)
    int64_t  enqueuedAt;        // instante en que reg la aceptó (para la espera máxima)
    int32_t  queue;             // cola de entrada de origen (0..i-1)
    int32_t  qty;               // cantidad (1..5) = unidades de reactivo que gasta
    int32_t  procTime;          // segundos de procesamiento (p)
    char     type;              // 'B', 'D', 'S'
    char     result;            // 'P', 'N', '?'
};

struct RingHeader {             // cola circular productor-consumidor
    int32_t capacity;
    int32_t head;               // próxima casilla a LEER
    int32_t tail;               // próxima casilla a ESCRIBIR
    int32_t count;              // elementos actuales
    SemSlot mutex;              // exclusión mutua (inicia en 1)
    SemSlot empty;              // casillas libres (inicia en capacity)
    SemSlot full;               // casillas ocupadas (inicia en 0)
};

struct SemSlot {                // un semáforo entre procesos
    sem_t    sem;               // (solo Linux) el semáforo vive aquí: sem_init(&sem, 1, v)
    uint32_t index;             // (Windows) número para el nombre "Local\<nombre>_sem<index>"
};

struct SharedHeader {
    uint32_t magic, version;    // "EVAL" v3: valida que el segmento es nuestro
    uint64_t totalSize;
    int64_t  initPid;           // para saber si init sigue vivo
    atomic<int32_t> ready, shuttingDown;
    Config   cfg;               // i, ie, oe, q, reactivos iniciales, robotCap, maxWait
    uint32_t semCount;          // cuántos semáforos (Windows los abre por número)
    uint64_t offInRings, offInSlots, offOutRing, offOutSlots, offHolding, offInternal;

    SemSlot  mutexId;       uint64_t nextId, statRegistered;          // ids únicos
    SemSlot  mutexReactive; int32_t reactive[3], reqPending[3];       // reactivos B, D, S
                            uint64_t reqTicket[3], nextTicket;        // turno FIFO del robot
                            int32_t robotState, robotType, robotLoad;
    SemSlot  robotReq;                                                // pedidos para el robot
    SemSlot  recharged[3];                                            // analizador t espera aquí
    SemSlot  mutexBoard;    Sample processing[3]; int32_t busy[3];    // tablero para ctrl
                            int32_t internalCount[3]; int64_t procStartedAt[3];
                            uint64_t statProcessed, statExpired;
};

struct HoldingSlot { int32_t busy; Sample sample; };   // muestra en tránsito entrada -> interna
```

Total de semáforos: 4 + 3 + 3·i + 3 (con `i = 5`: 25).

## 3. Colas de entrada (guía §3.2.3)

- `i` colas circulares de `ie` posiciones en la memoria compartida.
- **Productores:** procesos `reg` (varios). **Consumidor:** el hilo de entrada `k` de `init`.
- Semáforos por cola: `empty` (= `ie`), `full` (= 0), `mutex` (= 1). El `mutex` es necesario porque hay varios productores a la vez y `ctrl` también lee.

Productor (`reg`, `ringPush`):
```
wait(empty)                       // si la cola está llena, ESPERA (reg se bloquea)
wait(mutex)
    wait(mutexId); id = nextId++; post(mutexId)
    casillas[tail] = muestra(id, ahora); tail = (tail+1) % ie; count++
post(mutex)
post(full)                        // despierta al hilo de entrada
imprimir id  /  escribir id en el .spl
```

Consumidor (hilo de entrada `k`, `ringPop`):
```
wait(full)                        // si está vacía, el hilo duerme
wait(mutex)
    m = casillas[head]; head = (head+1) % ie; count--
    wait(mutexBoard); enMano[k] = m; post(mutexBoard)
post(mutex)
post(empty)                       // libera una casilla para los reg
```

## 4. Cola de salida (guía §3.2.5)

- Circular de `oe` posiciones. **Productores:** los 3 analizadores. **Consumidores:** procesos `rep`.
- Semáforos `empty` (= `oe`), `full` (= 0), `mutex` (= 1). Orden **FIFO**: los resultados salen en el orden en que se depositaron.
- Nunca se añade más allá de la capacidad: si está llena, el analizador espera en `empty` con su resultado en la mano (y sigue figurando en `list processing`). Cuando un `rep` retira un resultado hace `post(empty)` y el analizador continúa.

## 5. Colas internas (guía §3.2.4, nota 3)

- 3 colas (B, D, S) de tamaño `q`, **en la memoria privada de `init`**, compartidas solo entre sus hilos.
- Como no cruzan procesos se protegen con `std::mutex` + `std::condition_variable` (`InternalQueue`): `push` espera si está llena, `front` espera si está vacía.
- Para que `ctrl` (otro proceso) las vea, cada vez que cambian se copia su contenido al **espejo** de la memoria compartida (bajo `mutexBoard`).
- El analizador hace `front` (mira la primera **sin sacarla**) mientras consigue reactivo, y la saca (`popFront`) justo cuando empieza a procesarla, en la misma sección crítica en que la marca como `processing`. Así una muestra siempre aparece en exactamente un lugar.

## 6. Dónde está una muestra en cada momento

| Estado | Lugar | Lo muestra |
|---|---|---|
| Registrada, sin tomar | cola de entrada `k` | `list waiting` |
| Tomada por el hilo de entrada, cola interna llena | `HoldingSlot[k]` | `list waiting` |
| En la cola interna de su tipo (incluso esperando reactivo) | cola interna + espejo | `list waiting` |
| En el analizador | `processing[t]` | `list processing` |
| Terminada, esperando a `rep` | cola de salida | `list reported` |
| Retirada por `rep` | — (impresa por `rep`) | — |
