#!/usr/bin/env bash
# Regression test for: multiple `listen` directives in a single server
# block must all bind, and each Listener must use its block's config.
#
# default.conf: block 1 listens on 8080+8081 (root ./www), block 2 on
# 8082 (root ./www2). The original bug bound only ports[0], so 8081
# was silently dropped.

cd "$(dirname "$0")/.." || exit 2

CONF="config/default.conf"
LOG="/tmp/ws.log"
RE=":(8080|8081|8082)\b"
FAILS=0

pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAILS=$((FAILS + 1)); }

cleanup() {
    [ -n "${WS_PID:-}" ] && kill "$WS_PID" 2>/dev/null
    wait "$WS_PID" 2>/dev/null
}
trap cleanup EXIT

# Wait until `ss` shows exactly $1 of our ports bound, up to $2 * 0.1s.
wait_ports() {
    for _ in $(seq 1 "$2"); do
        [ "$(ss -tln 2>/dev/null | grep -cE "$RE")" -eq "$1" ] && return 0
        sleep 0.1
    done
    return 1
}

# 1. Free the ports (kill any stale webserv, fail if something else holds them)
pkill -f "bin/webserv" 2>/dev/null
if ! wait_ports 0 30; then
    echo "ABORT: ports held by a non-webserv process:"
    ss -tlnp 2>/dev/null | grep -E "$RE"
    exit 2
fi

# 2. Start webserv, wait for all 3 binds
./bin/webserv "$CONF" > "$LOG" 2>&1 &
WS_PID=$!
if ! wait_ports 3 50; then
    fail "only $(ss -tln | grep -cE "$RE")/3 ports bound after 5s"
    cat "$LOG"; exit 1
fi
pass "all 3 ports bound"

# 3. Every socket must be owned by *our* PID (catches stale-process false greens)
owners=$(ss -tlnp 2>/dev/null | grep -E "$RE" | grep -oE 'pid=[0-9]+' | sort -u)
[ "$owners" = "pid=$WS_PID" ] \
    && pass "all 3 sockets owned by pid $WS_PID" \
    || fail "expected only pid=$WS_PID, got: $owners"

# 4. HTTP probes — capture status and body in one curl call
fetch() { curl -s --max-time 3 -w $'\n%{http_code}' "http://127.0.0.1:$1/"; }
for p in 8080 8081 8082; do
    r=$(fetch "$p")
    eval "code_$p=\${r##*$'\\n'}; body_$p=\${r%$'\\n'*}"
done

[ "$code_8080" = "200" ] && pass "GET :8080/ -> 200" || fail "GET :8080/ -> '$code_8080'"
[ "$code_8081" = "200" ] && pass "GET :8081/ -> 200" || fail "GET :8081/ -> '$code_8081'"

# 5. Routing proofs: 8080 == 8081 (same block), 8082 differs (separate block)
[ -n "$body_8080" ] && [ "$body_8080" = "$body_8081" ] \
    && pass ":8080 and :8081 returned identical bodies (block 1 shared)" \
    || fail ":8080 and :8081 bodies differ — 8081 not routed via block 1"

[ "$body_8080" != "$body_8082" ] \
    && pass ":8082 body differs from :8080 (block 2 isolated)" \
    || fail ":8082 body equals :8080 — blocks not isolated"

echo
if [ "$FAILS" -eq 0 ]; then
    echo "All checks passed."
else
    echo "$FAILS check(s) failed."
    echo "--- webserv log ---"; cat "$LOG"
    exit 1
fi
