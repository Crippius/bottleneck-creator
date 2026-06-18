#!/bin/bash
# Sourced by all controller scripts. Resolves engine address and waits for the
# engine port to accept connections before launching finj_controller.py.
# Expects REPO_DIR, FINJ_DIR, WORKLOAD, ENGINE_PORT to be set by caller.

PYTHON_BIN=$(which python3)
HOST_FILE="$REPO_DIR/scripts/engine_hosts.txt"
ENGINE_WAIT_TIMEOUT=${ENGINE_WAIT_TIMEOUT:-300}

# Resolve engine host
if [[ $# -ge 1 && -n "${1:-}" ]]; then
    ADDRESSES="$1"
elif [[ -f "$HOST_FILE" && -s "$HOST_FILE" ]]; then
    ADDRESSES=$(head -1 "$HOST_FILE")
else
    echo "ERROR: engine host not provided and $HOST_FILE is empty." >&2
    echo "Either pass host:port as first argument or run 02_run_engine.sh first." >&2
    exit 1
fi

ENGINE_HOST="${ADDRESSES%%:*}"
ENGINE_PORT_RESOLVED="${ADDRESSES##*:}"

# Wait for engine to be ready
echo "Waiting for engine at ${ADDRESSES} (timeout=${ENGINE_WAIT_TIMEOUT}s)..."
waited=0
while ! nc -z "$ENGINE_HOST" "$ENGINE_PORT_RESOLVED" 2>/dev/null; do
    sleep 2
    waited=$((waited + 2))
    if (( waited >= ENGINE_WAIT_TIMEOUT )); then
        echo "ERROR: engine not reachable after ${ENGINE_WAIT_TIMEOUT}s" >&2
        exit 1
    fi
done
echo "Engine reachable at ${ADDRESSES}."

cd "$FINJ_DIR"
"$PYTHON_BIN" finj_controller.py \
    -c config/hpas_controller.config \
    -w "$WORKLOAD" \
    -a "$ADDRESSES"
