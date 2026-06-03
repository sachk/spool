/*
 * bigalloc.c -- size-gated allocation tracer (LD_PRELOAD).
 *
 * Heaptrack can't run here: unwinding on *every* allocation crashes on this
 * ARM target. But the question is only "what are the big allocations?", and
 * those are rare. So we interpose malloc/calloc/realloc/free/memalign and
 * capture a full backtrace ONLY when size >= BIGALLOC_THRESHOLD (default 1 MiB).
 * Live allocations are kept in a fixed open-addressed table; a background thread
 * periodically writes the live set (size + stack) to BIGALLOC_OUTPUT. Symbolize
 * the stacks offline with tools/webos/bigalloc-report.py.
 *
 * Overhead is negligible (a few hundred big allocs vs ~1.3M total), and we only
 * unwind big-alloc call sites (ffmpeg/mpv/decode), avoiding the JIT/SMP frames
 * that crash the unwinder.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <unwind.h>

#define MAX_FRAMES 24
#define TABLE_CAP 32768u   /* live >threshold allocations tracked at once */

struct entry {
    void *ptr;
    size_t size;
    int nframes;
    void *frames[MAX_FRAMES];
};

static struct entry g_table[TABLE_CAP];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static size_t g_threshold = 1u << 20; /* 1 MiB */
static char g_output[256] = "/tmp/bigalloc.out";
static int g_dump_sec = 5;

static void *(*real_malloc)(size_t);
static void *(*real_calloc)(size_t, size_t);
static void *(*real_realloc)(void *, size_t);
static void (*real_free)(void *);
static void *(*real_memalign)(size_t, size_t);
static int (*real_posix_memalign)(void **, size_t, size_t);

static __thread int in_hook; /* prevent recursion (dlsym/backtrace may alloc) */

/* Bootstrap allocator: dlsym() may call calloc before real_* are resolved. */
static char g_boot[64 * 1024];
static size_t g_boot_off;
static int is_boot_ptr(void *p) { return (char *)p >= g_boot && (char *)p < g_boot + sizeof(g_boot); }
static void *boot_alloc(size_t n) {
    n = (n + 15u) & ~15u;
    if (g_boot_off + n > sizeof(g_boot)) return NULL;
    void *p = g_boot + g_boot_off;
    g_boot_off += n;
    return p;
}

