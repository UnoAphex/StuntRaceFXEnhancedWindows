/*
 * Super FX / GSU-2 coprocessor emulation for LakeSnes
 *
 * Written from scratch in C. Architectural reference: ares (ISC license).
 * Technical reference: fullsnes, SNESdev wiki, GSU patent documentation.
 *
 * (c) 2026 sp00nznet — MIT license
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gsu.h"
#include "snes.h"
#include "statehandler.h"

/* ── helpers ──────────────────────────────────────────────── */

#define GSU_CLOCK_NORMAL  1
#define GSU_CLOCK_HIGH    2

#define PC       (gsu->r[15])
#define SREG     (gsu->r[gsu->sreg])
#define DREG     (gsu->r[gsu->dreg])

/* screen mode bits from SCMR */
#define SCMR_RON   (gsu->scmr & 0x10)  /* ROM access enabled */
#define SCMR_RAN   (gsu->scmr & 0x08)  /* RAM access enabled */
#define SCMR_MD    (gsu->scmr & 0x03)  /* screen mode (bpp) */
#define SCMR_HT    ((gsu->scmr & 0x20) | ((gsu->scmr >> 2) & 0x04))  /* height */

/* POR bits */
#define POR_TRANSPARENT  (gsu->por & 0x01)
#define POR_DITHER       (gsu->por & 0x02)
#define POR_COLOR_HIGH   (gsu->por & 0x04)
#define POR_FREEZE_HIGH  (gsu->por & 0x08)
#define POR_OBJ_MODE     (gsu->por & 0x10)

/* CFGR bits */
#define CFGR_IRQ   (gsu->cfgr & 0x80)  /* IRQ enable */
#define CFGR_MS0   (gsu->cfgr & 0x20)  /* multiplier speed */

static int gsu_clockspeed(Gsu *gsu) {
    return (gsu->clsr & 1) ? GSU_CLOCK_HIGH : GSU_CLOCK_NORMAL;
}

static void gsu_step(Gsu *gsu, int n) {
    int mult = (gsu_clockspeed(gsu) == GSU_CLOCK_HIGH) ? 1 : 2;
    gsu->cycles += n * mult;
}

static void gsu_reset_prefix(Gsu *gsu) {
    gsu->sreg = 0;
    gsu->dreg = 0;
    gsu->sreg_set = false;
    gsu->dreg_set = false;
    gsu->with_flag = false;
    gsu->flag_b = false;
    gsu->flag_alt1 = false;
    gsu->flag_alt2 = false;
}

static void gsu_update_nz(Gsu *gsu, uint16_t val) {
    gsu->flag_z = (val == 0);
    gsu->flag_s = (val & 0x8000) != 0;
}

/* ── memory access from GSU side ──────────────────────────── */

/*
 * Convert a GSU ROM address (bank << 16 | offset) to a file offset.
 * SuperFX LoROM carts use 32KB banks, so the mapping is:
 *   file_offset = bank * 0x8000 + (addr & 0x7FFF)
 */
static uint32_t gsu_rom_map(uint32_t addr) {
    uint8_t bank = (uint8_t)(addr >> 16);
    uint16_t offset = (uint16_t)(addr & 0xFFFF);
    /* LoROM SuperFX: 32KB banks */
    return (uint32_t)bank * 0x8000 + (offset & 0x7FFF);
}

static uint8_t gsu_rom_read(Gsu *gsu, uint32_t addr) {
    if (!gsu->rom) return 0;
    uint32_t mapped = gsu_rom_map(addr);
    return gsu->rom[mapped & (gsu->rom_size - 1)];
}

static uint8_t gsu_ram_read(Gsu *gsu, uint16_t addr) {
    if (!gsu->ram) return 0;
    return gsu->ram[addr & (gsu->ram_size - 1)];
}

static void gsu_ram_write(Gsu *gsu, uint16_t addr, uint8_t val) {
    if (!gsu->ram) return;
    gsu->ram[addr & (gsu->ram_size - 1)] = val;
}

/* ROM buffer read (async — starts read, returns buffered value) */
static uint8_t gsu_rom_buffer_read(Gsu *gsu) {
    return gsu->romdr;
}

static void gsu_rom_buffer_update(Gsu *gsu) {
    uint32_t addr = ((uint32_t)gsu->rombr << 16) | gsu->r[14];
    gsu->romdr = gsu_rom_read(gsu, addr);
}

/* RAM buffer */
static uint8_t gsu_ram_buffer_read(Gsu *gsu) {
    return gsu->ramdr;
}

static void gsu_ram_buffer_load(Gsu *gsu, uint16_t addr) {
    gsu->ramar = addr;
    gsu->ramdr = gsu_ram_read(gsu, ((uint32_t)gsu->rambr << 16) | addr);
}

static void gsu_ram_buffer_store(Gsu *gsu, uint16_t addr, uint8_t val) {
    gsu->ramar = addr;
    gsu->ramdr = val;
    gsu_ram_write(gsu, ((uint32_t)gsu->rambr << 16) | addr, val);
}

/* ── instruction cache ────────────────────────────────────── */

static void gsu_cache_flush(Gsu *gsu) {
    memset(gsu->cache_valid, 0, sizeof(gsu->cache_valid));
}

static uint8_t gsu_cache_fetch(Gsu *gsu, uint16_t addr) {
    uint16_t offset = (addr - gsu->cbr) & 0x1FF;
    int line = offset >> 4;

    if (!gsu->cache_valid[line]) {
        /* populate the 16-byte cache line from ROM */
        uint16_t line_base = (gsu->cbr + (line << 4)) & 0xFFFF;
        uint32_t rom_addr = ((uint32_t)gsu->pbr << 16) | line_base;
        for (int i = 0; i < 16; i++) {
            gsu->cache[((line << 4) | i) & 0x1FF] =
                gsu_rom_read(gsu, rom_addr + i);
        }
        gsu->cache_valid[line] = true;
        gsu_step(gsu, 16); /* cache fill penalty */
    }

    return gsu->cache[(offset) & 0x1FF];
}

/* ── pipeline ─────────────────────────────────────────────── */

static uint8_t gsu_pipe(Gsu *gsu) {
    uint8_t op = gsu->pipeline;
    uint16_t addr = PC++;
    /* Check if next byte is within cache range */
    uint16_t next = PC;
    if (next >= gsu->cbr && next < (gsu->cbr + 512)) {
        gsu->pipeline = gsu_cache_fetch(gsu, next);
    } else {
        uint32_t rom_addr = ((uint32_t)gsu->pbr << 16) | next;
        gsu->pipeline = gsu_rom_read(gsu, rom_addr);
    }
    return op;
}

static uint8_t gsu_peekpipe(Gsu *gsu) {
    if (!gsu->pipeline_valid) {
        uint16_t addr = PC;
        if (addr >= gsu->cbr && addr < (gsu->cbr + 512)) {
            gsu->pipeline = gsu_cache_fetch(gsu, addr);
        } else {
            uint32_t rom_addr = ((uint32_t)gsu->pbr << 16) | addr;
            gsu->pipeline = gsu_rom_read(gsu, rom_addr);
        }
        gsu->pipeline_valid = true;
    }
    return gsu->pipeline;
}

/* ── pixel cache & plot ───────────────────────────────────── */

static int gsu_get_bpp(Gsu *gsu) {
    switch (SCMR_MD) {
        case 0: return 2;
        case 1: return 4;
        case 2: return 4; /* reserved, treated as 4 */
        case 3: return 8;
    }
    return 2;
}

int gsu_screen_bpp(Gsu *gsu) {
    return gsu_get_bpp(gsu);
}

