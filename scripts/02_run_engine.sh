#!/bin/bash
#SBATCH --job-name=finj_engine
#SBATCH --partition=ALL
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=72
#SBATCH --time=00:15:00
#SBATCH --output=logs/finj_engine_%j.log
#SBATCH --error=logs/finj_engine_%j.log

# ---------------------------------------------------------------------------
# Start a FINJ engine on a compute node.
#
# The engine listens on port 30000 and executes HPAS commands sent by the
# controller.  After the job starts, the compute node hostname:port is written
# to scripts/engine_hosts.txt so that controller scripts can connect
# automatically.
#
# Usage:
#   sbatch --nodelist=itp01 scripts/02_run_engine.sh
#
# To start multiple engines on separate nodes (multi-node injection):
#   for i in 1 2 3; do sbatch scripts/02_run_engine.sh; done
# ---------------------------------------------------------------------------

set -uo pipefail

REPO_DIR=/home/tcrippa/bottleneck_creator
FINJ_DIR=$REPO_DIR/FINJ
ENGINE_PORT=30000

# ---- Prepare directories needed by HPAS anomalies ------------------------
mkdir -p /tmp/hpas_tmp

# ---- Record hostname:port so the controller can find this engine ----------
NODE_HOST=$(hostname)
HOST_FILE="$REPO_DIR/scripts/engine_hosts.txt"

# Keep one entry per host to avoid duplicate controller targets
if [[ ! -f "$HOST_FILE" ]] || ! grep -Fxq "${NODE_HOST}:${ENGINE_PORT}" "$HOST_FILE"; then
    echo "${NODE_HOST}:${ENGINE_PORT}" >> "$HOST_FILE"
fi

echo "========================================"
echo "FINJ engine starting"
echo "  Node      : ${NODE_HOST}"
echo "  Port      : ${ENGINE_PORT}"
echo "  HPAS bin  : $REPO_DIR/hpas_bin/bin/hpas"
echo "  Timestamp : $(date)"
echo "========================================"

PYTHON_BIN=$(command -v python3 || command -v python || true)
if [[ -z "$PYTHON_BIN" ]]; then
    echo "ERROR: Python interpreter not found."
    exit 127
fi

cd "$FINJ_DIR"
"$PYTHON_BIN" finj_engine.py -c config/hpas_engine.config

echo "Engine exited at $(date)"
