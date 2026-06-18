AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4 --install
AM_CFLAGS = -DBINDIR='"$(bindir)"' -O2

dist_bin_SCRIPTS = src/iobandwidth
bin_PROGRAMS = hpas hpas_fixed

# ---- hpas: original bottlenecked binary ------------------------------------
hpas_SOURCES = src/cachecopy.c src/cpuoccupy.c src/iometadata.c \
               src/main.c src/memeater.c src/memleak.c src/utils.c \
               src/utils.h src/anomalies.h
if HAVE_SHMEM
hpas_SOURCES += src/netoccupy.c
endif
if HAVE_XMM
hpas_SOURCES += src/membw.c
endif

noinst_LIBRARIES = libbranchmiss.a libpipestall.a libprecwaste.a

libbranchmiss_a_SOURCES = src/branchmiss.c
libbranchmiss_a_CFLAGS  = $(AM_CFLAGS) -mrdrnd

libpipestall_a_SOURCES = src/pipestall.c
libpipestall_a_CFLAGS  = -O2

libprecwaste_a_SOURCES = src/precwaste.c
libprecwaste_a_CFLAGS  = -O0 -fno-tree-vectorize

hpas_LDADD = libbranchmiss.a libpipestall.a libprecwaste.a

if HAVE_OMP
noinst_LIBRARIES    += libloadimb.a
libloadimb_a_SOURCES = src/loadimb.c
libloadimb_a_CFLAGS  = $(OPENMP_CFLAGS) $(AM_CFLAGS)
hpas_LDADD          += libloadimb.a
hpas_LDFLAGS         = $(OPENMP_CFLAGS)
endif

# ---- hpas_fixed: patched binary with bottlenecks removed -------------------
noinst_LIBRARIES += libbranchmiss_fixed.a libpipestall_fixed.a libprecwaste_fixed.a libcpuoccupy_fixed.a

libcpuoccupy_fixed_a_SOURCES = src/cpuoccupy_fixed.c
libcpuoccupy_fixed_a_CFLAGS  = $(AM_CFLAGS)

libbranchmiss_fixed_a_SOURCES = src/branchmiss_fixed.c
libbranchmiss_fixed_a_CFLAGS  = $(AM_CFLAGS) -mrdrnd

libpipestall_fixed_a_SOURCES = src/pipestall_fixed.c
libpipestall_fixed_a_CFLAGS  = -O2

libprecwaste_fixed_a_SOURCES = src/precwaste_fixed.c
libprecwaste_fixed_a_CFLAGS  = -O2 -march=native

hpas_fixed_SOURCES = src/cachecopy.c src/iometadata.c \
                     src/main.c src/memeater.c src/memleak.c src/utils.c \
                     src/utils.h src/anomalies.h
if HAVE_SHMEM
hpas_fixed_SOURCES += src/netoccupy.c
endif
if HAVE_XMM
hpas_fixed_SOURCES += src/membw.c
endif

hpas_fixed_LDADD = libbranchmiss_fixed.a libpipestall_fixed.a libprecwaste_fixed.a libcpuoccupy_fixed.a
if HAVE_OMP
hpas_fixed_LDADD   += libloadimb.a
hpas_fixed_LDFLAGS  = $(OPENMP_CFLAGS)
endif
