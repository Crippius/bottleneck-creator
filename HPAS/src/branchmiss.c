#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <getopt.h>
#include <stdbool.h>
#include <immintrin.h>
#include "src/utils.h"

#define DEFAULT_BRANCHMISS_SIZE  (64 * 1024 * 1024)

static struct option const long_opt[] =
{
    {"help",     no_argument,       NULL, 'h'},
    {"verbose",  no_argument,       NULL, 'v'},
    {"size",     required_argument, NULL, 's'},
    {"duration", required_argument, NULL, 'd'},
    {"start",    required_argument, NULL, 't'},
    {"rate",     required_argument, NULL, 'r'},
    {"procs",    required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}
};

static const char *branchmiss_usage = "Branch Misprediction Anomaly.\n\n"
    "-s, --size (=67108864)  Size of each working array in bytes.\n"
    "-d, --duration (=-1.0)  The total duration (in seconds), -1 for infinite.\n"
    "-t, --start (=0.0)      The time to wait (in seconds) before starting the anomaly.\n"
    "-r, --rate (=0)         Sleep between iterations in ms; 0 = full speed.\n"
    "-p, --procs (=1)        Number of worker processes to run simultaneously.\n"
    "-v, --verbose           Prints execution information.\n"
    "-h, --help              Prints this message.\n";


#pragma GCC push_options
#pragma GCC optimize("no-tree-vectorize,no-if-conversion")
static void do_branch_work(double *a, double *b, double *c, double *d, int n) {
    unsigned long long rv = 0;

    int i;
    for (i = 0; i < n - 1; i += 2) {
        while (!_rdrand64_step(&rv));
        c[i]     = (double)(int32_t)(rv >> 32)          * (1.0 / 2147483648.0);
        c[i + 1] = (double)(int32_t)(rv & 0xFFFFFFFFu)  * (1.0 / 2147483648.0);
    }
    if (i < n) {
        while (!_rdrand64_step(&rv));
        c[i] = (double)(int32_t)(rv >> 32) * (1.0 / 2147483648.0);
    }
    volatile double sink = 0.0;
    for (i = 0; i < n; i++) {
        if (c[i] < 0.0)
            a[i] = b[i] - c[i] * d[i];
        else
            a[i] = b[i] + c[i] * d[i];
    }
    sink = a[n >> 1];
    (void)sink;
}
#pragma GCC pop_options

static void branchmiss_worker(double *a, double *b, double *c, double *d,
                               int n, int sleep_ms, bool verbose) {
    int counter = 0;
    while (timer_flag) {
        do_branch_work(a, b, c, d, n);
        if (sleep_ms > 0)
            hpas_sleep(sleep_ms / 1000.0);
        if (verbose && ++counter % 100 == 0) {
            printf(".");
            fflush(stdout);
        }
    }
}

static void on_sigterm(int sig) {
    (void)sig;
    signal(SIGTERM, SIG_IGN);
    kill(0, SIGTERM);
    while (wait(NULL) > 0);
    exit(0);
}

int branchmiss(int argc, char *argv[]) {
    long   array_bytes = DEFAULT_BRANCHMISS_SIZE;
    double duration    = -1.0;
    double start_time  = 0.0;
    int    sleep_ms    = 0;
    int    nprocs      = 1;
    bool   verbose     = false;
    int    opt;

    while ((opt = getopt_long(argc, argv, "hvs:d:t:r:p:", long_opt, NULL)) != -1) {
        switch (opt) {
            case 's': array_bytes = atol(optarg);          break;
            case 'd': duration    = strtod(optarg, NULL);  break;
            case 't': start_time  = strtod(optarg, NULL);  break;
            case 'r': sleep_ms    = atoi(optarg);          break;
            case 'p': nprocs      = atoi(optarg);
                      if (nprocs < 1) { fprintf(stderr, "procs must be >= 1\n"); return 1; }
                      break;
            case 'v': verbose     = true;                  break;
            case 'h':
            default:
                printf("Usage: hpas branchmiss [OPTIONS]\n%s", branchmiss_usage);
                return 0;
        }
    }

    hpas_sleep(start_time);

    int n = (int)(array_bytes / sizeof(double));
    double *a    = malloc(n * sizeof(double));
    double *b    = malloc(n * sizeof(double));
    double *c    = malloc(n * sizeof(double));
    double *d    = malloc(n * sizeof(double));
    if (!a || !b || !c || !d) { perror("malloc"); return 1; }

    srand((unsigned)time(NULL));
    for (int i = 0; i < n; i++) {
        a[i] = b[i] = d[i] = rand() / (double)RAND_MAX * 2.0 - 1.0;
        c[i] = (rand() % 2 == 0) ? 1.0 : -1.0;
    }

    printf("Starting branchmiss: n=%d (%.0f MB/array), procs=%d, duration=%.1f\n",
           n, (double)(n * sizeof(double)) / (1024.0 * 1024.0), nprocs, duration);
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
                branchmiss_worker(a, b, c, d, n, sleep_ms, false);
                _exit(0);
            }
            pids[i] = pid;
        }
    }

    set_duration(duration);
    branchmiss_worker(a, b, c, d, n, sleep_ms, verbose);

    if (pids) {
        for (int i = 0; i < nprocs - 1; i++) {
            kill(pids[i], SIGTERM);
            waitpid(pids[i], NULL, 0);
        }
        free(pids);
    }

    free(a); free(b); free(c); free(d);
    printf("\nExiting branchmiss.\n");
    return 0;
}