int gsu_screen_height(Gsu *gsu) {
    int ht = SCMR_HT;
    switch (ht) {
        case 0: return 128;
        case 1: return 160;
        case 2: return 192;
        case 3: return 128; /* reserved */
        case 4: return 256; /* obj mode */
        default: return 128;
    }
}

static void gsu_flush_pixcache(Gsu *gsu, int n) {
    GsuPixelCache *pc = &gsu->pixcache[n];
    if (pc->bitpend == 0) return;

    int bpp = gsu_get_bpp(gsu);
    uint16_t addr = pc->offset;

    /* read-modify-write: read existing pixels, merge, write back */
    for (int bp = 0; bp < bpp; bp++) {
        uint16_t vram_addr = addr + ((bp >> 1) * 16) + (bp & 1);
        uint8_t byte = gsu_ram_read(gsu, vram_addr);

        for (int px = 0; px < 8; px++) {
            if (pc->bitpend & (1 << px)) {
                uint8_t bit = (pc->data[px] >> bp) & 1;
                byte = (byte & ~(0x80 >> px)) | (bit << (7 - px));
            }
        }

        gsu_ram_write(gsu, vram_addr, byte);
    }

    pc->bitpend = 0;
}

static void gsu_plot(Gsu *gsu, uint16_t x, uint16_t y) {
    uint8_t color = gsu->colr;

    /* transparency check */
    if (POR_TRANSPARENT && color == 0) return;

    /* dither */
    if (POR_DITHER && ((x ^ y) & 1)) {
        color >>= 4;
    }

    /* compute pixel cache offset */
    int bpp = gsu_get_bpp(gsu);
    int screen_h = gsu_screen_height(gsu);

    /* char offset = screen_base + (y/8)*screen_width_chars*bpp_bytes + (x/8)*bpp_bytes + (y%8)*2 */
    int scr_base = (uint16_t)gsu->scbr << 10;
    int char_w = (bpp == 2) ? 16 : 32;  /* bytes per 8x8 char */
    int scr_w_chars;

    if (POR_OBJ_MODE) {
        scr_w_chars = 16; /* 128 pixels / 8 = 16 chars */
    } else {
        /* depends on screen height mode, assuming standard for now */
        scr_w_chars = (screen_h == 256) ? 32 : screen_h / 8;
        scr_w_chars = 32; /* most games use 256-pixel wide screen */
    }

    uint16_t offset = scr_base
        + ((y >> 3) * scr_w_chars + (x >> 3)) * char_w
        + (y & 7) * 2;

    int px_index = x & 7;

    /* check if we can use an existing cache entry */
    for (int i = 0; i < 2; i++) {
        if (gsu->pixcache[i].offset == offset && gsu->pixcache[i].bitpend) {
            gsu->pixcache[i].data[px_index] = color;
            gsu->pixcache[i].bitpend |= (1 << px_index);
            return;
        }
    }

    /* use empty entry */
    for (int i = 0; i < 2; i++) {
        if (gsu->pixcache[i].bitpend == 0) {
            gsu->pixcache[i].offset = offset;
            gsu->pixcache[i].data[px_index] = color;
            gsu->pixcache[i].bitpend = (1 << px_index);
            return;
        }
    }

    /* both entries occupied — flush entry 0, reuse it */
    gsu_flush_pixcache(gsu, 0);
    gsu->pixcache[0] = gsu->pixcache[1];
    gsu->pixcache[1].offset = offset;
    gsu->pixcache[1].data[px_index] = color;
    gsu->pixcache[1].bitpend = (1 << px_index);
}

static uint8_t gsu_rpix(Gsu *gsu, uint16_t x, uint16_t y) {
    /* flush pixel cache first to ensure data is in RAM */
    gsu_flush_pixcache(gsu, 0);
    gsu_flush_pixcache(gsu, 1);

    int bpp = gsu_get_bpp(gsu);
    int scr_base = (uint16_t)gsu->scbr << 10;
    int char_w = (bpp == 2) ? 16 : 32;
    int scr_w_chars = 32;

    uint16_t offset = scr_base
        + ((y >> 3) * scr_w_chars + (x >> 3)) * char_w
        + (y & 7) * 2;

    int px_index = x & 7;
    uint8_t color = 0;

    for (int bp = 0; bp < bpp; bp++) {
        uint16_t vram_addr = offset + ((bp >> 1) * 16) + (bp & 1);
        uint8_t byte = gsu_ram_read(gsu, vram_addr);
        color |= ((byte >> (7 - px_index)) & 1) << bp;
    }

    return color;
}

/* ── instruction implementations ──────────────────────────── */

/* STOP — halt GSU execution */
static void gsu_op_stop(Gsu *gsu) {
    gsu->flag_go = false;
    gsu->flag_irq = !(CFGR_IRQ);
    gsu_flush_pixcache(gsu, 0);
    gsu_flush_pixcache(gsu, 1);
    gsu_step(gsu, 1);
}

/* NOP */
static void gsu_op_nop(Gsu *gsu) {
    gsu_step(gsu, 1);
}

/* CACHE — set cache base to current PC & 0xFFF0 */
static void gsu_op_cache(Gsu *gsu) {
    uint16_t new_cbr = PC & 0xFFF0;
    if (gsu->cbr != new_cbr) {
        gsu->cbr = new_cbr;
        gsu_cache_flush(gsu);
    }
    gsu_step(gsu, 1);
}

/* LSR — logical shift right */
static void gsu_op_lsr(Gsu *gsu) {
    uint16_t val = SREG;
    gsu->flag_cy = val & 1;
    uint16_t result = val >> 1;
    DREG = result;
    gsu_update_nz(gsu, result);
    gsu_step(gsu, 1);
}

/* ROL — rotate left through carry */
static void gsu_op_rol(Gsu *gsu) {
    uint16_t val = SREG;
    bool old_cy = gsu->flag_cy;
    gsu->flag_cy = (val & 0x8000) != 0;
    uint16_t result = (val << 1) | (old_cy ? 1 : 0);
    DREG = result;
    gsu_update_nz(gsu, result);
    gsu_step(gsu, 1);
}

/* ROR — rotate right through carry */
static void gsu_op_ror(Gsu *gsu) {
    uint16_t val = SREG;
    bool old_cy = gsu->flag_cy;
    gsu->flag_cy = val & 1;
    uint16_t result = (val >> 1) | (old_cy ? 0x8000 : 0);
    DREG = result;
    gsu_update_nz(gsu, result);
    gsu_step(gsu, 1);
}

/* BRA — unconditional branch (signed 8-bit offset) */
static void gsu_op_bra(Gsu *gsu) {
    int8_t offset = (int8_t)gsu_pipe(gsu);
    PC += offset;
    gsu_step(gsu, 1);
}

/* BGE — branch if greater or equal (N == V, approximated as !S || Z) */
static void gsu_op_bge(Gsu *gsu) {
    int8_t offset = (int8_t)gsu_pipe(gsu);
    if (gsu->flag_s == gsu->flag_ov) PC += offset;
    gsu_step(gsu, 1);
}

/* BLT — branch if less than */
static void gsu_op_blt(Gsu *gsu) {
    int8_t offset = (int8_t)gsu_pipe(gsu);
    if (gsu->flag_s != gsu->flag_ov) PC += offset;
    gsu_step(gsu, 1);
}

/* BNE — branch if not equal (Z clear) */
static void gsu_op_bne(Gsu *gsu) {
    int8_t offset = (int8_t)gsu_pipe(gsu);
    if (!gsu->flag_z) PC += offset;
    gsu_step(gsu, 1);
}

