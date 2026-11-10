#!/usr/bin/env bash
# Pruebas automáticas para Windows, desde MSYS2 (UCRT64) o Git Bash.
#   Uso:  make test     (en MSYS2 elige este script)   o   bash tests/run_tests_windows.sh
#
# Igual que tests/run_tests.sh pero sin lo que solo existe en Linux (/dev/shm, SIGINT).
# Los programas de Windows escriben fin de línea \r\n: se quita con `nocr` antes de comparar.
set -u
cd "$(dirname "$0")/.."
BIN="${BIN:-./bin/evaluator.exe}"
NAME="prueba_$$"
TMP="$(mktemp -d)"
export EVALUATOR_SPEED=20
PASS=0; FAIL=0; INIT_PID=""

ok()    { echo "  [OK]    $1"; PASS=$((PASS+1)); }
fail()  { echo "  [FALLA] $1"; FAIL=$((FAIL+1)); }
check() { if eval "$2"; then ok "$1"; else fail "$1"; fi; }
nocr()  { tr -d '\r'; }

start() { "$BIN" init -n "$NAME" "$@" > "$TMP/init.log" 2>&1 & INIT_PID=$!; sleep 1; }
# En Windows no hay SIGINT entre procesos: se termina init a la fuerza con taskkill /T
# (el wrapper evaluator.exe lanza evaluator-init.exe como hijo; /T cierra los dos).
# El sistema operativo libera la memoria compartida y los semáforos al cerrarse los handles.
stop()  {
  if [ -n "$INIT_PID" ]; then
    local winpid
    winpid=$(cat /proc/"$INIT_PID"/winpid 2>/dev/null)
    if [ -n "$winpid" ]; then taskkill //F //T //PID "$winpid" >/dev/null 2>&1; else kill "$INIT_PID" 2>/dev/null; fi
    wait "$INIT_PID" 2>/dev/null
    INIT_PID=""; sleep 1
  fi
}
ctrl()  { printf '%s\n' "$@" | "$BIN" ctrl -s "$NAME" | nocr; }
level() { ctrl "list reactive" | awk -v t="$1" '$1==t {print $2}'; }
trap 'stop; rm -rf "$TMP"' EXIT

[ -x "$BIN" ] || { echo "No existe $BIN; ejecute make"; exit 1; }

echo "1. Envoltorio y validación de argumentos"
"$BIN" >/dev/null 2>&1;                 check "sin comando -> código 1" '[ $? -eq 1 ]'
"$BIN" borrar >/dev/null 2>&1;          check "comando desconocido -> código 1" '[ $? -eq 1 ]'
"$BIN" init -n "$NAME" -i 0 >/dev/null 2>&1;   check "init -i 0 rechazado (código 1)" '[ $? -eq 1 ]'
"$BIN" init -n "$NAME" -b 101 >/dev/null 2>&1; check "init -b 101 rechazado" '[ $? -eq 1 ]'
"$BIN" reg -n "$NAME" - </dev/null >/dev/null 2>&1; check "reg sin sistema iniciado -> código 2" '[ $? -eq 2 ]'
"$BIN" rep -s "$NAME" -m 1 >/dev/null 2>&1;    check "rep sin sistema iniciado -> código 2" '[ $? -eq 2 ]'

echo "2. init y registro"
start -i 2 -ie 4 -oe 6
check "init arrancó" 'grep -q "listo" "$TMP/init.log"'
"$BIN" init -n "$NAME" > "$TMP/dup.out" 2>&1
check "segundo init con el mismo nombre se rechaza" '[ $? -eq 1 ]'
printf '0 B 1\n9 B 1\n# comentario\n\n0 X 1\n1 B 6\n1 D 2\n0 S\n' > "$TMP/m.txt"
"$BIN" reg -n "$NAME" "$TMP/m.txt" > /dev/null 2> "$TMP/reg.err"; RC=$?
check "crea m.spl con los ids 1 y 2" '[ "$(nocr < "$TMP/m.spl" | tr "\n" " ")" = "1 2 " ]'
check "rechaza 4 líneas inválidas con mensaje" '[ "$(nocr < "$TMP/reg.err" | grep -c "ignorada")" -eq 4 ]'
check "con líneas inválidas el código es 1" '[ $RC -eq 1 ]'
"$BIN" reg -n "$NAME" "$TMP/no_existe.txt" > "$TMP/ne.out" 2>&1; RC=$?
check "archivo inexistente se ignora sin mensaje" '[ $RC -eq 0 ] && [ ! -s "$TMP/ne.out" ]'
echo "0 B 2" | "$BIN" reg -n "$NAME" - | nocr > "$TMP/i.out"
check "modo interactivo imprime el id (3)" '[ "$(cat "$TMP/i.out")" = "3" ]'

