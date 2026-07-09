#ifndef _ANOMALIES_H
#define _ANOMALIES_H

int memleak(int argc, char *argv[]);
int membw(int argc, char *argv[]);
int memeater(int argc, char *argv[]);
int cachecopy(int argc, char *argv[]);
int cpuoccupy(int argc, char *argv[]);
int netoccupy(int argc, char *argv[]);
int iometadata(int argc, char *argv[]);
int branchmiss(int argc, char *argv[]);
int pipestall(int argc, char *argv[]);
int precwaste(int argc, char *argv[]);
#ifdef HAVE_OMP
int loadimb(int argc, char *argv[]);
#endif

#endif

