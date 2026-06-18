# Bottleneck Creator

An HPC performance anomaly suite designed to generate datasets through the targeted injection of resource and workload bottlenecks.

It integrates two open-source tools:
- **[HPAS](https://github.com/peaclab/HPAS)** — C/C++ binaries that stress specific hardware subsystems (CPU, cache, memory, I/O, network)
- **[FINJ](https://github.com/AlessioNetti/fault_injector)** — Python orchestrator that launches HPAS commands on target nodes at scheduled times and collects all output

```
[Controller node]            [Engine / target node]
finj_controller.py  ──────►  finj_engine.py  ──────►  hpas <anomaly>
    (schedules tasks)            (executes tasks)        (stresses hardware)
```

The controller reads a workload CSV (timestamps, durations, HPAS commands), distributes tasks to one or more engine daemons over TCP, and records a structured event log for each run.

---

## Prerequisites

| Requirement | Notes |
|-------------|-------|
| Python 3.4+ | Standard library only — no pip dependencies |
| GCC + GNU autotools | `autoconf`, `automake`, `libtool` |
| OpenMP | Optional — required for `loadimb` anomaly |
| OpenMPI with shmem | Optional — required for `netoccupy` anomaly |
| SSE/XMM headers | Optional — required for `membw` anomaly |
| `taskset` | Linux util-linux — used for CPU pinning |

---

## Installation — Build HPAS


**Local build:**
```bash
cd HPAS
./autogen.sh
./configure --prefix="$PWD/../hpas_bin"
make
make install
```

**SLURM build:**
```bash
sbatch --nodelist=<COMPUTE_NODE> scripts/01_build_hpas.sh
```


`./configure` measures cache sizes on the running CPU, so the build must happen on the same node that will execute the anomalies.



---

## Quick Start — Local (two terminals)

### Terminal 1 — Start the engine

```bash
python FINJ/finj_engine.py -c FINJ/config/hpas_engine.config
```

The engine listens on port `30000` and waits for commands from the controller.

### Terminal 2 — Run a probe first

```bash
python FINJ/finj_controller.py \
    -c FINJ/config/hpas_controller.config \
    -w FINJ/workloads/hpas_probe.csv
```

The probe runs each anomaly for 5 seconds. Check `FINJ/results/` and confirm all tasks completed without errors before running the full workload.

### Terminal 2 — Run the full workload

```bash
python FINJ/finj_controller.py \
    -c FINJ/config/hpas_controller.config \
    -w FINJ/workloads/hpas_workload.csv
```

Results appear in `FINJ/results/`.

---

## Cluster Setup (SLURM)

### Step 1 — Build HPAS on a compute node

```bash
sbatch --nodelist=<COMPUTE_NODE> scripts/01_build_hpas.sh
```

### Step 2 — Start one or more engines

```bash
sbatch --nodelist=<NODE> scripts/02_run_engine.sh

# For parallel injection across multiple nodes:
for node in nodeA nodeB nodeC; do
    sbatch --nodelist=$node scripts/02_run_engine.sh
done
```

Engine addresses are automatically written to `scripts/engine_hosts.txt`. Wait until jobs show `R` before proceeding:

```bash
watch squeue --me
```

### Step 3 — Submit the controller

```bash
# Full workload (all anomalies):
sbatch scripts/03_run_controller.sh

# Single anomaly — broken variant:
sbatch scripts/03_run_controller.sh FINJ/workloads/hpas_branchmiss.csv

# Single anomaly — fixed (baseline) variant:
sbatch scripts/03_run_controller.sh FINJ/workloads/hpas_branchmiss_fixed.csv

# Explicit engine address (bypasses engine_hosts.txt):
sbatch scripts/03_run_controller.sh FINJ/workloads/hpas_workload.csv nodeXX:30000
```

---

## Multi-node Configuration

To fan out injection across several nodes, list all engine addresses in the controller config:

```json
// FINJ/config/hpas_controller.config
"HOSTS": ["192.168.1.10:30000", "192.168.1.11:30000"]
```

Or pass them at runtime:

```bash
python FINJ/finj_controller.py \
    -c FINJ/config/hpas_controller.config \
    -w FINJ/workloads/hpas_workload.csv \
    -a 192.168.1.10:30000,192.168.1.11:30000
```

---

## HPAS Anomalies

All commands follow the pattern `hpas <anomaly> [options]`.

Common flags across all anomalies:
- `-p <procs>` — number of worker processes
- `-d <secs>` — self-termination timeout (`-1` = run until killed; FINJ manages termination)
- `-v` — verbose output

> **Note:** When using anomalies via FINJ workload CSVs, **omit `-d`** from the `args` field. FINJ enforces the duration column and kills the process at the right time.

| Anomaly | Subsystem | Key options | Mechanism |
|---------|-----------|-------------|-----------|
| `cpuoccupy` | CPU | `-u <pct>`, `-p <procs>` | Scalar FP dependency chain — low IPC, high CPU time |
| `branchmiss` | CPU | `-p <procs>` | RDRAND-filled array → ~50% branch misprediction rate |
| `loadimb` | CPU (OpenMP) | `-H <high_iters>`, `-l <low_iters>`, `-f <frac>` | Imbalanced thread groups; requires OpenMP |
| `cachecopy` | Cache | `-c L1\|L2\|L3`, `-m <mult>` | Working set = `<mult>` × cache size → constant eviction |
| `membw` | Memory BW | `-s <size>` | Streaming loads/stores via SSE; requires SSE headers |
| `memeater` | Memory | `-s <size>`, `-p <secs>` | Allocates a large working set periodically |
| `memleak` | Memory | `-s <size>`, `-p <secs>` | Allocates and leaks memory at fixed intervals |
| `pipestall` | Memory | `-p <procs>` | Non-temporal stores exhaust write-combining buffers |
| `iometadata` | I/O | `-l <dir>` | High-frequency filesystem metadata operations |
| `iobandwidth` | I/O | (see `hpas iobandwidth --help`) | Storage bandwidth saturation |
| `netoccupy` | Network | `-N 2` | Shared-memory messaging between 2 nodes; requires SHMEM |
| `precwaste` | FP | `-p <procs>` | Scalar double-precision at `-O0`, no vectorization |

`hpas_fixed <anomaly>` runs the patched baseline version of `branchmiss`, `cpuoccupy`, `pipestall`, and `precwaste` — the bottleneck is removed, suitable for paired comparison.

---

## Workload CSV Format

```
timestamp;duration;seqNum;isFault;cores;args
```

| Field | Description |
|-------|-------------|
| `timestamp` | Seconds from session start when the task should begin |
| `duration` | How long FINJ keeps the task alive (0 = run until natural exit) |
| `seqNum` | Unique integer task ID |
| `isFault` | `True` = anomaly injection, `False` = baseline benchmark |
| `cores` | CPU list for `numactl` pinning (e.g., `0-71`), or `None` to disable |
| `args` | Full shell command; use absolute paths for reliability |

**Example row:**
```
60;120;1;True;None;taskset -c 0-71 /home/user/bottleneck_creator/hpas_bin/bin/hpas cpuoccupy -u 80 -p 72
```

---

## Configuration Reference

### Controller (`FINJ/config/hpas_controller.config`)

| Option | Default | Description |
|--------|---------|-------------|
| `HOSTS` | `[]` | Engine addresses; overridden by `-a` flag or `engine_hosts.txt` |
| `PRE_SEND_INTERVAL` | `600` | Send task commands this many seconds before their timestamp |
| `WORKLOAD_PADDING` | `20` | Idle seconds at session start before task execution begins |
| `SESSION_WAIT` | `60` | Seconds to wait for engine acknowledgements |
| `RETRY_INTERVAL` | `600` | How long to attempt reconnection to a lost engine |
| `RETRY_PERIOD` | `30` | Interval between reconnection attempts |

### Engine (`FINJ/config/hpas_engine.config`)

| Option | Default | Description |
|--------|---------|-------------|
| `SERVER_PORT` | `30000` | TCP port the engine listens on |
| `MAX_REQUESTS` | `20` | Worker thread pool size |
| `SKIP_EXPIRED` | `true` | Discard task commands that arrive after their scheduled time |
| `RETRY_TASKS` | `true` | Restart tasks that exit before their duration elapses |
| `RETRY_TASKS_ON_ERROR` | `true` | Retry tasks that exit with a non-zero code |
| `ABRUPT_TASK_KILL` | `true` | Kill running tasks immediately on shutdown |
| `ENABLE_ROOT` | `false` | Allow commands that require sudo |
| `LOG_OUTPUTS` | `true` | Capture task stdout/stderr to files |
| `NUMA_CORES_FAULTS` | `null` | CPU binding override for fault tasks (`null` = disabled) |
| `NUMA_CORES_BENCHMARKS` | `null` | CPU binding override for benchmark tasks |

---

## Output

Results are written to `FINJ/results/` (configurable via `RESULTS_DIR` in controller config).

For each engine host:
- `<hostname>_<port>.csv` — structured event log with timestamps for every task lifecycle event
- One plain-text output file per task (if `LOG_OUTPUTS` is enabled)

**Event types in the CSV log:**

| Type | Meaning |
|------|---------|
| `command_session_s` | Session started |
| `command_session_e` | Session ended |
| `status_start` | Task started |
| `status_end` | Task finished normally |
| `status_restart` | Task restarted (it exited before its duration elapsed) |
| `status_err` | Task exited with an error |
| `detected_lost` | Connection to engine lost |
| `detected_restore` | Connection to engine re-established |

---

## Repository Layout

```
bottleneck_creator/
├── HPAS/                   # C/C++ anomaly source + build system
│   ├── src/                # Anomaly implementations
│   ├── Makefile.am
│   └── configure.ac
├── FINJ/                   # Python orchestrator
│   ├── finj_controller.py  # Controller entry point
│   ├── finj_engine.py      # Engine entry point
│   ├── config/             # JSON configs for controller and engine
│   ├── workloads/          # CSV workload definitions
│   └── results/            # Generated: event logs + task output files
├── scripts/                # SLURM job submission scripts
│   ├── 01_build_hpas.sh    # Build HPAS on a compute node
│   ├── 02_run_engine.sh    # Start a FINJ engine daemon
│   ├── 03_run_controller.sh # Submit the FINJ controller
│   ├── run_with_anomalies.sh # All-in-one local orchestration (no SLURM)
│   └── engine_hosts.txt    # Auto-generated: engine node addresses
├── hpas_bin/               # Generated: installed HPAS binaries
└── logs/                   # Generated: SLURM job output logs
```
