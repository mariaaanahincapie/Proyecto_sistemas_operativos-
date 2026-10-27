# 04. Concurrencia y sincronización

## 1. Procesos e hilos

| Quién | Tipo | Cantidad |
|---|---|---|
| `reg` | proceso | varios |
| `ctrl` | proceso | varios |
| `rep` | proceso | varios |
| `init` | proceso | 1 |
| hilo de entrada | hilo de `init` | `i` |
| hilo analizador (B, D, S) | hilo de `init` | 3 |
| hilo robot | hilo de `init` | 1 |
| hilo vigilante | hilo de `init` | 1 |

## 2. Hilo de entrada `k`

```
mientras no shutting_down:
    sem_wait(in_q[k].full)
    sem_wait(in_q[k].mutex)
        s = slots[head]; head=(head+1)%cap; count--
    sem_post(in_q[k].mutex)
    sem_post(in_q[k].empty)               // libera espacio para reg
    board.holding[k] = s                  // "la tengo en la mano"
    t = tipo(s)
    esperar espacio en interna[t]         // si está llena, espera
    push(interna[t], s); publicar_espejo(t)
    board.holding[k] = vacío
```

## 3. Hilo analizador `t` (B, D o S)

```
mientras no shutting_down:
    s = pop(interna[t]) (espera si vacía); publicar_espejo(t)

    si (ahora - s.t_enqueued) > MAX_WAIT:
        s.result = '?'; ir a DEPOSITAR

    mientras reactive[t] < s.qty:         // bajo mutex_reactive
        pedir_robot(t)
        sem_wait(sem_recharged[t])        // dormido: no procesa mientras se recarga

    reactive[t] -= s.qty                  // bajo mutex_reactive
    s.proc_time = aleatorio(min_t, max_t)
    board.processing[t] = s; busy[t] = 1
    sleep(s.proc_time)
    s.result = resultado_aleatorio()
    busy[t] = 0

  DEPOSITAR:
    sem_wait(out_q.empty)                 // si la salida está llena, ESPERA
    sem_wait(out_q.mutex); push(out_q, s); sem_post(out_q.mutex)
    sem_post(out_q.full)
```

## 4. Robot de recarga

Un único hilo. Lleva una sola clase de reactivo a la vez, hasta `robot_cap` unidades.

Pedido del analizador `t`:
```
si !req_pending[t]: req_pending[t] = 1; sem_post(robot_req)
```
Hilo robot:
```
mientras no shutting_down:
    sem_wait(robot_req)
    t = siguiente_pedido_FIFO()
    sleep(ROBOT_TRIP)                     // al almacén (3 s)
    carga = robot_cap                     // almacén infinito
    sleep(ROBOT_TRIP)                     // al analizador (3 s)
    lock(mutex_reactive)
        reactive[t] += min(carga, 100 - reactive[t]); req_pending[t] = 0
    unlock(mutex_reactive)
    sem_post(sem_recharged[t])
```
Mientras se recarga, el analizador está dormido en `sem_recharged[t]`: no procesa otra muestra. `ctrl update` también hace `sem_post(sem_recharged[t])`; el analizador vuelve a comprobar con el `mientras`.

## 5. Tabla de control

| Recurso | Quién accede | Mecanismo |
|---|---|---|
| Cola de entrada `k` | reg, hilo entrada `k`, ctrl | `empty`, `full`, `mutex` |
| Cola de salida | analizadores, rep, ctrl | `empty`, `full`, `mutex` |
| `next_id` | reg | `mutex_id` |
| `reactive[3]` | analizadores, robot, ctrl | `mutex_reactive` |
| Recarga pendiente | analizador ↔ robot | `robot_req`, `sem_recharged[t]` |
| Tablero de estado | hilos init, ctrl | `mutex_board` |
| Colas internas | hilos entrada, analizadores | `std::mutex` + semáforos locales |

## 6. Prevención de interbloqueo (deadlock)

- **Orden fijo de candados:** mutex de cola → `mutex_id` → `mutex_reactive` → `mutex_board`.
- **Regla:** nunca se duerme esperando `empty`/`full` mientras se tiene un mutex. Primero se espera el contador y después se toma el mutex.
- Los hilos que esperan no sostienen mutex. El único recurso que puede faltar (reactivo) lo repone el robot, que no depende de nada que tenga un analizador.
- Toda llamada `sem_wait` se reintenta si una señal la interrumpe (`EINTR`).

## 7. Espera acotada (una muestra no espera para siempre)

1. **Por diseño:** colas FIFO, semáforos que despiertan en orden, robot que atiende en orden y reactivo siempre reponible: no hay inanición.
2. **Red de seguridad:** si al llegar al analizador la muestra esperó más de `MAX_WAIT` (120 s), no se analiza: se reporta `?` y no gasta reactivo.
3. **Vigilante:** revisa cada 5 s y escribe una advertencia en la consola de `init`.

## 8. Cierre del sistema

`init` captura `SIGINT`: pone `shutting_down = 1`, hace `sem_post` en todos los semáforos donde haya hilos dormidos, espera los hilos (`join`), destruye semáforos, `munmap` y `shm_unlink`. Así no queda memoria compartida huérfana.
