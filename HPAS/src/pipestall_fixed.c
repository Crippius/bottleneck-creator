#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <getopt.h>
#include <stdbool.h>
#include "src/utils.h"



#define DEFAULT_PIPESTALL_DIM 2048

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

static const char *pipestall_usage = "Pipeline Stall Anomaly (FIXED - compute-bound with prefetch).\n\n"
    "-n, --dim (=2048)       One dimension of the 2-D work arrays (dim × dim doubles).\n"
    "-d, --duration (=-1.0)  The total duration (in seconds), -1 for infinite.\n"
    "-t, --start (=0.0)      The time to wait (in seconds) before starting the anomaly.\n"
    "-r, --rate (=0)         Sleep between sweeps in ms; 0 = full speed.\n"
    "-p, --procs (=1)        Number of worker processes to run simultaneously.\n"
    "-v, --verbose           Prints execution information.\n"
    "-h, --help              Prints this message.\n";

#pragma GCC push_options
#pragma GCC optimize("O3,tree-vectorize")
#pragma GCC target("avx2,fma")
static void do_compute(const double *orig, double *swap, size_t dim) {
    size_t n = dim * dim;
    for (size_t i = 0; i < n; i++) {
        if (i + 64 < n)
            __builtin_prefetch(&orig[i + 64], 0, 0);
        double v = orig[i];
        v = v * 1.0000001 + 1.0e-10;
        v = v * 1.0000001 + 1.0e-10;
        v = v * 1.0000001 + 1.0e-10;
        v = v * 1.0000001 + 1.0e-10;
        swap[i] = v;
    }
}
#pragma GCC pop_options

static void pipestall_worker(size_t dim, int sleep_ms, bool verbose) {
    double *orig = malloc(dim * dim * sizeof(double));
    double *swap = malloc(dim * dim * sizeof(double));
    if (!orig || !swap) { perror("pipestall_worker malloc"); _exit(1); }

    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    for (size_t k = 0; k < dim * dim; k++)
        orig[k] = (double)rand();

    int counter = 0;
    while (timer_flag) {
        do_compute(orig, swap, dim);
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
    size_t dim        = DEFAULT_PIPESTALL_DIM;
    double duration   = -1.0;
    double start_time = 0.0;
    int    sleep_ms   = 0;
    int    nprocs     = 1;
    bool   verbose    = false;
    int    opt;

    while ((opt = getopt_long(argc, argv, "hvn:d:t:r:p:", long_opt, NULL)) != -1) {
        switch (opt) {
            case 'n': dim        = (size_t)atol(optarg);   break;
            case 'd': duration   = strtod(optarg, NULL);   break;
            case 't': start_time = strtod(optarg, NULL);   break;
            case 'r': sleep_ms   = atoi(optarg);           break;
            case 'p': nprocs     = atoi(optarg);
                      if (nprocs < 1) { fprintf(stderr, "procs must be >= 1\n"); return 1; }
                      break;
            case 'v': verbose    = true;                   break;
            case 'h':
            default:
                printf("Usage: hpas_fixed pipestall [OPTIONS]\n%s", pipestall_usage);
                return 0;
        }
    }

    hpas_sleep(start_time);

    printf("Starting pipestall (FIXED): dim=%zu (%zu MB/array), procs=%d, duration=%.1f\n",
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

    printf("\nExiting pipestall (FIXED).\n");
    return 0;
}