/* BEQ — branch if equal (Z set) */
static void gsu_op_beq(Gsu *gsu) {
    int8_t offset = (int8_t)gsu_pipe(gsu);
    if (gsu->flag_z) PC += offset;
    gsu_step(gsu, 1);
}

/* BPL — branch if plus (S clear) */
static void gsu_op_bpl(Gsu *gsu) {
    int8_t offset = (int8_t)gsu_pipe(gsu);
    if (!gsu->flag_s) PC += offset;
    gsu_step(gsu, 1);
}

/* BMI — branch if minus (S set) */
static void gsu_op_bmi(Gsu *gsu) {
    int8_t offset = (int8_t)gsu_pipe(gsu);
    if (gsu->flag_s) PC += offset;
    gsu_step(gsu, 1);
}

/* BCC — branch if carry clear */
static void gsu_op_bcc(Gsu *gsu) {
    int8_t offset = (int8_t)gsu_pipe(gsu);
    if (!gsu->flag_cy) PC += offset;
    gsu_step(gsu, 1);
}

/* BCS — branch if carry set */
static void gsu_op_bcs(Gsu *gsu) {
    int8_t offset = (int8_t)gsu_pipe(gsu);
    if (gsu->flag_cy) PC += offset;
    gsu_step(gsu, 1);
}

/* BVC — branch if overflow clear */
static void gsu_op_bvc(Gsu *gsu) {
    int8_t offset = (int8_t)gsu_pipe(gsu);
    if (!gsu->flag_ov) PC += offset;
    gsu_step(gsu, 1);
}

/* BVS — branch if overflow set */
static void gsu_op_bvs(Gsu *gsu) {
    int8_t offset = (int8_t)gsu_pipe(gsu);
    if (gsu->flag_ov) PC += offset;
    gsu_step(gsu, 1);
}

/* TO Rn — set destination register */
static void gsu_op_to(Gsu *gsu, int n) {
    if (gsu->flag_b) {
        /* MOVE Rn, SREG */
        gsu->r[n] = SREG;
        gsu_update_nz(gsu, gsu->r[n]);
        gsu_reset_prefix(gsu);
    } else {
        gsu->dreg = n;
        gsu->dreg_set = true;
    }
    gsu_step(gsu, 1);
}

/* WITH Rn — set both source and destination */
static void gsu_op_with(Gsu *gsu, int n) {
    gsu->sreg = n;
    gsu->dreg = n;
    gsu->sreg_set = true;
    gsu->dreg_set = true;
    gsu->with_flag = true;
    gsu->flag_b = true;
    gsu_step(gsu, 1);
}

/* FROM Rn — set source register */
static void gsu_op_from(Gsu *gsu, int n) {
    if (gsu->flag_b) {
        /* MOVES DREG, Rn */
        DREG = gsu->r[n];
        gsu->flag_ov = (gsu->r[n] & 0x80) != 0;
        gsu->flag_s = (gsu->r[n] & 0x8000) != 0;
        gsu->flag_z = (gsu->r[n] == 0);
        gsu_reset_prefix(gsu);
    } else {
        gsu->sreg = n;
        gsu->sreg_set = true;
    }
    gsu_step(gsu, 1);
}

/* STW (Rn) — store word to RAM at Rn */
static void gsu_op_stw(Gsu *gsu, int n) {
    uint16_t addr = gsu->r[n];
    gsu_ram_buffer_store(gsu, addr, (uint8_t)(SREG & 0xFF));
    gsu_ram_buffer_store(gsu, addr + 1, (uint8_t)(SREG >> 8));
    gsu_step(gsu, 1);
}

/* STB (Rn) — store byte to RAM at Rn */
static void gsu_op_stb(Gsu *gsu, int n) {
    gsu_ram_buffer_store(gsu, gsu->r[n], (uint8_t)(SREG & 0xFF));
    gsu_step(gsu, 1);
}

/* LDW (Rn) — load word from RAM at Rn */
static void gsu_op_ldw(Gsu *gsu, int n) {
    uint16_t addr = gsu->r[n];
    uint8_t lo = gsu_ram_read(gsu, ((uint32_t)gsu->rambr << 16) | addr);
    uint8_t hi = gsu_ram_read(gsu, ((uint32_t)gsu->rambr << 16) | (uint16_t)(addr + 1));
    DREG = (uint16_t)(lo | (hi << 8));
    gsu_update_nz(gsu, DREG);
    gsu_step(gsu, 1);
}

/* LDB (Rn) — load byte from RAM at Rn */
static void gsu_op_ldb(Gsu *gsu, int n) {
    uint16_t addr = gsu->r[n];
    DREG = gsu_ram_read(gsu, ((uint32_t)gsu->rambr << 16) | addr);
    gsu_update_nz(gsu, DREG);
    gsu_step(gsu, 1);
}

/* PLOT — plot pixel at (R1, R2) with COLR, then R1++ */
static void gsu_op_plot(Gsu *gsu) {
    gsu_plot(gsu, gsu->r[1], gsu->r[2]);
    gsu->r[1]++;
    gsu_step(gsu, 1);
}

/* RPIX — read pixel at (R1, R2) into DREG */
static void gsu_op_rpix(Gsu *gsu) {
    DREG = gsu_rpix(gsu, gsu->r[1], gsu->r[2]);
    gsu_update_nz(gsu, DREG);
    gsu_step(gsu, 1);
}

/* COLOR / CMODE */
static void gsu_op_color(Gsu *gsu) {
    if (gsu->flag_alt1) {
        /* CMODE — set plot option register */
        gsu->por = (uint8_t)(SREG & 0xFF);
    } else {
        /* COLOR — set color register */
        uint8_t c = (uint8_t)(SREG & 0xFF);
        if (POR_COLOR_HIGH) {
            gsu->colr = (gsu->colr & 0x0F) | (c << 4);
        } else if (POR_FREEZE_HIGH) {
            gsu->colr = (gsu->colr & 0xF0) | (c & 0x0F);
        } else {
            gsu->colr = c;
        }
    }
    gsu_step(gsu, 1);
}

/* GETC — load color from ROM at R14 address via ROM buffer */
static void gsu_op_getc(Gsu *gsu) {
    uint8_t c = gsu->romdr;
    if (POR_COLOR_HIGH) {
        gsu->colr = (gsu->colr & 0x0F) | (c << 4);
    } else if (POR_FREEZE_HIGH) {
        gsu->colr = (gsu->colr & 0xF0) | (c & 0x0F);
    } else {
        gsu->colr = c;
    }
    gsu_step(gsu, 1);
}

/* GETB — load byte from ROM buffer into DREG (low byte) */
static void gsu_op_getb(Gsu *gsu) {
    DREG = gsu->romdr;
    gsu_update_nz(gsu, DREG);
    gsu_step(gsu, 1);
}

/* GETBH — GETB but into high byte */
static void gsu_op_getbh(Gsu *gsu) {
    DREG = (gsu->romdr << 8) | (SREG & 0xFF);
    gsu_update_nz(gsu, DREG);
    gsu_step(gsu, 1);
}

/* GETBL — GETB but keep high byte */
static void gsu_op_getbl(Gsu *gsu) {
    DREG = (SREG & 0xFF00) | gsu->romdr;
    gsu_update_nz(gsu, DREG);
    gsu_step(gsu, 1);
}

/* GETBS — GETB sign-extended */
static void gsu_op_getbs(Gsu *gsu) {
    DREG = (int16_t)(int8_t)gsu->romdr;
    gsu_update_nz(gsu, DREG);
    gsu_step(gsu, 1);
}

