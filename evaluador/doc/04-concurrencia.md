# 04. Concurrencia y sincronización

## 1. Mecanismos usados

| Mecanismo | Dónde | Por qué |
|---|---|---|
| Semáforo contador entre procesos (`empty`/`full`) | colas de entrada y salida | productor-consumidor con buffer acotado; bloquea sin consumir CPU |
| Semáforo binario entre procesos (`mutex*`) | colas, `nextId`, reactivos, tablero | exclusión mutua entre procesos distintos (un `std::mutex` no sirve entre procesos) |
| Semáforo de señalización (`robotReq`, `recharged[t]`) | robot ↔ analizadores, `ctrl update` | despertar a quien espera un evento |
| `std::mutex` + `std::condition_variable` | colas internas | solo los comparten hilos del mismo proceso |
| `std::mutex machine[t]` | analizador ↔ robot | mientras se carga reactivo el analizador no procesa |
| `std::atomic` | `stop`, `ready`, `shuttingDown` | banderas leídas por muchos hilos/procesos sin candado |

Todas las esperas sobre semáforos compartidos usan **tiempo límite** (200 ms) y, al vencer, revisan si hay que abandonar (apagado o `init` muerto). Así ningún proceso queda bloqueado para siempre si `init` desaparece.

## 2. Hilo de entrada `k`

```
mientras no stop:
    m = ringPop(entrada[k])                 // wait(full) ... en la sección crítica: enMano[k] = m
    t = tipo(m)
    interna[t].push(m)                      // si está llena, espera (cv)
        con el candado de la interna:  lock(mutexBoard); espejo[t] = interna[t]; enMano[k] = vacío
```

## 3. Hilo analizador `t` (B, D o S)

```
mientras no stop:
    m = interna[t].front()                  // espera si vacía; NO la saca todavía
    repetir:
        si ahora - m.enqueuedAt > MAX_WAIT: vencida; salir        // no espera para siempre
        lock(machine[t])                    // si el robot está cargando, espera aquí
        lock(mutexReactive)
            si reactive[t] >= m.qty: reactive[t] -= m.qty; ok
            si no y !reqPending[t]: reqPending[t]=1; reqTicket[t]=nextTicket++; pedir
        unlock(mutexReactive)
        si ok: salir (se conserva machine[t])
        unlock(machine[t]); si pedir: post(robotReq)
        wait(recharged[t], 1 s)             // dormido; lo despierta el robot o ctrl update
    p = aleatorio(min_t, max_t)
    interna[t].popFront()  + lock(mutexBoard): espejo, processing[t] = m, busy[t] = 1
    si vencida: m.result = '?' (no gasta reactivo)
    si no:      dormir p segundos; m.result = P/N/? ; unlock(machine[t])
    ringPush(salida, m)                     // wait(empty): si la salida está llena, ESPERA
        en la sección crítica: lock(mutexBoard); busy[t] = 0
```

## 4. Robot (único, guía §2)

Lleva **un solo tipo** de reactivo por viaje, hasta `-ee` unidades (el almacén no se agota). Atiende los pedidos en orden de llegada (`reqTicket` más pequeño).

```
mientras no stop:
    wait(robotReq)                          // dormido hasta que un analizador pida
    lock(mutexReactive); t = pedido con menor ticket; estado = "yendo al almacén"; unlock
    dormir 3 s                              // ida al almacén
    carga = robotCap; estado = "regresando"
    dormir 3 s                              // vuelta al analizador
    lock(machine[t])                        // espera a que el analizador termine su muestra actual
        estado = "cargando"; dormir 1 s     // durante la carga el analizador NO procesa
        lock(mutexReactive)
            reactive[t] += min(carga, 100 - reactive[t]); reqPending[t] = 0; estado = libre
        unlock(mutexReactive)
    unlock(machine[t])
    post(recharged[t])                      // despierta al analizador
```

`ctrl update` suma el reactivo bajo `mutexReactive` y hace `post(recharged[t])`; el analizador vuelve a comprobar con el ciclo `repetir`.

## 5. Tabla de recursos compartidos

