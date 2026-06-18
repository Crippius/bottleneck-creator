#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <xmmintrin.h>
#include <getopt.h>
#include <stdbool.h>
#include "src/utils.h"

/*
 * Pipeline-stall anomaly via write-combining (WC) buffer exhaustion.
 *
 * The inner loop reads orig[dim*m + i] sequentially (stride-1, prefetchable)
 * and streams swap[dim*i + m] non-temporally with a stride of dim elements.
 * Because consecutive streaming stores target cache lines that are dim*8 bytes
 * apart, they land in different WC buffer slots.  Ice Lake has ~12 WC slots;
 * with dim >> 12 the buffer fills instantly and the pipeline stalls waiting for
 * DRAM write-combine flushes.  Result: high stall_rate and high CPI — same
 * mechanism as membw, which produces PIPELINE_STALL=0.5.
 *
 * Workers allocate their own private arrays (post-fork) so each process drives
 * an independent DRAM stream and there is no copy-on-write overhead.
 */

#define DEFAULT_PIPESTALL_DIM 4096  /* 4096 × 4096 × 8B = 128 MB per array */

static struct option const long_opt[] =
{
    {"help",     no_argument,       NULL, 'h'},
    {"verbose",  no_argument,       NULL, 'v'},
    {"dim",      required_argument, NULL, 'n'},
    {"duration", required_argument, NULL, 'd'},
    {"start",    required_argument, NULL, 't'},
    {"rate",     required_argument, NULL, 'r'},
    {"procs",    required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}
};

static const char *pipestall_usage = "Pipeline Stall Anomaly (WC-buffer exhaustion).\n\n"
    "-n, --dim (=4096)       One dimension of the 2-D work arrays (dim × dim doubles).\n"
    "-d, --duration (=-1.0)  The total duration (in seconds), -1 for infinite.\n"
    "-t, --start (=0.0)      The time to wait (in seconds) before starting the anomaly.\n"
    "-r, --rate (=0)         Sleep between sweeps in ms; 0 = full speed.\n"
    "-p, --procs (=1)        Number of worker processes to run simultaneously.\n"
    "-v, --verbose           Prints execution information.\n"
    "-h, --help              Prints this message.\n";

/* Non-temporal transposed copy: sequential reads, large-stride streaming writes.
 * Streaming writes bypass L3 and land in write-combine buffers; when dim > ~12
 * the WC buffer fills every inner-loop iteration → DRAM-flush stalls dominate. */
static void do_stall(const double *orig, double *swap, size_t dim) {
    for (size_t m = 0; m < dim; m++) {
        for (size_t i = 0; i < dim; i++) {
            _mm_stream_pi(
                (__m64 *)(&swap[dim * i + m]),
                *(__m64 *)(&orig[dim * m + i]));
            _mm_empty();
        }
    }
}

static void pipestall_worker(size_t dim, int sleep_ms, bool verbose) {
    double *orig = malloc(dim * dim * sizeof(double));
    double *swap = malloc(dim * dim * sizeof(double));
    if (!orig || !swap) { perror("pipestall_worker malloc"); _exit(1); }

    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    for (size_t k = 0; k < dim * dim; k++)
        swap[k] = (double)rand();

    int counter = 0;
    while (timer_flag) {
        do_stall(orig, swap, dim);
        if (sleep_ms > 0)
            hpas_sleep(sleep_ms / 1000.0);
        if (verbose && ++counter % 10 == 0) {
            printf(".");
            fflush(stdout);
        }
    }
    free(orig);
    free(swap);
}

static void on_sigterm(int sig) {
    (void)sig;
    signal(SIGTERM, SIG_IGN);
    kill(0, SIGTERM);
    while (wait(NULL) > 0);
    exit(0);
}

int pipestall(int argc, char *argv[]) {
    size_t dim      = DEFAULT_PIPESTALL_DIM;
    double duration = -1.0;
    double start_time = 0.0;
    int    sleep_ms = 0;
    int    nprocs   = 1;
    bool   verbose  = false;
    int    opt;

    while ((opt = getopt_long(argc, argv, "hvn:d:t:r:p:", long_opt, NULL)) != -1) {
        switch (opt) {
            case 'n': dim       = (size_t)atol(optarg);    break;
            case 'd': duration  = strtod(optarg, NULL);    break;
            case 't': start_time = strtod(optarg, NULL);   break;
            case 'r': sleep_ms  = atoi(optarg);            break;
            case 'p': nprocs    = atoi(optarg);
                      if (nprocs < 1) { fprintf(stderr, "procs must be >= 1\n"); return 1; }
                      break;
            case 'v': verbose   = true;                    break;
            case 'h':
            default:
                printf("Usage: hpas pipestall [OPTIONS]\n%s", pipestall_usage);
                return 0;
        }
    }

    hpas_sleep(start_time);

    printf("Starting pipestall: dim=%zu (%zu MB/array), procs=%d, duration=%.1f\n",
           dim, (dim * dim * sizeof(double)) / (1024 * 1024), nprocs, duration);
    fflush(stdout);

    setpgid(0, 0);
    signal(SIGTERM, on_sigterm);

    pid_t *pids = NULL;
    if (nprocs > 1) {
        pids = malloc((nprocs - 1) * sizeof(pid_t));
        for (int i = 0; i < nprocs - 1; i++) {
            pid_t pid = fork();
            if (pid == 0) {
                signal(SIGTERM, SIG_DFL);
                set_duration(duration);
                pipestall_worker(dim, sleep_ms, false);
                _exit(0);
            }
            pids[i] = pid;
        }
    }

    set_duration(duration);
    pipestall_worker(dim, sleep_ms, verbose);

    if (pids) {
        for (int i = 0; i < nprocs - 1; i++) {
            kill(pids[i], SIGTERM);
            waitpid(pids[i], NULL, 0);
        }
        free(pids);
    }

    printf("\nExiting pipestall.\n");
    return 0;
}
