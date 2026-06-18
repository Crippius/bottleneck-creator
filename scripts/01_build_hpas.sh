#!/bin/bash
#SBATCH --job-name=hpas_build
#SBATCH --partition=ALL
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --time=00:30:00
#SBATCH --output=logs/hpas_build_%j.log
#SBATCH --error=logs/hpas_build_%j.log

# ---------------------------------------------------------------------------
# Build hpas (broken) and hpas_fixed (patched) from the HPAS source tree.
#
# IMPORTANT: HPAS uses x86-specific intrinsics (_mm_stream_pi, _rdrand64_step).
# This job MUST run on an x86_64 node (itp*), not on AArch64 (taishan*).
#
# Usage:
#   sbatch --nodelist=itp01 scripts/01_build_hpas.sh
#
# Binaries are installed to hpas_bin/bin/hpas and hpas_bin/bin/hpas_fixed.
# ---------------------------------------------------------------------------


REPO_DIR=/home/tcrippa/bottleneck_creator
HPAS_DIR=$REPO_DIR/HPAS
PREFIX=$REPO_DIR/hpas_bin

# ---- Modules --------------------------------------------------------------
module purge
module load comp/gcc/14.2.0

echo "========================================"
echo "HPAS build starting"
echo "  Node      : $(hostname)"
echo "  Arch      : $(uname -m)"
echo "  Partition : ${SLURM_JOB_PARTITION}"
echo "  GCC       : $(gcc --version | head -1)"
echo "  Timestamp : $(date)"
echo "========================================"

# ---- Skip build if binaries are already up to date -----------------------
if [[ -f "$PREFIX/bin/hpas" && -f "$PREFIX/bin/hpas_fixed" ]] && \
   find "$HPAS_DIR/src" -name "*.c" -newer "$PREFIX/bin/hpas" | grep -qv .; then
    echo "--- Binaries are up-to-date, skipping build ---"
    echo "  $PREFIX/bin/hpas"
    echo "  $PREFIX/bin/hpas_fixed"
    exit 0
fi

# ---- Build ----------------------------------------------------------------
cd "$HPAS_DIR"

echo "--- Running autogen.sh ---"
./autogen.sh

echo "--- Running configure ---"
make distclean 2>/dev/null || true
./configure --prefix="$PREFIX"

echo "--- Building ($(nproc) cores) ---"
make -j"$(nproc)"

echo "--- Installing ---"
make install

echo "========================================"
echo "HPAS build finished at $(date)"
echo "  $PREFIX/bin/hpas"
echo "  $PREFIX/bin/hpas_fixed"
echo "========================================"
