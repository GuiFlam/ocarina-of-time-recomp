// From metroid-prime-4-recomp runtime/dispatch.h @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied unchanged.
// From fe3h-pc-recomp runtime/dispatch.h @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), copied unchanged.
// From totk-pc-recomp runtime/dispatch.h @ c944f9f94520874f2a669add30dd08d29ee9f7b5 (MIT, Copyright (c) 2026 Valiantsin Dzerakh), copied unchanged.
// Guest address -> host function lookup for indirect branches (rt_dispatch, declared in cpu.h).
//
// One flat table covers every module's code range: slot (pc - lo) >> 2 holds the host function for a guest
// function start, as a 32-bit offset from an anchor address. Lookup is one range check and one load. The table is
// built once by rt_dispatch_init and never written again, so threads read it without locks.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "cpu.h"

// A lifted module: its place in the guest layout and the generator's sorted function table.
typedef struct RtModule {
    const char *name;
    uint64_t base, size;      // guest image (layout.h MOD_<NAME>_BASE / _SIZE)
    const uint64_t *addrs;    // <mod>_fn_addrs: guest function starts, strictly ascending
    const GuestFn *fns;       // <mod>_fn_ptrs: the lifted function for each start
    uint64_t count;           // <mod>_fn_count
} RtModule;

// The module glue that tools/build_game.py writes for each build (build/<version>/modules.c), from layout.json and
// layout.h: every module of the layout, and the modules linked into this executable.
typedef struct RtLayoutEntry { const char *name; uint64_t base, size; } RtLayoutEntry;
extern const RtLayoutEntry rt_layout[];
extern const int rt_layout_count;
int rt_linked_modules(RtModule *out, int max);

// Register a host function at a guest address outside every module (HLE entry points). Allowed only before
// rt_dispatch_init; the registry is fixed afterwards.
void rt_register_host(uint64_t addr, GuestFn fn);

// Build the table from the modules (kept by pointer; they must outlive the process). Call it once, before any guest
// thread starts: thread creation publishes the table to the threads. Fatal on a malformed table:
// unsorted or duplicate starts, a start outside its module, overlapping modules, host code spread over 4 GiB.
void rt_dispatch_init(const RtModule *mods, int count);

// The host function for a guest address, or NULL when nothing is there. Does not run guest code.
GuestFn rt_dispatch_lookup(uint64_t pc);

// "0x7000001234 (main+0x1234)" for a guest address inside a module, else the bare address. Returns buf.
const char *rt_guest_name(uint64_t pc, char *buf, int size);

typedef struct RtDispatchStats {
    uint64_t lo, hi;          // guest range the table covers
    uint64_t slots;           // (hi - lo) / 4
    uint64_t table_bytes;     // reserved for the table
    uint64_t functions;       // filled slots
    uint64_t host_fns;        // rt_register_host entries
} RtDispatchStats;
RtDispatchStats rt_dispatch_stats(void);

// Runtime fatal error: message to stderr, then exit. Used by the dispatcher and the stubs.
void rt_fatal(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