/* ADD Rn / ADC Rn / ADD #n / ADC #n */
static void gsu_op_add(Gsu *gsu, int n) {
    uint16_t a = SREG;
    uint16_t b;
    bool carry_in = false;

    if (gsu->flag_alt1 && gsu->flag_alt2) {
        /* ADC #n */
        b = n;
        carry_in = gsu->flag_cy;
    } else if (gsu->flag_alt1) {
        /* ADC Rn */
        b = gsu->r[n];
        carry_in = gsu->flag_cy;
    } else if (gsu->flag_alt2) {
        /* ADD #n */
        b = n;
    } else {
        /* ADD Rn */
        b = gsu->r[n];
    }

    uint32_t result = (uint32_t)a + (uint32_t)b + (carry_in ? 1 : 0);
    DREG = (uint16_t)result;
    gsu->flag_cy = (result > 0xFFFF);
    gsu->flag_ov = (~(a ^ b) & (a ^ result) & 0x8000) != 0;
    gsu_update_nz(gsu, DREG);
    gsu_step(gsu, 1);
}

/* SUB Rn / SBC Rn / SUB #n / CMP Rn */
static void gsu_op_sub(Gsu *gsu, int n) {
    uint16_t a = SREG;
    uint16_t b;
    bool carry_in = false;

    if (gsu->flag_alt1 && gsu->flag_alt2) {
        /* CMP Rn */
        b = gsu->r[n];
        int32_t result = (int32_t)a - (int32_t)b;
        gsu->flag_cy = (a >= b);
        gsu->flag_ov = ((a ^ b) & (a ^ (uint16_t)result) & 0x8000) != 0;
        gsu_update_nz(gsu, (uint16_t)result);
        /* CMP does NOT write result to DREG */
        gsu_step(gsu, 1);
        return;
    } else if (gsu->flag_alt1) {
        /* SBC Rn */
        b = gsu->r[n];
        carry_in = !gsu->flag_cy; /* borrow */
    } else if (gsu->flag_alt2) {
        /* SUB #n */
        b = n;
    } else {
        /* SUB Rn */
        b = gsu->r[n];
    }

    uint32_t result = (uint32_t)a - (uint32_t)b - (carry_in ? 1 : 0);
    DREG = (uint16_t)result;
    gsu->flag_cy = (a >= b + (carry_in ? 1 : 0));
    gsu->flag_ov = ((a ^ b) & (a ^ DREG) & 0x8000) != 0;
    gsu_update_nz(gsu, DREG);
    gsu_step(gsu, 1);
}

/* AND Rn / BIC Rn / AND #n / BIC #n */
static void gsu_op_and(Gsu *gsu, int n) {
    uint16_t b;
    if (gsu->flag_alt2) {
        b = n;
    } else {
        b = gsu->r[n];
    }

    if (gsu->flag_alt1) {
        /* BIC (bit clear) */
        DREG = SREG & ~b;
    } else {
        DREG = SREG & b;
    }
    gsu_update_nz(gsu, DREG);
    gsu_step(gsu, 1);
}

/* OR Rn / XOR Rn / OR #n / XOR #n */
static void gsu_op_or(Gsu *gsu, int n) {
    uint16_t b;
    if (gsu->flag_alt2) {
        b = n;
    } else {
        b = gsu->r[n];
    }

    if (gsu->flag_alt1) {
        /* XOR */
        DREG = SREG ^ b;
    } else {
        DREG = SREG | b;
    }
    gsu_update_nz(gsu, DREG);
    gsu_step(gsu, 1);
}

/* MULT / UMULT */
static void gsu_op_mult(Gsu *gsu, int n) {
    uint16_t b;
    if (gsu->flag_alt2) {
        b = n;  /* immediate */
    } else {
        b = gsu->r[n];
    }

    if (gsu->flag_alt1) {
        /* UMULT — unsigned */
        DREG = (uint16_t)((SREG & 0xFF) * (b & 0xFF));
    } else {
        /* MULT — signed */
        DREG = (uint16_t)((int16_t)(int8_t)(SREG & 0xFF) * (int16_t)(int8_t)(b & 0xFF));
    }
    gsu_update_nz(gsu, DREG);
    gsu_step(gsu, CFGR_MS0 ? 1 : 2);  /* multiply takes extra cycle */
}

/* FMULT / LMULT */
static void gsu_op_fmult(Gsu *gsu) {
    uint32_t result;
    if (gsu->flag_alt1) {
        /* LMULT — 16x16 = 32, low word to R4, high word to DREG */
        result = (uint32_t)(int32_t)((int16_t)SREG * (int16_t)gsu->r[6]);
        gsu->r[4] = (uint16_t)result;
        DREG = (uint16_t)(result >> 16);
    } else {
        /* FMULT — fractional multiply, top 16 bits of 16x16 */
        result = (uint32_t)(int32_t)((int16_t)SREG * (int16_t)gsu->r[6]);
        DREG = (uint16_t)(result >> 16);
        gsu->flag_cy = (result & 0x8000) != 0;
    }
    gsu_update_nz(gsu, DREG);
    gsu_step(gsu, CFGR_MS0 ? 3 : 6);
}

/* IWT Rn, #imm16 / LM Rn, (imm16) / SM (imm16), Rn / LMS Rn, (imm8*2) / SMS (imm8*2), Rn */
static void gsu_op_iwt(Gsu *gsu, int n) {
    if (gsu->flag_alt1 && gsu->flag_alt2) {
        /* SMS (Rn), (xx) — store Rn to RAM at imm8*2 */
        uint8_t imm = gsu_pipe(gsu);
        uint16_t addr = (uint16_t)imm << 1;
        gsu_ram_buffer_store(gsu, addr, (uint8_t)(gsu->r[n] & 0xFF));
        gsu_ram_buffer_store(gsu, addr + 1, (uint8_t)(gsu->r[n] >> 8));
        gsu_step(gsu, 1);
    } else if (gsu->flag_alt1) {
        /* LM Rn, (xx) — load Rn from RAM at imm16 */
        uint8_t lo = gsu_pipe(gsu);
        uint8_t hi = gsu_pipe(gsu);
        uint16_t addr = (uint16_t)(lo | (hi << 8));
        gsu_ram_buffer_load(gsu, addr);
        uint8_t a = gsu->ramdr;
        gsu_ram_buffer_load(gsu, addr + 1);
        uint8_t b = gsu->ramdr;
        gsu->r[n] = (uint16_t)(a | (b << 8));
        gsu_update_nz(gsu, gsu->r[n]);
        gsu_step(gsu, 1);
    } else if (gsu->flag_alt2) {
        /* SM (xx), Rn — store Rn to RAM at imm16 */
        uint8_t lo = gsu_pipe(gsu);
        uint8_t hi = gsu_pipe(gsu);
        uint16_t addr = (uint16_t)(lo | (hi << 8));
        gsu_ram_buffer_store(gsu, addr, (uint8_t)(gsu->r[n] & 0xFF));
        gsu_ram_buffer_store(gsu, addr + 1, (uint8_t)(gsu->r[n] >> 8));
        gsu_step(gsu, 1);
    } else {
        /* IWT Rn, #imm16 — load immediate word */
        uint8_t lo = gsu_pipe(gsu);
        uint8_t hi = gsu_pipe(gsu);
        gsu->r[n] = (uint16_t)(lo | (hi << 8));
        gsu_step(gsu, 1);
    }
}