static void resolve(void) {
    real_malloc = (void *(*)(size_t))dlsym(RTLD_NEXT, "malloc");
    real_calloc = (void *(*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");
    real_realloc = (void *(*)(void *, size_t))dlsym(RTLD_NEXT, "realloc");
    real_free = (void (*)(void *))dlsym(RTLD_NEXT, "free");
    real_memalign = (void *(*)(size_t, size_t))dlsym(RTLD_NEXT, "memalign");
    real_posix_memalign = (int (*)(void **, size_t, size_t))dlsym(RTLD_NEXT, "posix_memalign");
}

/* libgcc .eh_frame/.ARM.exidx unwinder (no libunwind -- it crashes here). */
struct bt_state { void **frames; int n; int max; };
static _Unwind_Reason_Code bt_cb(struct _Unwind_Context *ctx, void *arg) {
    struct bt_state *s = arg;
    if (s->n >= s->max) return _URC_END_OF_STACK;
    uintptr_t ip = _Unwind_GetIP(ctx);
    if (ip) s->frames[s->n++] = (void *)ip;
    return _URC_NO_REASON;
}

static size_t hashptr(void *p) { return ((uintptr_t)p >> 4) * 2654435761u; }

static void table_insert(void *p, size_t size, void *frames[], int n) {
    size_t i = hashptr(p) % TABLE_CAP;
    for (size_t probe = 0; probe < TABLE_CAP; probe++) {
        struct entry *e = &g_table[(i + probe) % TABLE_CAP];
        if (e->ptr == NULL || e->ptr == p) {
            e->ptr = p;
            e->size = size;
            e->nframes = n;
            memcpy(e->frames, frames, n * sizeof(void *));
            return;
        }
    }
    /* table full: silently drop (shouldn't happen for big allocs) */
}

static void table_remove(void *p) {
    if (!p) return;
    size_t i = hashptr(p) % TABLE_CAP;
    for (size_t probe = 0; probe < TABLE_CAP; probe++) {
        struct entry *e = &g_table[(i + probe) % TABLE_CAP];
        if (e->ptr == p) { e->ptr = NULL; return; }
        if (e->ptr == NULL) return;
    }
}

static void track(void *p, size_t size, void *caller) {
    if (!p || size < g_threshold || in_hook) return;
    in_hook = 1;
    void *frames[MAX_FRAMES];
    /* frame[0] = immediate caller (__builtin_return_address, always available,
     * no unwind tables needed -> names the allocator even in libs without CFI
     * like ffmpeg/Qt). _Unwind_Backtrace adds deeper context where it can. */
    int base = 0;
    if (caller) frames[base++] = caller;
    struct bt_state s = { frames + base, 0, MAX_FRAMES - base };
    _Unwind_Backtrace(bt_cb, &s);
    pthread_mutex_lock(&g_lock);
    table_insert(p, size, frames, base + s.n);
    pthread_mutex_unlock(&g_lock);
    in_hook = 0;
}

void *malloc(size_t n) {
    if (!real_malloc) { if (!in_hook) { in_hook = 1; resolve(); in_hook = 0; } if (!real_malloc) return boot_alloc(n); }
    void *p = real_malloc(n);
    track(p, n, __builtin_return_address(0));
    return p;
}

void *calloc(size_t a, size_t b) {
    if (!real_calloc) { if (!in_hook) { in_hook = 1; resolve(); in_hook = 0; } if (!real_calloc) { void *p = boot_alloc(a * b); if (p) memset(p, 0, a * b); return p; } }
    void *p = real_calloc(a, b);
    track(p, a * b, __builtin_return_address(0));
    return p;
}

void *realloc(void *old, size_t n) {
    if (!real_realloc) { if (!in_hook) { in_hook = 1; resolve(); in_hook = 0; } if (!real_realloc) return boot_alloc(n); }
    if (old && !is_boot_ptr(old)) { pthread_mutex_lock(&g_lock); table_remove(old); pthread_mutex_unlock(&g_lock); }
    void *p = real_realloc(old, n);
    track(p, n, __builtin_return_address(0));
    return p;
}

void free(void *p) {
    if (is_boot_ptr(p)) return;
    if (!real_free) { if (!in_hook) { in_hook = 1; resolve(); in_hook = 0; } }
    if (p && !in_hook) { pthread_mutex_lock(&g_lock); table_remove(p); pthread_mutex_unlock(&g_lock); }
    if (real_free) real_free(p);
}

void *memalign(size_t al, size_t n) {
    if (!real_memalign) { if (!in_hook) { in_hook = 1; resolve(); in_hook = 0; } if (!real_memalign) return boot_alloc(n); }
    void *p = real_memalign(al, n);
    track(p, n, __builtin_return_address(0));
    return p;
}

int posix_memalign(void **out, size_t al, size_t n) {
    if (!real_posix_memalign) { if (!in_hook) { in_hook = 1; resolve(); in_hook = 0; } }
    int r = real_posix_memalign(out, al, n);
    if (r == 0) track(*out, n, __builtin_return_address(0));
    return r;
}

static void dump_once(void) {
    in_hook = 1;
    char tmp[300];
    snprintf(tmp, sizeof(tmp), "%s.tmp", g_output);
    FILE *f = fopen(tmp, "w");
    if (!f) { in_hook = 0; return; }
    pthread_mutex_lock(&g_lock);
    size_t total = 0, count = 0;
    for (size_t i = 0; i < TABLE_CAP; i++) {
        struct entry *e = &g_table[i];
        if (!e->ptr) continue;
        total += e->size;
        count++;
        fprintf(f, "%zu", e->size);
        for (int j = 0; j < e->nframes; j++) {
            Dl_info di;
            if (dladdr(e->frames[j], &di) && di.dli_fname) {
                if (di.dli_sname && di.dli_saddr) {
                    uintptr_t soff = (uintptr_t)e->frames[j] - (uintptr_t)di.dli_saddr;
                    fprintf(f, " %s(%s+0x%lx)", di.dli_fname, di.dli_sname, (unsigned long)soff);
                } else {
                    uintptr_t off = (uintptr_t)e->frames[j] - (uintptr_t)di.dli_fbase;
                    fprintf(f, " %s+0x%lx", di.dli_fname, (unsigned long)off);
                }
            } else {
                fprintf(f, " ?+0x%lx", (unsigned long)(uintptr_t)e->frames[j]);
            }
        }
        fprintf(f, "\n");
    }
    pthread_mutex_unlock(&g_lock);
    fprintf(f, "# live_big_allocs=%zu total_bytes=%zu\n", count, total);
    fclose(f);
    rename(tmp, g_output);
    in_hook = 0;
}

static void *dumper(void *arg) {
    (void)arg;
    for (;;) {
        struct timespec ts = { g_dump_sec, 0 };
        nanosleep(&ts, NULL);
        dump_once();
    }
    return NULL;
}

__attribute__((constructor)) static void bigalloc_init(void) {
    resolve();
    const char *t = getenv("BIGALLOC_THRESHOLD");
    if (t && *t) g_threshold = strtoull(t, NULL, 10);
    const char *o = getenv("BIGALLOC_OUTPUT");
    if (o && *o) { snprintf(g_output, sizeof(g_output), "%s", o); }
    const char *d = getenv("BIGALLOC_DUMP_SEC");
    if (d && *d) g_dump_sec = atoi(d);
    pthread_t th;
    pthread_create(&th, NULL, dumper, NULL);
    pthread_detach(th);
}
