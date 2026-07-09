#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <config.h>
#include "src/anomalies.h"

#ifndef BINDIR
#  define BINDIR "."
#endif

static const char *main_usage = "Please input a valid anomaly name:\n"
    "memleak, memeater, membw, cpuoccupy, netoccupy, cachecopy,\n"
    "iometadata, iobandwidth, branchmiss, pipestall, precwaste, loadimb.\n";

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("%s", main_usage);
        _exit(0);
    }
    if (strcmp(argv[1], "memleak") == 0) {
        return memleak(argc - 1, &argv[1]);
    }
    if (strcmp(argv[1], "memeater") == 0) {
        return memeater(argc - 1, &argv[1]);
    }
    if (strcmp(argv[1], "membw") == 0) {
#ifndef HAVE_XMMINTRIN_H
        printf("Please compile with xmmintrin.h to get memory bandwidth anomaly.\n");
#else
        return membw(argc - 1, &argv[1]);
#endif
    }
    if (strcmp(argv[1], "cpuoccupy") == 0) {
        return cpuoccupy(argc - 1, &argv[1]);
    }
    if (strcmp(argv[1], "netoccupy") == 0) {
#if HAVE_SHMEM != 1
        printf("Please compile with SHMEM to get network contention anomaly.\n");
        return 0;
#else
        return netoccupy(argc - 1, &argv[1]);
#endif
    }
    if (strcmp(argv[1], "cachecopy") == 0) {
        return cachecopy(argc - 1, &argv[1]);
    }
    if (strcmp(argv[1], "iometadata") == 0) {
        return iometadata(argc - 1, &argv[1]);
    }
    if (strcmp(argv[1], "iobandwidth") == 0) {
        execvp(BINDIR"/iobandwidth", &argv[1]);
        if (errno != 0) {
            printf("exec failed: %s\n"
                   "This may be because `make install` is not executed.\n"
                   "Check if %s exists.\n",
                   strerror(errno), BINDIR"/iobandwidth");
            _exit(0);
        }
        return 0;
    }
    if (strcmp(argv[1], "branchmiss") == 0) {
        return branchmiss(argc - 1, &argv[1]);
    }
    if (strcmp(argv[1], "pipestall") == 0) {
        return pipestall(argc - 1, &argv[1]);
    }
    if (strcmp(argv[1], "precwaste") == 0) {
        return precwaste(argc - 1, &argv[1]);
    }
    if (strcmp(argv[1], "loadimb") == 0) {
#ifndef HAVE_OMP
        printf("Please compile with OpenMP to get load imbalance anomaly.\n");
        return 0;
#else
        return loadimb(argc - 1, &argv[1]);
#endif
    }
    printf("%s", main_usage);
    _exit(0);
}

