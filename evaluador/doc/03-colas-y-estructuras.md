# 03. Colas y estructuras

## 1. Memoria compartida

Una única región creada por `init` (tamaño calculado con `-i`, `-ie`, `-oe`, `-q`). Linux: `shm_open` + `ftruncate` + `mmap`, nombre `/<nombre>`.
Solo se guardan tipos **simples** (sin `std::string` ni `std::vector`), porque otros procesos deben poder leerlos.

```
[ SharedHeader ][ in_q[0..i-1] cabeceras ][ in_slots ... ][ out_q cabecera ][ out_slots ... ]
```
Los **offsets** se calculan en `init` y se guardan en la cabecera.

## 2. Estructuras (C++17)

```cpp
struct Sample {
    uint32_t id;          // identificador único (>= 1)
    uint8_t  queue;       // cola de entrada de origen (0..i-1)
    char     type;        // 'B', 'D', 'S'
    uint8_t  qty;         // cantidad de muestra (1..5)
    uint8_t  proc_time;   // segundos de procesamiento
    char     result;      // 'N', 'P', '?'
    int64_t  t_enqueued;  // instante de ingreso (epoch, segundos)
};

struct RingHeader {       // cola circular productor-consumidor
    uint32_t head;        // próxima posición a LEER
    uint32_t tail;        // próxima posición a ESCRIBIR
    uint32_t count;       // elementos actuales
    sem_t    mutex;       // exclusión sobre head/tail/count
    sem_t    empty;       // espacios libres (inicia = capacidad)
    sem_t    full;        // elementos disponibles (inicia = 0)
};

struct Board {            // tablero de estado (para ctrl)
    Sample  processing[3]; uint8_t busy[3];
    Sample  internal[3][MAX_Q]; uint8_t internal_n[3];  // espejo de colas internas
    Sample  holding[MAX_I];     uint8_t holding_ok[MAX_I];
    sem_t   mutex_board;
};

struct SharedHeader {
    uint32_t magic;       // validar que es nuestro segmento
    pid_t    init_pid;    // detectar si el sistema sigue vivo
    uint32_t n_in, in_cap, out_cap, int_cap, robot_cap;
    volatile uint8_t shutting_down;
    uint32_t next_id;     sem_t mutex_id;
    int32_t  reactive[3]; sem_t mutex_reactive;      // 0=B 1=D 2=S
    sem_t    sem_recharged[3];                       // el analizador espera aquí
    sem_t    robot_req;                              // pedidos pendientes
    uint8_t  req_pending[3];
    Board    board;
};
```
Los `sem_t` se inicializan con `sem_init(&s, 1, valor)` (compartido entre procesos).

## 3. Cola de entrada

- `i` colas circulares de `ie` posiciones.
- **Productores:** procesos `reg` (varios). **Consumidor:** un hilo de entrada por cola.
- Semáforos por cola: `empty` (= `ie`), `full` (= 0), `mutex` (= 1).
- El `mutex` es necesario porque hay varios productores y `ctrl` también lee.

Productor (`reg`):
```
sem_wait(empty)                         // si está llena, ESPERA
sem_wait(mutex)
    sem_wait(mutex_id); id = next_id++; sem_post(mutex_id)
    slots[tail] = muestra; tail=(tail+1)%cap; count++
sem_post(mutex)
sem_post(full)
imprimir id
```

## 4. Cola de salida

- Circular de `oe` posiciones. **Productores:** 3 analizadores. **Consumidores:** procesos `rep`.
- Semáforos `empty` (= `oe`), `full` (= 0), `mutex` (= 1). Orden FIFO.
- Nunca se añade más allá de la capacidad: si está llena, el analizador espera en `empty` con su resultado en la mano.

## 5. Colas internas

- 3 colas (B, D, S), tamaño `q`, **solo en memoria de `init`**, compartidas entre sus hilos.
- Protegidas con `std::mutex` + semáforos locales (`std::counting_semaphore`).
- Cada vez que cambian, el hilo publica una copia en `Board.internal` (bajo `mutex_board`) para que `ctrl list waiting` las vea.
