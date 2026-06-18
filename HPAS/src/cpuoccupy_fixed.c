#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include "src/utils.h"

static struct option const long_opt[] =
{
    {"help",        no_argument,       NULL, 'h'},
    {"verbose",     no_argument,       NULL, 'v'},
    {"utilization", required_argument, NULL, 'u'},
    {"duration",    required_argument, NULL, 'd'},
    {"start",       required_argument, NULL, 't'},
    {"procs",       required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}
};

static const char *cpuoccupy_usage = "CPU intensive orphan process.\n\n"
    "-u, --utilization (=100%)  The utilization (%) of one core.\n"
    "-p, --procs (=1)           Number of worker processes to run simultaneously.\n"
    "-d, --duration (=-1.0)     The total duration (in seconds), -1 for infinite.\n"
    "-t, --start (=0.0)         The time to wait (in seconds) before starting the anomaly.\n"
    "-v, --verbose              Prints execution information.\n"
    "-h, --help                 Prints this message.\n";
static const char short_opt[] = "hvd:u:t:p:";

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* AVX2+FMA: 4 independent double chains × 4 doubles/ymm × 2 FLOPs/FMA */
#pragma GCC push_options
#pragma GCC optimize("O3,tree-vectorize")
#pragma GCC target("avx2,fma")
static void do_avx_work(double *a, int n) {
    for (int i = 0; i < n; i++)
        a[i] = a[i] * 1.0000001 + 1.0e-10;
}
#pragma GCC pop_options

static void cpuoccupy_worker(int percutil, double dursec, bool verbose) {
    double interval  = 0.1;
    double worktime  = interval * (percutil / 100.0);
    double sleeptime = interval - worktime;
    int counter = 0;

    /* 256 doubles = 2 KB, stays in L1; independent elements → full AVX2 throughput */
    double a[256];
    for (int i = 0; i < 256; i++) a[i] = (double)(i + 1) * 0.001;

    double start = now_sec();
    double deadline = (dursec > 0) ? start + dursec : -1.0;

    while (1) {
        double t0 = now_sec();
        do {
            do_avx_work(a, 256);
        } while (now_sec() - t0 < worktime);

        hpas_sleep(sleeptime);

        if (deadline > 0 && now_sec() >= deadline)
            break;
        if (verbose && ++counter % 100 == 0) {
            printf(".");
            fflush(stdout);
        }
    }
    /* prevent dead-code elimination of a[] */
    volatile double sink = a[0];
    (void)sink;
}

static void on_sigterm(int sig) {
    (void)sig;
    signal(SIGTERM, SIG_IGN);
    kill(0, SIGTERM);
    while (wait(NULL) > 0);
    exit(0);
}

int cpuoccupy(int argc, char *argv[]) {
    bool verbose    = false;
    double dursec   = -1;
    double start_time = 0;
    int percutil    = 100;
    int nprocs      = 1;
    int c;

    while ((c = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1) {
        switch (c) {
            case 'u':
                percutil = (int)strtol(optarg, NULL, 10);
                if (percutil < 0 || percutil > 100) {
                    printf("bad percent util\n");
                    exit(-1);
                }
                break;
            case 't':
                start_time = strtod(optarg, NULL);
                if (start_time < 0) {
                    printf("Start time cannot be negative.\n");
                    _exit(0);
                }
                break;
            case 'd':
                dursec = strtod(optarg, NULL);
                break;
            case 'p':
                nprocs = atoi(optarg);
                if (nprocs < 1) { fprintf(stderr, "procs must be >= 1\n"); return 1; }
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
            default:
                printf("Usage: %s [OPTIONS]\n%s", argv[0], cpuoccupy_usage);
                return 0;
        }
    }

    hpas_sleep(start_time);

    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    printf("%sStarting cpuoccupy (avx): utilization=%d%%, procs=%d\n\n",
           asctime(timeinfo), percutil, nprocs);
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
                cpuoccupy_worker(percutil, dursec, false);
                _exit(0);
            }
            pids[i] = pid;
        }
    }

    cpuoccupy_worker(percutil, dursec, verbose);

    if (pids) {
        for (int i = 0; i < nprocs - 1; i++) {
            kill(pids[i], SIGTERM);
            waitpid(pids[i], NULL, 0);
        }
        free(pids);
    }

    printf("Exiting cpuoccupy.\n");
    return 0;
}
