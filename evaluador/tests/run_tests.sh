#!/usr/bin/env bash
# Pruebas automáticas del evaluador (Linux).   Uso:  make test   o   bash tests/run_tests.sh
# Se usa EVALUATOR_SPEED=20 para que la simulación corra 20 veces más rápido
# (solo acelera las esperas reales; los tiempos reportados siguen siendo los de la guía).
set -u
cd "$(dirname "$0")/.."
BIN="${BIN:-bin/evaluator}"
NAME="prueba_$$"
TMP="$(mktemp -d)"
export EVALUATOR_SPEED=20
PASS=0; FAIL=0; INIT_PID=""

ok()    { echo "  [OK]    $1"; PASS=$((PASS+1)); }
fail()  { echo "  [FALLA] $1"; FAIL=$((FAIL+1)); }
check() { if eval "$2"; then ok "$1"; else fail "$1"; fi; }

start() { "$BIN" init -n "$NAME" "$@" > "$TMP/init.log" 2>&1 & INIT_PID=$!; sleep 0.5; }
stop()  { if [ -n "$INIT_PID" ]; then kill -INT "$INIT_PID" 2>/dev/null; wait "$INIT_PID" 2>/dev/null; INIT_PID=""; fi; }
ctrl()  { printf '%s\n' "$@" | "$BIN" ctrl -s "$NAME"; }
level() { ctrl "list reactive" | awk -v t="$1" '$1==t {print $2}'; }
trap 'stop; rm -rf "$TMP"' EXIT

[ -x "$BIN" ] || { echo "No existe $BIN; ejecute make"; exit 1; }

echo "1. Envoltorio y validación de argumentos"
"$BIN" >/dev/null 2>&1;                 check "sin comando -> código 1" '[ $? -eq 1 ]'
"$BIN" borrar >/dev/null 2>&1;          check "comando desconocido -> código 1" '[ $? -eq 1 ]'
"$BIN" init -n "$NAME" -i 0 >/dev/null 2>&1;   check "init -i 0 rechazado (código 1)" '[ $? -eq 1 ]'
"$BIN" init -n "$NAME" -b 101 >/dev/null 2>&1; check "init -b 101 rechazado (máximo 100)" '[ $? -eq 1 ]'
"$BIN" init -n "$NAME" -x 3 >/dev/null 2>&1;   check "init con opción desconocida rechazado" '[ $? -eq 1 ]'
"$BIN" reg -n "$NAME" - </dev/null >/dev/null 2>&1; check "reg sin sistema iniciado -> código 2" '[ $? -eq 2 ]'
"$BIN" rep -s "$NAME" -m 1 >/dev/null 2>&1;    check "rep sin sistema iniciado -> código 2" '[ $? -eq 2 ]'
"$BIN" rep -s "$NAME" >/dev/null 2>&1;         check "rep sin -i ni -m -> código 1" '[ $? -eq 1 ]'

echo "2. init y registro por archivo"
start -i 2 -ie 4 -oe 6
check "init crea la memoria compartida" '[ -e /dev/shm/$NAME ]'
"$BIN" init -n "$NAME" > "$TMP/dup.out" 2>&1
check "segundo init con el mismo nombre se rechaza" '[ $? -eq 1 ] && grep -q "ya está iniciado" "$TMP/dup.out"'
printf '0 B 1\n9 B 1\n# comentario\n\n0 X 1\n1 B 6\n1 D 2\n0 S\n' > "$TMP/m.txt"
"$BIN" reg -n "$NAME" "$TMP/m.txt" > "$TMP/reg.out" 2> "$TMP/reg.err"; RC=$?
check "crea m.spl con los ids de las 2 líneas válidas" '[ "$(tr "\n" " " < "$TMP/m.spl")" = "1 2 " ]'
check "rechaza 4 líneas inválidas con mensaje (cola, tipo, cantidad, formato)" '[ "$(wc -l < "$TMP/reg.err")" -eq 4 ]'
check "con líneas inválidas el código es 1" '[ $RC -eq 1 ]'
"$BIN" reg -n "$NAME" "$TMP/no_existe.txt" > "$TMP/ne.out" 2>&1; RC=$?
check "archivo inexistente se ignora sin mensaje" '[ $RC -eq 0 ] && [ ! -s "$TMP/ne.out" ]'
echo "0 B 2" | EVALUATOR_SHM="$NAME" "$BIN" reg - > "$TMP/i.out"
check "modo interactivo imprime el id (3); EVALUATOR_SHM funciona" '[ "$(cat "$TMP/i.out")" = "3" ]'