/* LMS Rn, (imm8*2) — short form of LM */
static void gsu_op_lms(Gsu *gsu, int n) {
    uint8_t imm = gsu_pipe(gsu);
    uint16_t addr = (uint16_t)imm << 1;
    gsu_ram_buffer_load(gsu, addr);
    uint8_t a = gsu->ramdr;
    gsu_ram_buffer_load(gsu, addr + 1);
    uint8_t b = gsu->ramdr;
    gsu->r[n] = (uint16_t)(a | (b << 8));
    gsu_update_nz(gsu, gsu->r[n]);
    gsu_step(gsu, 1);
}

/* IBT Rn, #imm8 / LMS Rn, (xx) / SMS (xx), Rn */
static void gsu_op_ibt(Gsu *gsu, int n) {
    if (gsu->flag_alt1 && gsu->flag_alt2) {
        /* SMS (xx), Rn */
        uint8_t imm = gsu_pipe(gsu);
        uint16_t addr = (uint16_t)imm << 1;
        gsu_ram_buffer_store(gsu, addr, (uint8_t)(gsu->r[n] & 0xFF));
        gsu_ram_buffer_store(gsu, addr + 1, (uint8_t)(gsu->r[n] >> 8));
    } else if (gsu->flag_alt1) {
        /* LMS Rn, (xx) */
        uint8_t imm = gsu_pipe(gsu);
        uint16_t addr = (uint16_t)imm << 1;
        gsu_ram_buffer_load(gsu, addr);
        uint8_t a = gsu->ramdr;
        gsu_ram_buffer_load(gsu, addr + 1);
        uint8_t b = gsu->ramdr;
        gsu->r[n] = (uint16_t)(a | (b << 8));
        gsu_update_nz(gsu, gsu->r[n]);
    } else if (gsu->flag_alt2) {
        /* SMS (xx), Rn */
        uint8_t imm = gsu_pipe(gsu);
        uint16_t addr = (uint16_t)imm << 1;
        gsu_ram_buffer_store(gsu, addr, (uint8_t)(gsu->r[n] & 0xFF));
        gsu_ram_buffer_store(gsu, addr + 1, (uint8_t)(gsu->r[n] >> 8));
    } else {
        /* IBT Rn, #imm8 — load immediate byte (sign-extended) */
        int8_t imm = (int8_t)gsu_pipe(gsu);
        gsu->r[n] = (uint16_t)(int16_t)imm;
    }
    gsu_step(gsu, 1);
}

/* INC Rn / DEC Rn */
static void gsu_op_inc(Gsu *gsu, int n) {
    gsu->r[n]++;
    gsu_update_nz(gsu, gsu->r[n]);
    gsu_step(gsu, 1);
}

static void gsu_op_dec(Gsu *gsu, int n) {
    gsu->r[n]--;
    gsu_update_nz(gsu, gsu->r[n]);
    gsu_step(gsu, 1);
}

/* JMP Rn / LJMP Rn */
static void gsu_op_jmp(Gsu *gsu, int n) {
    if (gsu->flag_alt1) {
        /* LJMP — set PBR from SREG, PC from Rn */
        gsu->pbr = (uint8_t)(SREG & 0x7F);
        PC = gsu->r[n];
        gsu->cbr = PC & 0xFFF0;
        gsu_cache_flush(gsu);
    } else {
        PC = gsu->r[n];
    }
    gsu_step(gsu, 1);
}

/* LOOP — decrement R12, branch to R13 if not zero */
static void gsu_op_loop(Gsu *gsu) {
    gsu->r[12]--;
    gsu_update_nz(gsu, gsu->r[12]);
    if (!gsu->flag_z) {
        PC = gsu->r[13];
    }
    gsu_step(gsu, 1);
}

/* LINK #n — R11 = PC + n */
static void gsu_op_link(Gsu *gsu, int n) {
    gsu->r[11] = PC + n;
    gsu_step(gsu, 1);
}

/* SEX — sign-extend byte to word */
static void gsu_op_sex(Gsu *gsu) {
    DREG = (uint16_t)(int16_t)(int8_t)(SREG & 0xFF);
    gsu_update_nz(gsu, DREG);
    gsu_step(gsu, 1);
}

/* ASR / DIV2 */
static void gsu_op_asr(Gsu *gsu) {
    if (gsu->flag_alt1) {
        /* DIV2 — arithmetic shift right with round-to-zero */
        int16_t val = (int16_t)SREG;
        gsu->flag_cy = val & 1;
        if (val == -1) {
            DREG = 0;
        } else {
            DREG = (uint16_t)(val >> 1);
        }
    } else {
        gsu->flag_cy = SREG & 1;
        DREG = (uint16_t)((int16_t)SREG >> 1);
    }
    gsu_update_nz(gsu, DREG);
    gsu_step(gsu, 1);
}

/* NOT */
static void gsu_op_not(Gsu *gsu) {
    DREG = ~SREG;
    gsu_update_nz(gsu, DREG);
    gsu_step(gsu, 1);
}

/* SWAP — swap high/low bytes */
static void gsu_op_swap(Gsu *gsu) {
    DREG = (SREG >> 8) | (SREG << 8);
    gsu_update_nz(gsu, DREG);
    gsu_step(gsu, 1);
}

/* LOB — low byte (clear high byte) */
static void gsu_op_lob(Gsu *gsu) {
    DREG = SREG & 0xFF;
    gsu_update_nz(gsu, DREG);
    gsu->flag_s = (DREG & 0x80) != 0; /* 8-bit sign for LOB */
    gsu_step(gsu, 1);
}

/* HIB — high byte (move to low, clear high) */
static void gsu_op_hib(Gsu *gsu) {
    DREG = SREG >> 8;
    gsu_update_nz(gsu, DREG);
    gsu->flag_s = (DREG & 0x80) != 0; /* 8-bit sign for HIB */
    gsu_step(gsu, 1);
}

/* MERGE — merge R7 (high) and R8 (low) nibbles */
static void gsu_op_merge(Gsu *gsu) {
    DREG = ((gsu->r[7] & 0xFF00) | (gsu->r[8] >> 8));
    gsu->flag_s = (DREG & 0x8080) != 0;
    gsu->flag_z = (DREG & 0xF0F0) != 0;  /* unusual Z flag for MERGE */
    gsu->flag_cy = (DREG & 0xE0E0) != 0;
    gsu->flag_ov = (DREG & 0xC0C0) != 0;
    gsu_step(gsu, 1);
}

/* SBK — store byte to last RAM address */
static void gsu_op_sbk(Gsu *gsu) {
    gsu_ram_buffer_store(gsu, gsu->ramar, (uint8_t)(SREG & 0xFF));
    gsu_ram_buffer_store(gsu, gsu->ramar + 1, (uint8_t)(SREG >> 8));
    gsu_step(gsu, 1);
}

/* ROMB — set ROM bank register */
static void gsu_op_romb(Gsu *gsu) {
    gsu->rombr = (uint8_t)(SREG & 0x7F);
    gsu_step(gsu, 1);
}

/* RAMB — set RAM bank register */
static void gsu_op_ramb(Gsu *gsu) {
    gsu->rambr = (uint8_t)(SREG & 0x01);
    gsu_step(gsu, 1);
}

/* ALT1, ALT2, ALT3 — instruction prefix modifiers */
static void gsu_op_alt1(Gsu *gsu) {
    gsu->flag_alt1 = true;
    gsu_step(gsu, 1);
}

static void gsu_op_alt2(Gsu *gsu) {
    gsu->flag_alt2 = true;
    gsu_step(gsu, 1);
}

static void gsu_op_alt3(Gsu *gsu) {
    gsu->flag_alt1 = true;
    gsu->flag_alt2 = true;
    gsu_step(gsu, 1);
}

/* ── main instruction dispatch ────────────────────────────── */