| Recurso | Quién accede | Protección |
|---|---|---|
| Cola de entrada `k` | `reg` (escribe), hilo entrada `k` (lee), `ctrl`/vigilante (consultan) | `empty`, `full`, `mutex` de la cola |
| Cola de salida | analizadores (escriben), `rep` (retira), `ctrl` (consulta) | `empty`, `full`, `mutex` de la salida |
| `nextId`, `statRegistered` | `reg` | `mutexId` |
| `reactive[3]`, pedidos, estado del robot | analizadores, robot, `ctrl` | `mutexReactive` |
| Pedido de recarga | analizador → robot | `robotReq` (semáforo contador) |
| Fin de recarga | robot / `ctrl` → analizador | `recharged[t]` |
| Tablero (`processing`, `busy`, en mano, espejo) | hilos de `init`, `ctrl`, vigilante | `mutexBoard` |
| Colas internas | hilos de entrada, analizadores | `std::mutex` + `std::condition_variable` |
| Analizador `t` como máquina | analizador `t`, robot | `std::mutex machine[t]` |

## 6. Prevención de interbloqueo (deadlock)

1. **Orden global de candados.** Siempre se toman en este orden y nunca al revés:
   `machine[t]` → `mutex` de cola de entrada (de menor a mayor `k`) → `mutex` de la cola de salida → candado de una cola interna → `mutexId` → `mutexReactive` → `mutexBoard`.
   Ejemplos: `reg` toma cola → `mutexId`; el hilo de entrada toma cola → `mutexBoard`, y luego interna → `mutexBoard`; el analizador toma `machine` → `mutexReactive`, e interna → `mutexBoard`; `ctrl list waiting` toma todas las colas de entrada en orden creciente y luego `mutexBoard`. Con un orden total no puede formarse un ciclo de espera (se rompe la condición de *espera circular* de Coffman).
2. **No se duerme sosteniendo un mutex.** Primero se espera el contador (`empty`/`full`) y después se toma el `mutex`. Un analizador que espera reactivo suelta `machine[t]` antes de dormir; uno que espera espacio en la salida ya soltó `machine[t]`.
3. **El único recurso que puede faltar (reactivo) siempre se repone:** el robot no depende de nada que tenga un analizador (solo de `machine[t]`, que el analizador suelta al terminar cada muestra).
4. **Secciones críticas cortas**, sin E/S ni esperas dentro.
5. **Señales:** si una espera se interrumpe (`EINTR`) se reintenta.

## 7. Espera acotada (una muestra no espera para siempre — guía §2)

1. **Por diseño:** todas las colas son FIFO y el robot atiende por turno, así que no hay inanición: cada muestra avanza cuando avanzan las de delante.
2. **Red de seguridad:** si al llegar al analizador la muestra ya esperó más de `MAX_WAIT` (120 s, `EVALUATOR_MAX_WAIT`), no se analiza: se reporta `?` ("hay que volver a hacer el examen") y no gasta reactivo. El analizador también revisa esto cada segundo mientras espera reactivo.
3. **Vigilante:** cada 5 s revisa colas de entrada, muestras en mano y colas internas, y avisa en la consola de `init` las que superan la mitad de `MAX_WAIT`.

Única excepción, por decisión del usuario: si nadie ejecuta `rep`, la salida se llena y el sistema se detiene por contrapresión (es lo que pide la guía: no exceder la capacidad de la salida).

## 8. Cierre del sistema

`init` captura Ctrl+C / SIGTERM (el manejador solo pone una bandera atómica). El hilo principal entonces:

1. pone `shuttingDown = 1` en la región (los `reg`/`rep`/`ctrl` que esperan lo ven en ≤ 200 ms y terminan con código 2);
2. pone `stop = true` y despierta a todos los hilos (variables de condición y esperas con tiempo límite);
3. hace `join` de todos los hilos;
4. imprime un resumen y hace `shm_unlink` + `munmap` (Windows: `CloseHandle`).

Los `sem_t` viven dentro de la región: se liberan con ella cuando el último proceso la desmapea (no se llama a `sem_destroy` porque un cliente podría seguir en `sem_timedwait`).

**Caída abrupta (`kill -9`):** los clientes lo detectan porque `kill(initPid, 0)` falla; el siguiente `init` con el mismo nombre ve que el pid guardado ya no existe y borra el segmento huérfano antes de crear uno nuevo.