echo "3. Reporte"
"$BIN" rep -s "$NAME" -m 3 > "$TMP/rep.out"
check "rep -m 3 entrega 3 resultados" '[ "$(wc -l < "$TMP/rep.out")" -eq 3 ]'
check "formato [id i k r] con r en P, N o ?" '[ "$(grep -Ec "^\[[0-9]+ [0-9]+ [BDS] [PN?]\]$" "$TMP/rep.out")" -eq 3 ]'
check "rep retira los resultados (list reported queda vacío)" '[ "$(ctrl "list reported" | grep -c "^\[")" -eq 0 ]'
stop
check "Ctrl+C libera la memoria compartida" '[ ! -e /dev/shm/$NAME ]'
check "init muestra el resumen al cerrar" 'grep -q "registradas=3" "$TMP/init.log"'

echo "4. Varios reg simultáneos: ids únicos"
start -i 4 -ie 64 -oe 10
for k in 0 1 2 3; do yes "$k B 1" | head -n 30 | "$BIN" reg -n "$NAME" - > "$TMP/ids$k.txt" & done
wait $(jobs -p | grep -v "^$INIT_PID$") 2>/dev/null
cat "$TMP"/ids?.txt > "$TMP/all.txt"
check "120 ids registrados" '[ "$(wc -l < "$TMP/all.txt")" -eq 120 ]'
check "ningún id repetido" '[ "$(sort -u "$TMP/all.txt" | wc -l)" -eq 120 ]'
stop

echo "5. Colas llenas: reg espera, la salida nunca supera su capacidad"
start -i 1 -ie 2 -q 1 -oe 1
( yes "0 B 1" | head -n 8 | "$BIN" reg -n "$NAME" - > "$TMP/full.out" ) & REGPID=$!
sleep 2
check "reg sigue bloqueado (cola de entrada llena)" 'kill -0 $REGPID 2>/dev/null'
check "aceptó menos de 8 muestras mientras está bloqueado" '[ "$(wc -l < "$TMP/full.out")" -lt 8 ]'
check "list reported nunca pasa de oe=1" '[ "$(ctrl "list reported" | grep -c "^\[")" -le 1 ]'
check "list processing muestra el analizador ocupado" '[ "$(ctrl "list processing" | grep -c "^\[")" -eq 1 ]'
check "list waiting muestra las muestras en espera" '[ "$(ctrl "list waiting" | grep -c "^\[")" -ge 2 ]'
"$BIN" rep -s "$NAME" -m 8 > "$TMP/rep5.out"
wait $REGPID
check "al retirar resultados reg termina y salen las 8" '[ "$(wc -l < "$TMP/full.out")" -eq 8 ] && [ "$(wc -l < "$TMP/rep5.out")" -eq 8 ]'
check "los resultados salen en orden FIFO (1..8)" '[ "$(tr -d "[" < "$TMP/rep5.out" | cut -d" " -f1 | tr "\n" " ")" = "1 2 3 4 5 6 7 8 " ]'
stop