static void gsu_execute(Gsu *gsu, uint8_t op) {
    bool r14_before = false;
    uint16_t r14_val = gsu->r[14];

    switch (op) {
    case 0x00: gsu_op_stop(gsu); return;
    case 0x01: gsu_op_nop(gsu); break;
    case 0x02: gsu_op_cache(gsu); break;
    case 0x03: gsu_op_lsr(gsu); break;
    case 0x04: gsu_op_rol(gsu); break;
    case 0x05: gsu_op_bra(gsu); break;
    case 0x06: gsu_op_bge(gsu); break;
    case 0x07: gsu_op_blt(gsu); break;
    case 0x08: gsu_op_bne(gsu); break;
    case 0x09: gsu_op_beq(gsu); break;
    case 0x0A: gsu_op_bpl(gsu); break;
    case 0x0B: gsu_op_bmi(gsu); break;
    case 0x0C: gsu_op_bcc(gsu); break;
    case 0x0D: gsu_op_bcs(gsu); break;
    case 0x0E: gsu_op_bvc(gsu); break;
    case 0x0F: gsu_op_bvs(gsu); break;

    /* $10-$1F: TO Rn / WITH Rn */
    case 0x10: case 0x11: case 0x12: case 0x13:
    case 0x14: case 0x15: case 0x16: case 0x17:
    case 0x18: case 0x19: case 0x1A: case 0x1B:
    case 0x1C: case 0x1D: case 0x1E: case 0x1F:
        if (gsu->flag_alt1) {
            gsu_op_with(gsu, op & 0x0F);
        } else {
            gsu_op_to(gsu, op & 0x0F);
        }
        return; /* don't reset prefix for TO/WITH */

    /* $20-$2F: FROM Rn */
    case 0x20: case 0x21: case 0x22: case 0x23:
    case 0x24: case 0x25: case 0x26: case 0x27:
    case 0x28: case 0x29: case 0x2A: case 0x2B:
    case 0x2C: case 0x2D: case 0x2E: case 0x2F:
        gsu_op_from(gsu, op & 0x0F);
        if (!gsu->flag_b) return; /* FROM as prefix — don't reset */
        break;

    /* $30-$3B: STW (Rn) / STB (Rn) */
    case 0x30: case 0x31: case 0x32: case 0x33:
    case 0x34: case 0x35: case 0x36: case 0x37:
    case 0x38: case 0x39: case 0x3A: case 0x3B:
        if (gsu->flag_alt1) {
            gsu_op_stb(gsu, op & 0x0F);
        } else {
            gsu_op_stw(gsu, op & 0x0F);
        }
        break;

    case 0x3C: gsu_op_loop(gsu); break;
    case 0x3D: gsu_op_alt1(gsu); return; /* ALT1 prefix — don't reset flags */
    case 0x3E: gsu_op_alt2(gsu); return; /* ALT2 prefix — don't reset flags */
    case 0x3F: gsu_op_alt3(gsu); return; /* ALT3 prefix — don't reset flags */

    /* NOTE: The old code had GETB/SBK/ROMB at $3D/$3E/$3F which was wrong.
     * Per the GSU spec (fullsnes, ares), $3D/$3E/$3F are ALT prefix opcodes.
     * GETB/GETBH/GETBL/GETBS are ALT-mode variants of $EF (GETC).
     * SBK is dispatched from the $90-$9F range.
     * ROMB/RAMB are dispatched from specific ALT modes of other opcodes. */

#if 0 /* Old incorrect dispatch — preserved for reference */
    case 0x3D_old:
        if (gsu->flag_alt1) {
            gsu_op_getbh(gsu);
        } else if (gsu->flag_alt2) {
            gsu_op_getbl(gsu);
        } else if (gsu->flag_alt1 && gsu->flag_alt2) {
            gsu_op_getbs(gsu);
        } else {
            gsu_op_getb(gsu);
        }
        break;
    case 0x3E_old: gsu_op_sbk(gsu); break;
    case 0x3F_old:
        if (gsu->flag_alt1) {
            gsu_op_romb(gsu);
        } else if (gsu->flag_alt2) {
            gsu_op_ramb(gsu);
        } else {
            gsu_op_nop(gsu);
        }
        break;
#endif /* Old incorrect dispatch */

    /* $40-$4B: LDW (Rn) / LDB (Rn) */
    case 0x40: case 0x41: case 0x42: case 0x43:
    case 0x44: case 0x45: case 0x46: case 0x47:
    case 0x48: case 0x49: case 0x4A: case 0x4B:
        if (gsu->flag_alt1) {
            gsu_op_ldb(gsu, op & 0x0F);
        } else {
            gsu_op_ldw(gsu, op & 0x0F);
        }
        break;

    case 0x4C: gsu_op_plot(gsu); break; /* PLOT / RPIX */
    case 0x4D: gsu_op_swap(gsu); break;
    case 0x4E: gsu_op_color(gsu); break; /* COLOR / CMODE */
    case 0x4F: gsu_op_not(gsu); break;

    /* $50-$5F: ADD Rn / ADC Rn / ADD #n / ADC #n */
    case 0x50: case 0x51: case 0x52: case 0x53:
    case 0x54: case 0x55: case 0x56: case 0x57:
    case 0x58: case 0x59: case 0x5A: case 0x5B:
    case 0x5C: case 0x5D: case 0x5E: case 0x5F:
        gsu_op_add(gsu, op & 0x0F);
        break;

    /* $60-$6F: SUB Rn / SBC Rn / SUB #n / CMP Rn */
    case 0x60: case 0x61: case 0x62: case 0x63:
    case 0x64: case 0x65: case 0x66: case 0x67:
    case 0x68: case 0x69: case 0x6A: case 0x6B:
    case 0x6C: case 0x6D: case 0x6E: case 0x6F:
        gsu_op_sub(gsu, op & 0x0F);
        break;

    /* $70-$7F: MERGE / AND / BIC / MULT / UMULT */
    case 0x70: gsu_op_merge(gsu); break;
    case 0x71: case 0x72: case 0x73:
    case 0x74: case 0x75: case 0x76: case 0x77:
    case 0x78: case 0x79: case 0x7A: case 0x7B:
    case 0x7C: case 0x7D: case 0x7E: case 0x7F:
        gsu_op_and(gsu, op & 0x0F);
        break;

    /* $80-$8F: MULT Rn / UMULT Rn */
    case 0x80: case 0x81: case 0x82: case 0x83:
    case 0x84: case 0x85: case 0x86: case 0x87:
    case 0x88: case 0x89: case 0x8A: case 0x8B:
    case 0x8C: case 0x8D: case 0x8E: case 0x8F:
        gsu_op_mult(gsu, op & 0x0F);
        break;

    /* $90-$9F: SBK, LINK, SEX, ASR, ROR, JMP, LOB, FMULT
     * Corrected from ares/fullsnes reference. These are individual
     * opcodes, not a register-indexed block. */
    case 0x90: gsu_op_sbk(gsu); break;                     /* SBK — store back */
    case 0x91: case 0x92: case 0x93: case 0x94:
        gsu_op_link(gsu, op & 0x0F); break;                /* LINK #1-4 */
    case 0x95: gsu_op_sex(gsu); break;                      /* SEX — sign extend */
    case 0x96:
        if (gsu->flag_alt1) {
            /* DIV2 — divide by 2 (signed) */
            int16_t val = (int16_t)SREG;
            DREG = (uint16_t)((val >> 1) + ((val + 1) >> 16));
            gsu_update_nz(gsu, DREG);
        } else {
            gsu_op_asr(gsu);                                /* ASR — arithmetic shift right */
        }
        gsu_step(gsu, 1);
        break;
    case 0x97:                                              /* ROR — rotate right through carry */
        {
            uint16_t carry_in = gsu->flag_cy ? 0x8000 : 0;
            gsu->flag_cy = SREG & 1;
            DREG = (SREG >> 1) | carry_in;
            gsu_update_nz(gsu, DREG);
            gsu_step(gsu, 1);
        }
        break;
    case 0x98: case 0x99: case 0x9A: case 0x9B:
    case 0x9C: case 0x9D:
        gsu_op_jmp(gsu, op & 0x0F); break;                 /* JMP R8-R13 / LJMP(ALT1) */
    case 0x9E:
        DREG = SREG & 0x00FF;                              /* LOB — low byte */
        gsu_update_nz(gsu, DREG);
        gsu_step(gsu, 1);
        break;
    case 0x9F: gsu_op_fmult(gsu); break;                   /* FMULT / LMULT(ALT1) */

    /* $A0-$AF: IBT Rn, #pp — Immediate Byte Transfer (2-byte instruction)
     * ALT1: LMS Rn, (yy) — load from short address (yy*2)
     * ALT2: SMS (yy), Rn — store to short address (yy*2)
     * NOTE: Was incorrectly mapped as IWT (3-byte) which consumed an extra byte! */
    case 0xA0: case 0xA1: case 0xA2: case 0xA3:
    case 0xA4: case 0xA5: case 0xA6: case 0xA7:
    case 0xA8: case 0xA9: case 0xAA: case 0xAB:
    case 0xAC: case 0xAD: case 0xAE: case 0xAF:
        gsu_op_ibt(gsu, op & 0x0F);
        break;

    /* $B0-$BF: OR Rn / XOR Rn (with ALT modes) */
    case 0xB0: case 0xB1: case 0xB2: case 0xB3:
    case 0xB4: case 0xB5: case 0xB6: case 0xB7:
    case 0xB8: case 0xB9: case 0xBA: case 0xBB:
    case 0xBC: case 0xBD: case 0xBE: case 0xBF:
        gsu_op_or(gsu, op & 0x0F);
        break;

    /* $C0-$CF: HIB / INC Rn / DEC Rn / LOB
     * C0=HIB, C1-CE=INC(normal)/DEC(ALT1), CF=FMULT or LOB */
    case 0xC0: gsu_op_hib(gsu); break;
    case 0xC1: case 0xC2: case 0xC3:
    case 0xC4: case 0xC5: case 0xC6: case 0xC7:
    case 0xC8: case 0xC9: case 0xCA: case 0xCB:
    case 0xCC: case 0xCD: case 0xCE:
        if (gsu->flag_alt1) {
            gsu_op_dec(gsu, op & 0x0F);
        } else {
            gsu_op_inc(gsu, op & 0x0F);
        }
        break;
    case 0xCF:
        /* GETC — get color from ROM buffer (same as $DF without ALT) */
        gsu_op_lob(gsu);
        break;

    /* $D0-$DE: GETC / RAMB / ROMB
     * Normal: GETC (transfer ROM buffer to color register)
     * ALT1: RAMB (set RAM bank from SREG)
     * ALT2: ROMB (set ROM bank from SREG) */
    case 0xD0: case 0xD1: case 0xD2: case 0xD3:
    case 0xD4: case 0xD5: case 0xD6: case 0xD7:
    case 0xD8: case 0xD9: case 0xDA: case 0xDB:
    case 0xDC: case 0xDD: case 0xDE:
        if (gsu->flag_alt1) {
            gsu_op_ramb(gsu);
        } else if (gsu->flag_alt2) {
            gsu_op_romb(gsu);
        } else {
            gsu_op_getc(gsu);
        }
        break;
    case 0xDF:
        /* GETB / GETBH / GETBL / GETBS */
        if (gsu->flag_alt1 && gsu->flag_alt2) {
            gsu_op_getbs(gsu);         /* ALT3: signed byte */
        } else if (gsu->flag_alt1) {
            gsu_op_getbh(gsu);         /* ALT1: high byte */
        } else if (gsu->flag_alt2) {
            gsu_op_getbl(gsu);         /* ALT2: low byte */
        } else {
            gsu_op_getb(gsu);          /* normal: byte */
        }
        break;

    /* $E0-$EF: INC R0-R15 / DEC (ALT1) — another INC block?
     * Actually: JMP Rn (normal) / LJMP (ALT1) for R0-R15 */
    case 0xE0: case 0xE1: case 0xE2: case 0xE3:
    case 0xE4: case 0xE5: case 0xE6: case 0xE7:
    case 0xE8: case 0xE9: case 0xEA: case 0xEB:
    case 0xEC: case 0xED: case 0xEE: case 0xEF:
        gsu_op_jmp(gsu, op & 0x0F);
        break;

    /* $F0-$FF: IWT R0-R15, #ppqq — Immediate Word Transfer (3-byte)
     * ALT1: LM R0-R15, (hhll) — load from RAM address
     * ALT2: SM (hhll), R0-R15 — store to RAM address
     * NOTE: Was mapped as SEX/LINK/NOP which was completely wrong! */
    case 0xF0: case 0xF1: case 0xF2: case 0xF3:
    case 0xF4: case 0xF5: case 0xF6: case 0xF7:
    case 0xF8: case 0xF9: case 0xFA: case 0xFB:
    case 0xFC: case 0xFD: case 0xFE: case 0xFF:
        gsu_op_iwt(gsu, op & 0x0F);
        break;
    }

    /* check if R14 was modified → update ROM buffer */
    if (gsu->r[14] != r14_val) {
        gsu_rom_buffer_update(gsu);
    }

    /* if R15 (PC) was NOT modified by instruction (branches do modify it),
       the pipeline already advanced it. For non-prefix opcodes, reset prefix. */
    if (op < 0x10 || op >= 0x30 || (op >= 0x20 && gsu->flag_b)) {
        /* only reset prefix for non-prefix opcodes */
        if (op != 0x3D && op != 0x3F) { /* ALT variants handled internally */
            gsu_reset_prefix(gsu);
        }
    }
}

