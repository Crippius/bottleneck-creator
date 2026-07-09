#include <config.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <getopt.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "src/utils.h"

static struct option const long_opt[] =
{
    {"help",       no_argument,       NULL, 'h'},
    {"verbose",    no_argument,       NULL, 'v'},
    {"cache",      required_argument, NULL, 'c'},
    {"multiplier", required_argument, NULL, 'm'},
    {"start",      required_argument, NULL, 't'},
    {"period",     required_argument, NULL, 'r'},
    {"duration",   required_argument, NULL, 'd'},
    {"nprocs",     required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}
};

const char *cachecopy_usage = "cache.\n\n"
    "-c, --cache (=L1)       Which cache to use (L1, L2, L3).\n"
    "-m, --multiplier (=1.0) The multiplier for cache size.\n"
    "-p, --nprocs (=1)       Number of worker processes to run simultaneously.\n"
    "-r, --period (=0.0)     The time to wait (in seconds) between tests.\n"
    "-d, --duration (=-1.0)  The total duration (in seconds), -1 for infinite.\n"
    "-t, --start (=0.0)      The time to wait (in seconds) before starting the anomaly.\n"
    "-v, --verbose           Prints execution information.\n"
    "-h, --help              Prints this message.\n";
static const char short_opt[] = "hvd:c:r:m:t:p:";

double run_one(const double *lptr, double *rptr, size_t ndouble)
{
    uint32_t i;
    double sum = 0;
    for (i = 0; i < ndouble; ++rptr, ++lptr, ++i)
        (*rptr) = (*lptr);
    return sum;
}

static void cachecopy_worker(size_t size, double multiplier, double period,
                              double duration, bool verbose) {
    double *ldata;
    long ndouble = (long)(((double)size * multiplier) / sizeof(double));
    if (ndouble % 2 == 1) ndouble++;

    int k = posix_memalign((void **)&ldata, CPU_L1_CACHE, ndouble * sizeof(double));
    if (k != 0) { perror("posix_memalign"); _exit(1); }
    double *rdata = ldata + (ndouble / 2);
    for (uint32_t j = 0; j < (uint32_t)(ndouble / 2); ++j) {
        ldata[j] = 0.02;
        rdata[j] = 0.01;
    }

    double sum = 0;
    int counter = 0;
    set_duration(duration);
    while (timer_flag) {
        sum += run_one(ldata, rdata, ndouble / 2);
        hpas_sleep(period);
        if (verbose && ++counter % 100 == 0) {
            printf(".");
            fflush(stdout);
        }
    }
    (void)sum;
    free(ldata);
}

static void on_sigterm(int sig) {
    (void)sig;
    signal(SIGTERM, SIG_IGN);
    kill(0, SIGTERM);
    while (wait(NULL) > 0);
    exit(0);
}

int cachecopy(int argc, char *argv[])
{
    int c;
    bool verbose    = false;
    size_t size     = CPU_L3_CACHE;
    double multiplier = 1.0;
    double period   = 0;
    double duration = -1;
    double start_time = 0;
    int nprocs      = 1;

    while ((c = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1) {
        switch (c) {
            case -1:
            case 0:
                break;
            case 't':
                start_time = strtod(optarg, NULL);
                if (start_time < 0) { printf("Start time cannot be negative.\n"); _exit(0); }
                break;
            case 'c':
                if (optarg[0] != 'L' && optarg[0] != 'l') { printf("Cache size can only be L1, L2 or L3\n"); return -1; }
                if (optarg[2] != '\0') { printf("Cache size can only be L1, L2 or L3\n"); return -1; }
                switch (optarg[1]) {
                    case '1': size = CPU_L1_CACHE; break;
                    case '2': size = CPU_L2_CACHE; break;
                    case '3': size = CPU_L3_CACHE; break;
                    default:  printf("Cache size can only be L1, L2 or L3\n"); return -1;
                }
                break;
            case 'r':
                period = strtod(optarg, NULL);
                if (period < 0) { printf("Period cannot be smaller than zero.\n"); _exit(0); }
                break;
            case 'd':
                duration = strtod(optarg, NULL);
                break;
            case 'm':
                multiplier = strtof(optarg, NULL);
                break;
            case 'p':
                nprocs = atoi(optarg);
                if (nprocs < 1) { fprintf(stderr, "nprocs must be >= 1\n"); return 1; }
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
            default:
                printf("Usage: %s [OPTIONS]\n%s", argv[0], cachecopy_usage);
                return 0;
        }
    }

    hpas_sleep(start_time);

    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    long ndouble_print = (long)(((double)size * multiplier) / sizeof(double));
    printf("%sStarting cache: ndouble=%ld period=%f duration=%f nprocs=%d\n\n",
           asctime(timeinfo), ndouble_print, period, duration, nprocs);
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
                cachecopy_worker(size, multiplier, period, duration, false);
                _exit(0);
            }
            pids[i] = pid;
        }
    }

    cachecopy_worker(size, multiplier, period, duration, verbose);

    if (pids) {
        for (int i = 0; i < nprocs - 1; i++) {
            kill(pids[i], SIGTERM);
            waitpid(pids[i], NULL, 0);
        }
        free(pids);
    }

    printf("\nFinished cache anomaly\n");
    return 0;
}