echo "3. Reporte"
"$BIN" rep -s "$NAME" -m 3 | nocr > "$TMP/rep.out"
check "rep -m 3 entrega 3 resultados" '[ "$(wc -l < "$TMP/rep.out")" -eq 3 ]'
check "formato [id i k r]" '[ "$(grep -Ec "^\[[0-9]+ [0-9]+ [BDS] [PN?]\]$" "$TMP/rep.out")" -eq 3 ]'
check "rep retira los resultados" '[ "$(ctrl "list reported" | grep -c "^\[")" -eq 0 ]'
stop

echo "4. Varios reg simultáneos: ids únicos"
start -i 4 -ie 64 -oe 10
PIDS=""
for k in 0 1 2 3; do
  ( yes "$k B 1" | head -n 30 | "$BIN" reg -n "$NAME" - | nocr > "$TMP/ids$k.txt" ) & PIDS="$PIDS $!"
done
wait $PIDS
cat "$TMP"/ids?.txt > "$TMP/all.txt"
check "120 ids registrados" '[ "$(wc -l < "$TMP/all.txt")" -eq 120 ]'
check "ningún id repetido" '[ "$(sort -u "$TMP/all.txt" | wc -l)" -eq 120 ]'
stop

echo "5. Colas llenas"
start -i 1 -ie 2 -q 1 -oe 1
( yes "0 B 1" | head -n 8 | "$BIN" reg -n "$NAME" - | nocr > "$TMP/full.out" ) & REGPID=$!
sleep 3
check "reg aceptó menos de 8 (está esperando espacio)" '[ "$(wc -l < "$TMP/full.out")" -lt 8 ]'
check "list reported nunca pasa de oe=1" '[ "$(ctrl "list reported" | grep -c "^\[")" -le 1 ]'
check "list waiting muestra muestras en espera" '[ "$(ctrl "list waiting" | grep -c "^\[")" -ge 2 ]'
"$BIN" rep -s "$NAME" -m 8 | nocr > "$TMP/rep5.out"
wait $REGPID
check "al retirar resultados salen las 8" '[ "$(wc -l < "$TMP/full.out")" -eq 8 ] && [ "$(wc -l < "$TMP/rep5.out")" -eq 8 ]'
check "orden FIFO (1..8)" '[ "$(tr -d "[" < "$TMP/rep5.out" | cut -d" " -f1 | tr "\n" " ")" = "1 2 3 4 5 6 7 8 " ]'
stop

echo "6. Reactivo, robot y update"
start -i 1 -b 3 -ee 50
printf '0 B 2\n0 B 2\n0 B 2\n' | "$BIN" reg -n "$NAME" - > /dev/null
"$BIN" rep -s "$NAME" -m 3 | nocr > "$TMP/rep6.out"
check "las 3 muestras terminan" '[ "$(wc -l < "$TMP/rep6.out")" -eq 3 ]'
check "nivel B final = 47" '[ "$(level B)" = "47" ]'
check "el robot trabajó" 'grep -q "entregó 50 unidades de B" "$TMP/init.log"'
ctrl "update B 10" > /dev/null
check "update B 10 -> 57" '[ "$(level B)" = "57" ]'
ctrl "update D 30" > /dev/null
check "update con tope 100" '[ "$(level D)" = "100" ]'
stop

echo "7. rep -i y dos rep a la vez"
start -i 2
T0=$(date +%s); "$BIN" rep -s "$NAME" -i 2 > /dev/null; T1=$(date +%s)
check "rep -i 2 vuelve en unos 2 s" '[ $((T1-T0)) -ge 2 ] && [ $((T1-T0)) -le 4 ]'
printf '0 B 1\n1 B 1\n0 B 1\n1 B 1\n0 B 1\n1 B 1\n' | "$BIN" reg -n "$NAME" - > /dev/null
( "$BIN" rep -s "$NAME" -m 3 | nocr > "$TMP/r1.out" ) & P1=$!
( "$BIN" rep -s "$NAME" -m 3 | nocr > "$TMP/r2.out" ) & P2=$!
wait $P1 $P2
check "cada resultado lo recibe un solo rep" '[ "$(cat "$TMP/r1.out" "$TMP/r2.out" | sort -u | wc -l)" -eq 6 ]'
stop

echo "8. init caído"
start
stop
"$BIN" reg -n "$NAME" - </dev/null >/dev/null 2>&1
check "tras cerrar init, reg da código 2" '[ $? -eq 2 ]'
start
check "init arranca de nuevo con el mismo nombre" 'grep -q "listo" "$TMP/init.log"'
stop

echo
echo "Resultado: $PASS correctas, $FAIL con falla"
[ "$FAIL" -eq 0 ]
