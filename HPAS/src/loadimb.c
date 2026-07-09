#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <omp.h>
#include "src/utils.h"

#define DEFAULT_LOADIMB_ELEMS      131072
#define DEFAULT_LOADIMB_HIGH_ITERS 100
#define DEFAULT_LOADIMB_LOW_ITERS  10

static struct option const long_opt[] =
{
    {"help",      no_argument,       NULL, 'h'},
    {"verbose",   no_argument,       NULL, 'v'},
    {"procs",     required_argument, NULL, 'p'},
    {"size",      required_argument, NULL, 's'},
    {"high",      required_argument, NULL, 'H'},
    {"low",       required_argument, NULL, 'l'},
    {"slow-frac", required_argument, NULL, 'f'},
    {"duration",  required_argument, NULL, 'd'},
    {"start",     required_argument, NULL, 't'},
    {NULL, 0, NULL, 0}
};

static const char *loadimb_usage = "Intra-Node Load Imbalance Anomaly.\n\n"
    "-p, --procs (=OMP_NUM_THREADS)  Number of OpenMP threads.\n"
    "-s, --size (=131072)            Array elements per thread.\n"
    "-H, --high (=100)               Iteration count for the slow (heavy) threads.\n"
    "-l, --low (=10)                 Iteration count for the fast (light) threads.\n"
    "-f, --slow-frac (=0.25)         Fraction of threads that are slow (0.0-1.0).\n"
    "-d, --duration (=-1.0)          The total duration (in seconds), -1 for infinite.\n"
    "-t, --start (=0.0)              The time to wait (in seconds) before starting.\n"
    "-v, --verbose                   Prints execution information.\n"
    "-h, --help                      Prints this message.\n\n"
    "NOTE: -p must equal the number of cores the process is pinned to.\n"
    "      Example: taskset -c 0-3 hpas loadimb -p 4\n";


static void do_work(double *arrA, const double *arrB, int n, int iters, int tid) {
    unsigned int rng = (unsigned int)(tid + 1) * 2654435761u;
    for (int it = 0; it < iters; it++) {
        for (int i = 0; i < n; i++) {
            rng = rng * 1664525u + 1013904223u;
            int idx = (int)(rng >> 1) % n;
            arrA[idx] = arrB[idx] / (double)(i + 10);
        }
    }
}

int loadimb(int argc, char *argv[]) {
    int    nthreads   = omp_get_max_threads();
    int    n          = DEFAULT_LOADIMB_ELEMS;
    int    high_iters = DEFAULT_LOADIMB_HIGH_ITERS;
    int    low_iters  = DEFAULT_LOADIMB_LOW_ITERS;
    double slow_frac  = 0.25;
    double duration   = -1.0;
    double start_time = 0.0;
    bool   verbose    = false;
    int    opt;

    while ((opt = getopt_long(argc, argv, "hvp:s:H:l:f:d:t:", long_opt, NULL)) != -1) {
        switch (opt) {
            case 'p': nthreads   = atoi(optarg);           break;
            case 's': n          = atoi(optarg);           break;
            case 'H': high_iters = atoi(optarg);           break;
            case 'l': low_iters  = atoi(optarg);           break;
            case 'f': slow_frac  = strtod(optarg, NULL);
                      if (slow_frac <= 0.0 || slow_frac >= 1.0) {
                          fprintf(stderr, "slow-frac must be in (0.0, 1.0)\n");
                          return 1;
                      }
                      break;
            case 'd': duration   = strtod(optarg, NULL);   break;
            case 't': start_time = strtod(optarg, NULL);   break;
            case 'v': verbose    = true;                   break;
            case 'h':
            default:
                printf("Usage: hpas loadimb [OPTIONS]\n%s", loadimb_usage);
                return 0;
        }
    }

    hpas_sleep(start_time);

    omp_set_num_threads(nthreads);
    int n_slow = (int)(nthreads * slow_frac + 0.5);
    if (n_slow < 1) n_slow = 1;
    printf("Starting loadimb: threads=%d, elems=%d, high=%d, low=%d, slow_frac=%.3f (%d slow), duration=%.1f\n",
           nthreads, n, high_iters, low_iters, slow_frac, n_slow, duration);
    fflush(stdout);

    int counter = 0;
    set_duration(duration);
    while (timer_flag) {
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            int nt  = omp_get_num_threads();


            int n_slow_local = (int)(nt * slow_frac + 0.5);
            if (n_slow_local < 1) n_slow_local = 1;
            int iters = (tid < nt - n_slow_local) ? low_iters : high_iters;

            if (iters > 0) {
                double *arrA = malloc((size_t)n * sizeof(double));
                double *arrB = malloc((size_t)n * sizeof(double));

                unsigned int seed = (unsigned int)(tid + 1) * 2654435761u;
                for (int i = 0; i < n; i++) {
                    seed = seed * 1664525u + 1013904223u;
                    arrB[i] = (double)(seed >> 1) / (double)(1u << 31) + 0.1;
                }
                memset(arrA, 0, (size_t)n * sizeof(double));
                do_work(arrA, arrB, n, iters, tid);
                free(arrA);
                free(arrB);
            }


            #pragma omp barrier
        }

        if (verbose && ++counter % 10 == 0) {
            printf(".");
            fflush(stdout);
        }
    }

    printf("\nExiting loadimb.\n");
    return 0;
}