echo "6. Reactivo, robot y update"
start -i 1 -b 3 -ee 50
printf '0 B 2\n0 B 2\n0 B 2\n' | "$BIN" reg -n "$NAME" - > /dev/null
"$BIN" rep -s "$NAME" -m 3 > "$TMP/rep6.out"
check "las 3 muestras terminan aunque solo había reactivo para 1" '[ "$(wc -l < "$TMP/rep6.out")" -eq 3 ]'
check "nivel B final = 3 - 2 + 50 - 2 - 2 = 47" '[ "$(level B)" = "47" ]'
check "el robot fue al almacén y cargó el analizador" 'grep -q "robot.*va al almacén por reactivo B" "$TMP/init.log" && grep -q "entregó 50 unidades de B" "$TMP/init.log"'
ctrl "update B 10" > /dev/null
check "update B 10 suma al nivel actual (57)" '[ "$(level B)" = "57" ]'
ctrl "update D 30" > /dev/null
check "update no supera el máximo 100" '[ "$(level D)" = "100" ]'
ctrl "update B -5" "update X 5" "update B" "foo" "list nada" > /dev/null 2> "$TMP/ctrl.err"
check "ctrl rechaza 5 comandos inválidos con 'error:'" '[ "$(grep -c "^error:" "$TMP/ctrl.err")" -eq 5 ]'
stop

echo "7. update despierta al analizador que espera reactivo"
start -i 1 -b 0 -ee 1
echo "0 S 5" | "$BIN" reg -n "$NAME" - > /dev/null
sleep 0.5
ctrl "update S 20" > /dev/null
"$BIN" rep -s "$NAME" -m 1 > "$TMP/rep7.out"
check "la muestra se procesa tras update" '[ "$(wc -l < "$TMP/rep7.out")" -eq 1 ]'
stop

echo "8. Espera máxima: una muestra no espera para siempre"
EVALUATOR_SPEED=1 EVALUATOR_MAX_WAIT=8 "$BIN" init -n "$NAME" -i 1 -b 0 -ee 1 > "$TMP/init.log" 2>&1 & INIT_PID=$!; sleep 0.5
echo "0 B 5" | "$BIN" reg -n "$NAME" - > /dev/null
EVALUATOR_SPEED=1 "$BIN" rep -s "$NAME" -m 1 > "$TMP/rep8.out"
check "sin reactivo a tiempo se reporta '?'" 'grep -q " B ?\]" "$TMP/rep8.out"'
check "el vigilante avisó la espera excesiva" 'grep -q "vigilante.*esperando" "$TMP/init.log"'
stop

echo "9. rep -i y dos rep a la vez"
start -i 2
T0=$(date +%s); "$BIN" rep -s "$NAME" -i 2 > /dev/null; T1=$(date +%s)
check "rep -i 2 vuelve en unos 2 s sin resultados" '[ $((T1-T0)) -ge 2 ] && [ $((T1-T0)) -le 3 ]'
printf '0 B 1\n1 B 1\n0 B 1\n1 B 1\n0 B 1\n1 B 1\n' | "$BIN" reg -n "$NAME" - > /dev/null
"$BIN" rep -s "$NAME" -m 3 > "$TMP/r1.out" & P1=$!
"$BIN" rep -s "$NAME" -m 3 > "$TMP/r2.out" & P2=$!
wait $P1 $P2
check "cada resultado lo recibe un solo rep (6 ids distintos)" '[ "$(cat "$TMP/r1.out" "$TMP/r2.out" | sort -u | wc -l)" -eq 6 ]'
stop

echo "10. Cierre del sistema y recuperación"
start -i 1 -ie 1 -q 1 -oe 1
( yes "0 D 1" | head -n 10 | "$BIN" reg -n "$NAME" - > /dev/null 2>&1; echo $? > "$TMP/regrc" ) &
sleep 1; stop; sleep 0.5
check "reg bloqueado termina con código 2 al detener init" '[ "$(cat "$TMP/regrc")" = "2" ]'
start
kill -9 "$INIT_PID"; wait "$INIT_PID" 2>/dev/null; INIT_PID=""
"$BIN" reg -n "$NAME" - </dev/null >/dev/null 2>&1
check "reg detecta que init murió (código 2)" '[ $? -eq 2 ]'
start
check "init arranca de nuevo tras una caída (limpia el segmento huérfano)" 'grep -q "listo" "$TMP/init.log"'
stop

echo
echo "Resultado: $PASS correctas, $FAIL con falla"
[ "$FAIL" -eq 0 ]
