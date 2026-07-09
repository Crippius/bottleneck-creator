#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <getopt.h>
#include <stdbool.h>
#include "src/utils.h"



#define DEFAULT_PRECWASTE_N     512

static struct option const long_opt[] =
{
    {"help",     no_argument,       NULL, 'h'},
    {"verbose",  no_argument,       NULL, 'v'},
    {"size",     required_argument, NULL, 'n'},
    {"duration", required_argument, NULL, 'd'},
    {"start",    required_argument, NULL, 't'},
    {"rate",     required_argument, NULL, 'r'},
    {"procs",    required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}
};

static const char *precwaste_usage = "Precision Waste Anomaly (FIXED - float + AVX2 SP).\n\n"
    "-n, --size (=512)       Side length of the n x n matrices.\n"
    "-d, --duration (=-1.0)  The total duration (in seconds), -1 for infinite.\n"
    "-t, --start (=0.0)      The time to wait (in seconds) before starting the anomaly.\n"
    "-r, --rate (=0)         Sleep between MMM repetitions in ms; 0 = full speed.\n"
    "-p, --procs (=1)        Number of worker processes to run simultaneously.\n"
    "-v, --verbose           Prints execution information.\n"
    "-h, --help              Prints this message.\n";

static void do_mmm(const float *A, const float *B, float *C, int n) {

    for (int i = 0; i < n; i++)
        for (int k = 0; k < n; k++) {
            float aik = A[i * n + k];
            for (int j = 0; j < n; j++)
                C[i * n + j] += aik * B[k * n + j];
        }
}

static void precwaste_worker(const float *A, const float *B, float *C,
                              int n, int sleep_ms, bool verbose) {
    volatile float sink = 0.0f;
    int counter = 0;
    while (timer_flag) {
        memset(C, 0, (size_t)n * n * sizeof(float));
        do_mmm(A, B, C, n);
        sink = C[(n / 2) * n + n / 2];
        if (sleep_ms > 0)
            hpas_sleep(sleep_ms / 1000.0);
        if (verbose && ++counter % 5 == 0) {
            printf(".");
            fflush(stdout);
        }
    }
    (void)sink;
}

static void on_sigterm(int sig) {
    (void)sig;
    signal(SIGTERM, SIG_IGN);
    kill(0, SIGTERM);
    while (wait(NULL) > 0);
    exit(0);
}

int precwaste(int argc, char *argv[]) {
    int    n          = DEFAULT_PRECWASTE_N;
    double duration   = -1.0;
    double start_time = 0.0;
    int    sleep_ms   = 0;
    int    nprocs     = 1;
    bool   verbose    = false;
    int    opt;

    while ((opt = getopt_long(argc, argv, "hvn:d:t:r:p:", long_opt, NULL)) != -1) {
        switch (opt) {
            case 'n': n          = atoi(optarg);           break;
            case 'd': duration   = strtod(optarg, NULL);   break;
            case 't': start_time = strtod(optarg, NULL);   break;
            case 'r': sleep_ms   = atoi(optarg);           break;
            case 'p': nprocs     = atoi(optarg);
                      if (nprocs < 1) { fprintf(stderr, "procs must be >= 1\n"); return 1; }
                      break;
            case 'v': verbose    = true;                   break;
            case 'h':
            default:
                printf("Usage: hpas_fixed precwaste [OPTIONS]\n%s", precwaste_usage);
                return 0;
        }
    }

    hpas_sleep(start_time);

    float *A = malloc((size_t)n * n * sizeof(float));
    float *B = malloc((size_t)n * n * sizeof(float));
    float *C = calloc((size_t)n * n, sizeof(float));
    if (!A || !B || !C) { perror("malloc"); return 1; }

    srand(12345u);
    for (int i = 0; i < n * n; i++) {
        A[i] = rand() / (float)RAND_MAX;
        B[i] = rand() / (float)RAND_MAX;
    }

    printf("Starting precwaste (FIXED, float/AVX): n=%d (%ld MB/matrix), procs=%d, duration=%.1f\n",
           n, ((long)n * n * sizeof(float)) / (1024 * 1024), nprocs, duration);
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
                precwaste_worker(A, B, C, n, sleep_ms, false);
                _exit(0);
            }
            pids[i] = pid;
        }
    }

    set_duration(duration);
    precwaste_worker(A, B, C, n, sleep_ms, verbose);

    if (pids) {
        for (int i = 0; i < nprocs - 1; i++) {
            kill(pids[i], SIGTERM);
            waitpid(pids[i], NULL, 0);
        }
        free(pids);
    }

    free(A); free(B); free(C);
    printf("\nExiting precwaste (FIXED).\n");
    return 0;
}

