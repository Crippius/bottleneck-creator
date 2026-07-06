#!/bin/bash
#SBATCH --job-name=finj_ctrl
#SBATCH --partition=ALL
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --time=00:30:00
#SBATCH --output=logs/finj_ctrl_%j.log
#SBATCH --error=logs/finj_ctrl_%j.log

# ---------------------------------------------------------------------------
# Run the FINJ controller with a configurable workload.
#
# Usage:
#   sbatch scripts/03_run_controller.sh [WORKLOAD_CSV [ENGINE_HOST:PORT]]
#
# WORKLOAD_CSV     Path to workload CSV (absolute, or relative to repo root).
#                  Defaults to FINJ/workloads/hpas_workload.csv.
# ENGINE_HOST:PORT Engine address. Falls back to scripts/engine_hosts.txt.
#
# Examples:
#   sbatch scripts/03_run_controller.sh
#   sbatch scripts/03_run_controller.sh FINJ/workloads/hpas_branchmiss.csv
#   sbatch scripts/03_run_controller.sh FINJ/workloads/hpas_branchmiss_fixed.csv itp01:30000
# ---------------------------------------------------------------------------

set -uo pipefail

# ---- Paths ----------------------------------------------------------------
REPO_DIR=/home/tcrippa/bottleneck_creator
FINJ_DIR=$REPO_DIR/FINJ
ENGINE_WAIT_TIMEOUT=300

# ---- Workload -------------------------------------------------------------
_WORKLOAD_ARG="${1:-}"
if [[ -z "$_WORKLOAD_ARG" ]]; then
    WORKLOAD="$REPO_DIR/FINJ/workloads/hpas_workload_fixed.csv"
elif [[ "$_WORKLOAD_ARG" = /* ]]; then
    WORKLOAD="$_WORKLOAD_ARG"
else
    WORKLOAD="$REPO_DIR/$_WORKLOAD_ARG"
fi

echo "========================================"
echo "FINJ controller starting"
echo "  Node      : $(hostname)"
echo "  Workload  : ${WORKLOAD}"
echo "  Timestamp : $(date)"
echo "========================================"

source "$REPO_DIR/scripts/_controller_common.sh" "${2:-}"