/* ── public API ───────────────────────────────────────────── */

Gsu *gsu_init(void *snes) {
    Gsu *gsu = calloc(1, sizeof(Gsu));
    gsu->snes = snes;
    gsu->vcr = 0x04; /* GSU-2 version */
    return gsu;
}

void gsu_free(Gsu *gsu) {
    if (gsu) free(gsu);
}

void gsu_power(Gsu *gsu) {
    memset(gsu->r, 0, sizeof(gsu->r));
    gsu->sreg = 0;
    gsu->dreg = 0;
    gsu->sreg_set = false;
    gsu->dreg_set = false;
    gsu->with_flag = false;
    gsu->flag_z = false;
    gsu->flag_cy = false;
    gsu->flag_s = false;
    gsu->flag_ov = false;
    gsu->flag_go = false;
    gsu->flag_rom = false;
    gsu->flag_alt1 = false;
    gsu->flag_alt2 = false;
    gsu->flag_il = false;
    gsu->flag_ih = false;
    gsu->flag_b = false;
    gsu->flag_irq = false;
    gsu->pbr = 0;
    gsu->rombr = 0;
    gsu->rambr = 0;
    gsu->cbr = 0;
    gsu->scbr = 0;
    gsu->scmr = 0;
    gsu->colr = 0;
    gsu->por = 0;
    gsu->bramr = 0;
    gsu->cfgr = 0;
    gsu->clsr = 0;
    gsu->romdr = 0;
    gsu->ramar = 0;
    gsu->ramdr = 0;
    gsu->pipeline = 0;
    gsu->pipeline_valid = false;
    gsu->cycles = 0;
    memset(gsu->cache, 0, sizeof(gsu->cache));
    gsu_cache_flush(gsu);
    memset(gsu->pixcache, 0, sizeof(gsu->pixcache));
}

