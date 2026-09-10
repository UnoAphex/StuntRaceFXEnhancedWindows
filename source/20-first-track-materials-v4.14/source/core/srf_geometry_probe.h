/* Read-only, bounded GSU instruction observation. No emulated state writes.
 * Binary format is documented in COMPATIBILITY_GEOMETRY_PROBE.md. */
#define SRF_PROBE_SLOTS 16384u
#define SRF_PROBE_SNAPSHOTS 256u
static uint32_t srf_probe_histogram[SRF_PROBE_SLOTS][4];
static uint16_t srf_probe_snapshots[SRF_PROBE_SNAPSHOTS][20];
static uint32_t srf_probe_snapshot_count;
static uint32_t srf_probe_watch = 0xffffffffu;
static int srf_probe_active;
static uint32_t srf_probe_status[6]; /* version, missed histogram, missed watches, observed, missed polygons, invalid polygons */
static uint16_t srf_probe_polygons[2048][68]; /* count, color, pointer, RAM bank, 32 word XY pairs */
static uint32_t srf_probe_polygon_count;

void S9xSRFStartGeometryProbe(uint32_t watch)
{
    memset(srf_probe_histogram, 0, sizeof(srf_probe_histogram));
    srf_probe_snapshot_count = 0;
    srf_probe_watch = watch;
    memset(srf_probe_status, 0, sizeof(srf_probe_status));
    srf_probe_status[0] = 2;
    srf_probe_polygon_count = 0;
    srf_probe_active = watch == 0xfffffffeu ? 2 : 1;
}

const uint8_t *S9xSRFGetGeometryProbe(uint32_t *size, int snapshots)
{
    srf_probe_active = 0;
    if (snapshots == 3) {
        *size = srf_probe_polygon_count * sizeof(srf_probe_polygons[0]);
        return (const uint8_t *)srf_probe_polygons;
    }
    if (snapshots == 2) {
        *size = sizeof(srf_probe_status);
        return (const uint8_t *)srf_probe_status;
    }
    if (snapshots) {
        *size = srf_probe_snapshot_count * sizeof(srf_probe_snapshots[0]);
        return (const uint8_t *)srf_probe_snapshots;
    }
    *size = sizeof(srf_probe_histogram);
    return (const uint8_t *)srf_probe_histogram;
}

static void srf_observe_geometry(void)
{
    /* R15 points at the next pipe byte, not necessarily the current opcode.
     * Preserve that convention explicitly; do not call this a verified PC. */
    uint32_t key = ((GSU.vPrgBankReg & 255u) << 16) | (R15 & 65535u);
    uint32_t slot = (key * 2654435761u) & (SRF_PROBE_SLOTS - 1);
    uint32_t attempt;
    /* USA Rev 1 draw-list consumer, after count/color decode and before scan
     * conversion. Guard the pipe opcode and register relationship as well as
     * address; this is not a generic GSU hook for other ROM revisions. */
    if (key == 0x01a971 && PIPE == 0xb1) {
        unsigned count = R12 & 65535u;
        unsigned pointer = R1 & 65535u;
        if (count < 3 || count > 32 || (R3 & 65535u) != count * 4 ||
            pointer + count * 4 > 65536u || !GSU.pvRamBank) {
            ++srf_probe_status[5];
        } else if (srf_probe_polygon_count == 2048) {
            ++srf_probe_status[4];
        } else {
            uint16_t *out = srf_probe_polygons[srf_probe_polygon_count++];
            unsigned i;
            memset(out, 0, sizeof(srf_probe_polygons[0]));
            out[0] = (uint16_t)count;
            out[1] = (uint16_t)GSU.vColorReg;
            out[2] = (uint16_t)pointer;
            out[3] = (uint16_t)GSU.vRamBankReg;
            for (i = 0; i < count * 2; ++i) {
                unsigned address = pointer + i * 2;
                out[4 + i] = GSU.pvRamBank[address] | (GSU.pvRamBank[address + 1] << 8);
            }
        }
    }
    if (srf_probe_active == 2) return; /* Live outlines: no instruction histogram. */
    ++srf_probe_status[3];
    for (attempt = 0; attempt < 32; ++attempt) {
        uint32_t *entry = srf_probe_histogram[slot];
        if (!entry[1] || entry[0] == key) {
            entry[0] = key;
            if (entry[1] != 0xffffffffu) ++entry[1];
            if (PIPE == 0x4c) {
                if (GSU.vStatusReg & 0x100) ++entry[3];
                else ++entry[2];
            }
            break;
        }
        slot = (slot + 1) & (SRF_PROBE_SLOTS - 1);
    }
    if (attempt == 32) ++srf_probe_status[1];
    if (key == srf_probe_watch && srf_probe_snapshot_count == SRF_PROBE_SNAPSHOTS)
        ++srf_probe_status[2];
    if (key == srf_probe_watch && srf_probe_snapshot_count < SRF_PROBE_SNAPSHOTS) {
        uint16_t *out = srf_probe_snapshots[srf_probe_snapshot_count++];
        unsigned i;
        out[0] = (uint16_t)GSU.vPrgBankReg;
        out[1] = (uint16_t)PIPE;
        out[2] = (uint16_t)GSU.vStatusReg;
        out[3] = (uint16_t)GSU.vCacheBaseReg;
        for (i = 0; i < 16; ++i) out[4 + i] = (uint16_t)GSU.avReg[i];
    }
}
