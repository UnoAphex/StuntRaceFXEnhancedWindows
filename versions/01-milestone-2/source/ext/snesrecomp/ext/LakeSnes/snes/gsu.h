/*
 * Super FX / GSU-2 coprocessor emulation for LakeSnes
 *
 * Written from scratch in C, using the ares emulator (ISC license)
 * as architectural reference.
 *
 * The GSU (Game Support Unit) is a custom RISC processor found in
 * SNES cartridges. It has 16 general-purpose 16-bit registers,
 * a 512-byte instruction cache, and a pixel plot engine for
 * rendering directly to a framebuffer in cartridge RAM.
 *
 * Register space: $3000-$303F (accessible by the 65816 when GSU is idle)
 * Cache RAM:      $3100-$32FF (512 bytes, accessible by 65816 when idle)
 */

#ifndef GSU_H
#define GSU_H

#include <stdint.h>
#include <stdbool.h>

typedef struct Gsu Gsu;

#include "statehandler.h"

/* pixel cache entry — 8 pixels at a time */
typedef struct {
    uint16_t offset;     /* VRAM word offset for this 8-pixel row */
    uint8_t  bitpend;    /* bitmask of which pixels are pending */
    uint8_t  data[8];    /* pixel color indices */
} GsuPixelCache;

struct Gsu {
    void *snes;  /* back-pointer to Snes struct */

    /* ── general-purpose registers ─────────────────────── */
    uint16_t r[16];      /* R0-R15 (R15 = PC) */
    uint8_t  sreg;       /* source register index (FROM) */
    uint8_t  dreg;       /* destination register index (TO) */
    bool     sreg_set;   /* FROM prefix active */
    bool     dreg_set;   /* TO prefix active */
    bool     with_flag;  /* WITH prefix active (sets both sreg+dreg) */

    /* ── status flags register (SFR) ───────────────────── */
    bool flag_z;         /* zero */
    bool flag_cy;        /* carry */
    bool flag_s;         /* sign (negative) */
    bool flag_ov;        /* overflow */
    bool flag_go;        /* running (set when R15 high byte written) */
    bool flag_rom;       /* ROM access flag (read-busy) */
    bool flag_alt1;      /* ALT1 prefix */
    bool flag_alt2;      /* ALT2 prefix */
    bool flag_il;        /* IRQ pending (low) */
    bool flag_ih;        /* IRQ pending (high) */
    bool flag_b;         /* WITH active (same as with_flag) */
    bool flag_irq;       /* IRQ line to 65816 */

    /* ── control registers ─────────────────────────────── */
    uint8_t  pbr;        /* program bank register */
    uint8_t  rombr;      /* ROM bank register (for ROMB) */
    uint8_t  rambr;      /* RAM bank register (for RAMB) */
    uint16_t cbr;        /* cache base register */
    uint8_t  scbr;       /* screen base register */
    uint8_t  scmr;       /* screen mode register */
    uint8_t  colr;       /* plot color register */
    uint8_t  por;        /* plot option register */
    uint8_t  bramr;      /* backup RAM register (write-protect) */
    uint8_t  vcr;        /* version code register (always 0x04 for GSU-2) */
    uint8_t  cfgr;       /* config register */
    uint8_t  clsr;       /* clock speed register */

    /* ── ROM/RAM access buffers ────────────────────────── */
    uint8_t  romdr;      /* ROM data read buffer */
    uint16_t ramar;      /* RAM buffer address */
    uint8_t  ramdr;      /* RAM data buffer */

    /* ── pipeline ──────────────────────────────────────── */
    uint8_t  pipeline;   /* prefetched opcode */
    bool     pipeline_valid;

    /* ── instruction cache (512 bytes, 32 x 16-byte lines) */
    uint8_t  cache[512];
    bool     cache_valid[32]; /* one flag per 16-byte line */

    /* ── pixel cache (2 entries) ───────────────────────── */
    GsuPixelCache pixcache[2];

    /* ── memory pointers (set during load) ─────────────── */
    uint8_t  *rom;
    uint32_t rom_size;
    uint8_t  *ram;       /* cartridge RAM (GSU work RAM, 64-128 KB) */
    uint32_t ram_size;

    /* ── timing ────────────────────────────────────────── */
    uint64_t cycles;     /* cycles executed */
};

/* lifecycle */
Gsu *gsu_init(void *snes);
void gsu_free(Gsu *gsu);
void gsu_reset(Gsu *gsu);
void gsu_power(Gsu *gsu);

/* memory setup (called after ROM load) */
void gsu_load(Gsu *gsu, uint8_t *rom, uint32_t rom_size,
              uint8_t *ram, uint32_t ram_size);

/* I/O register access from 65816 side ($3000-$303F) */
uint8_t gsu_read(Gsu *gsu, uint16_t addr);
void    gsu_write(Gsu *gsu, uint16_t addr, uint8_t val);

/* cache RAM access from 65816 side ($3100-$32FF) */
uint8_t gsu_cache_read(Gsu *gsu, uint16_t addr);
void    gsu_cache_write(Gsu *gsu, uint16_t addr, uint8_t val);

/* execute GSU instructions until it stops or hits a cycle limit */
void gsu_run(Gsu *gsu);

/* state save/load */
void gsu_handleState(Gsu *gsu, StateHandler *sh);

/* screen mode helpers */
int  gsu_screen_height(Gsu *gsu);
int  gsu_screen_bpp(Gsu *gsu);

#endif /* GSU_H */