void gsu_reset(Gsu *gsu) {
    gsu_power(gsu);
}

void gsu_load(Gsu *gsu, uint8_t *rom, uint32_t rom_size,
              uint8_t *ram, uint32_t ram_size) {
    gsu->rom = rom;
    gsu->rom_size = rom_size;
    gsu->ram = ram;
    gsu->ram_size = ram_size;
}

/* Build SFR from flag bits */
static uint16_t gsu_get_sfr(Gsu *gsu) {
    uint16_t sfr = 0;
    sfr |= gsu->flag_z   ? 0x0002 : 0;
    sfr |= gsu->flag_cy  ? 0x0004 : 0;
    sfr |= gsu->flag_s   ? 0x0008 : 0;
    sfr |= gsu->flag_ov  ? 0x0010 : 0;
    sfr |= gsu->flag_go  ? 0x0020 : 0;
    sfr |= gsu->flag_rom ? 0x0040 : 0;
    sfr |= gsu->flag_alt1 ? 0x0100 : 0;
    sfr |= gsu->flag_alt2 ? 0x0200 : 0;
    sfr |= gsu->flag_il  ? 0x0400 : 0;
    sfr |= gsu->flag_ih  ? 0x0800 : 0;
    sfr |= gsu->flag_b   ? 0x1000 : 0;
    sfr |= gsu->flag_irq ? 0x8000 : 0;
    return sfr;
}

static void gsu_set_sfr(Gsu *gsu, uint16_t sfr) {
    gsu->flag_z    = (sfr & 0x0002) != 0;
    gsu->flag_cy   = (sfr & 0x0004) != 0;
    gsu->flag_s    = (sfr & 0x0008) != 0;
    gsu->flag_ov   = (sfr & 0x0010) != 0;
    gsu->flag_go   = (sfr & 0x0020) != 0;
    gsu->flag_rom  = (sfr & 0x0040) != 0;
    gsu->flag_alt1 = (sfr & 0x0100) != 0;
    gsu->flag_alt2 = (sfr & 0x0200) != 0;
    gsu->flag_il   = (sfr & 0x0400) != 0;
    gsu->flag_ih   = (sfr & 0x0800) != 0;
    gsu->flag_b    = (sfr & 0x1000) != 0;
    gsu->flag_irq  = (sfr & 0x8000) != 0;
}

/* I/O register read ($3000-$303F) from 65816 side */
uint8_t gsu_read(Gsu *gsu, uint16_t addr) {
    addr &= 0x3F;

    /* R0-R15: $3000-$301F (16-bit LE) */
    if (addr < 0x20) {
        int reg = addr >> 1;
        if (addr & 1) {
            return (uint8_t)(gsu->r[reg] >> 8);
        } else {
            return (uint8_t)(gsu->r[reg] & 0xFF);
        }
    }

    switch (addr) {
    case 0x30: return (uint8_t)(gsu_get_sfr(gsu) & 0xFF);
    case 0x31: {
        uint8_t val = (uint8_t)(gsu_get_sfr(gsu) >> 8);
        gsu->flag_irq = false;  /* reading SFR high clears IRQ */
        return val;
    }
    case 0x34: return gsu->pbr;
    case 0x36: return gsu->rombr;
    case 0x38: return gsu->scbr;
    case 0x39: return gsu->clsr;
    case 0x3A: return gsu->scmr;
    case 0x3B: return gsu->vcr;  /* always 0x04 for GSU-2 */
    case 0x3C: return gsu->rambr;
    case 0x3E: return gsu->cbr & 0xFF;
    case 0x3F: return gsu->cbr >> 8;
    }

    return 0;
}

/* I/O register write ($3000-$303F) from 65816 side */
void gsu_write(Gsu *gsu, uint16_t addr, uint8_t val) {
    addr &= 0x3F;

    /* R0-R15: $3000-$301F (16-bit LE) */
    if (addr < 0x20) {
        int reg = addr >> 1;
        if (addr & 1) {
            gsu->r[reg] = (gsu->r[reg] & 0x00FF) | ((uint16_t)val << 8);
            /* Writing R15 high byte starts GSU execution */
            if (reg == 15) {
                gsu->flag_go = true;
                gsu->pipeline_valid = false;
                gsu_run(gsu);
            }
        } else {
            gsu->r[reg] = (gsu->r[reg] & 0xFF00) | val;
        }
        return;
    }

    switch (addr) {
    case 0x30:
        gsu_set_sfr(gsu, (gsu_get_sfr(gsu) & 0xFF00) | val);
        break;
    case 0x31:
        gsu_set_sfr(gsu, (gsu_get_sfr(gsu) & 0x00FF) | ((uint16_t)val << 8));
        break;
    case 0x33: gsu->bramr = val; break;
    case 0x34: gsu->pbr = val & 0x7F; break;
    case 0x37: gsu->cfgr = val; break;
    case 0x38: gsu->scbr = val; break;
    case 0x39: gsu->clsr = val; break;
    case 0x3A: gsu->scmr = val; break;
    }
}

/* Cache RAM read ($3100-$32FF) */
uint8_t gsu_cache_read(Gsu *gsu, uint16_t addr) {
    return gsu->cache[(addr - 0x3100) & 0x1FF];
}

/* Cache RAM write ($3100-$32FF) */
void gsu_cache_write(Gsu *gsu, uint16_t addr, uint8_t val) {
    gsu->cache[(addr - 0x3100) & 0x1FF] = val;
}

/* Execute GSU until STOP or cycle limit */
void gsu_run(Gsu *gsu) {
    if (!gsu->flag_go) return;

    /* Set a generous cycle limit to prevent infinite loops.
     * SuperFX 3D render programs need millions of cycles to complete
     * a full frame (polygon rasterization, Z-buffer, etc.) */
    uint64_t start = gsu->cycles;
    uint64_t limit = 200000000;  /* 200M cycles — tile decompression needs many cycles */

    gsu_peekpipe(gsu);

    while (gsu->flag_go && (gsu->cycles - start) < limit) {
        uint8_t op = gsu_pipe(gsu);
        gsu_execute(gsu, op);
    }
}

void gsu_handleState(Gsu *gsu, StateHandler *sh) {
    /* save/load all GSU state */
    for (int i = 0; i < 16; i++) {
        sh_handleWords(sh, &gsu->r[i], NULL);
    }
    sh_handleBytes(sh,
        &gsu->sreg, &gsu->dreg, &gsu->pbr, &gsu->rombr,
        &gsu->rambr, &gsu->scbr, &gsu->scmr, &gsu->colr,
        &gsu->por, &gsu->bramr, &gsu->vcr, &gsu->cfgr,
        &gsu->clsr, &gsu->romdr, &gsu->ramdr, &gsu->pipeline,
        NULL
    );
    sh_handleWords(sh, &gsu->cbr, &gsu->ramar, NULL);
    sh_handleByteArray(sh, gsu->cache, 512);
    sh_handleBools(sh,
        &gsu->sreg_set, &gsu->dreg_set, &gsu->with_flag,
        &gsu->flag_z, &gsu->flag_cy, &gsu->flag_s, &gsu->flag_ov,
        &gsu->flag_go, &gsu->flag_rom, &gsu->flag_alt1, &gsu->flag_alt2,
        &gsu->flag_il, &gsu->flag_ih, &gsu->flag_b, &gsu->flag_irq,
        &gsu->pipeline_valid,
        NULL
    );
}
