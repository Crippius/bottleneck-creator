#!/bin/bash
#SBATCH --job-name=hpas_run
#SBATCH --partition=ALL
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=72
#SBATCH --time=01:00:00
#SBATCH --output=logs/hpas_run_%j.log
#SBATCH --error=logs/hpas_run_%j.log

set -euo pipefail

REPO_DIR=/home/tcrippa/bottleneck_creator
FINJ_DIR=$REPO_DIR/FINJ
WORKLOAD=${WORKLOAD:-workloads/hpas_workload.csv}
APP_SCRIPT=${APP_SCRIPT:-/home/tcrippa/miniapps/scripts/run_cloverleaf.sh}
ENGINE_PORT=30000
PYTHON_BIN=$(which python3)

NODE_HOST=$(hostname)

# Start engine in background
cd "$FINJ_DIR"
"$PYTHON_BIN" finj_engine.py -c config/hpas_engine.config &
ENGINE_PID=$!

# Start the application in background
bash "$APP_SCRIPT" &
APP_PID=$!

# Wait for engine port to open
echo "Waiting for engine on port ${ENGINE_PORT}..."
for i in $(seq 1 60); do
    nc -z "$NODE_HOST" "$ENGINE_PORT" 2>/dev/null && break
    sleep 2
done

# Launch controller
"$PYTHON_BIN" finj_controller.py \
    -c config/hpas_controller.config \
    -w "$WORKLOAD" \
    -a "${NODE_HOST}:${ENGINE_PORT}"

# Wait for application to finish
wait "$APP_PID" || true
kill "$ENGINE_PID" 2>/dev/null || true
wait "$ENGINE_PID" 2>/dev/null || true

echo "Done."
