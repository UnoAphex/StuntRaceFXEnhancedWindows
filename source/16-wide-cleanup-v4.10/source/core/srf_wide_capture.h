/* USA Rev 1 presentation-only signed polygon observation. */
static uint16_t srf_wide_polygons[2048][68];
static uint32_t srf_wide_count;
static int srf_wide_active;
static uint16_t srf_wide_pending_color;
static int srf_wide_pending;
typedef struct {
    uint16_t count, color;
    int16_t center_x, center_y;
    float xyz[96];
} SrfWideCameraFace;
static SrfWideCameraFace srf_wide_camera[2048];
static uint32_t srf_wide_camera_count;
static uint16_t srf_wide_dma[64][8];
static uint32_t srf_wide_dma_count;
static uint16_t srf_wide_serial;
// Assemble a whole GSU bitmap generation across SNES frames, then freeze it
// when the game starts transferring that bitmap to VRAM.
static SrfWideCameraFace srf_wide_building[2][2048], srf_wide_ready[2][2048];
static uint32_t srf_wide_building_count[2], srf_wide_ready_count[2];
static int srf_wide_building_slot = -1;
void S9xSRFResetWideCapture(void) {
    srf_wide_active=0;
    srf_wide_building_slot=-1;
    memset(srf_wide_building_count,0,sizeof(srf_wide_building_count));
    memset(srf_wide_ready_count,0,sizeof(srf_wide_ready_count));
    srf_wide_camera_count=srf_wide_count=srf_wide_dma_count=0;
    srf_wide_pending=0;
}

const uint8_t *S9xSRFGetWideReady(unsigned slot, uint32_t *size) {
    *size=slot<2 ? srf_wide_ready_count[slot]*sizeof(SrfWideCameraFace) : 0;
    return slot<2 ? (const uint8_t*)srf_wide_ready[slot] : 0;
}

void S9xSRFWideDMA(unsigned bank, unsigned address, unsigned dest, unsigned bytes, unsigned mode)
{
    uint16_t* out;
    int slot = -1;
    if (!srf_wide_active) return;
    if (bank==0x70 && mode==1 && bytes==0x2200) {
        if (address==0x2c00 && dest==0) slot=0;
        if (address==0xb800 && dest==0x3400) slot=1;
    }
    if (slot>=0) {
        srf_wide_ready_count[slot]=srf_wide_building_count[slot];
        memcpy(srf_wide_ready[slot],srf_wide_building[slot],
            srf_wide_ready_count[slot]*sizeof(SrfWideCameraFace));
    }
    if (srf_wide_dma_count == 64) return;
    out = srf_wide_dma[srf_wide_dma_count++];
    out[0]=bank; out[1]=address; out[2]=dest; out[3]=bytes;
    out[4]=mode; out[5]=SCBR; out[6]=srf_wide_camera_count; out[7]=srf_wide_serial;
}
const uint8_t *S9xSRFGetWideDMA(uint32_t *size) {
    *size=srf_wide_dma_count*sizeof(srf_wide_dma[0]);
    return (const uint8_t*)srf_wide_dma;
}

void S9xSRFStartWideCapture(void)
{
    srf_wide_count = 0;
    srf_wide_active = 1;
    srf_wide_pending = 0;
    srf_wide_camera_count = 0;
    srf_wide_dma_count = 0;
    ++srf_wide_serial;
}

const uint8_t *S9xSRFGetWideCamera(uint32_t *size)
{
    *size = srf_wide_camera_count * sizeof(SrfWideCameraFace);
    return (const uint8_t *)srf_wide_camera;
}

const uint8_t *S9xSRFGetWideCapture(uint32_t *size)
{
    srf_wide_active = 0;
    *size = srf_wide_count * sizeof(srf_wide_polygons[0]);
    return (const uint8_t *)srf_wide_polygons;
}

static uint16_t srf_wide_word(unsigned address)
{
    return GSU.pvRamBank[address] | (GSU.pvRamBank[address + 1] << 8);
}

static void srf_wide_append(unsigned count, unsigned pointer, unsigned color)
{
    unsigned i;
    uint16_t *out;
    if (count < 3 || count > 32 || pointer + count * 4 > 65536 || srf_wide_count == 2048)
        return;
    out = srf_wide_polygons[srf_wide_count++];
    memset(out, 0, sizeof(srf_wide_polygons[0]));
    out[0] = count;
    out[1] = color;
    out[2] = pointer;
    out[3] = GSU.vRamBankReg;
    for (i = 0; i < count * 2; ++i) out[4 + i] = srf_wide_word(pointer + i * 2);
}

static void srf_observe_wide(void)
{
    unsigned key = ((GSU.vPrgBankReg & 255u) << 16) | (R15 & 65535u);
    if (!GSU.pvRamBank) return;
    if (key == 0x01918e && PIPE == 0xb5) {
        unsigned pointer = R7 & 65535u;
        unsigned count = srf_wide_word(0x0146);
        srf_wide_pending = 0;
        if ((R3 & 0x8000) || pointer < 2 || count < 3 || count > 32) return;
        srf_wide_pending_color = srf_wide_word(pointer - 2) >> 8;
        if (GSU.pvRomBank && srf_wide_camera_count < 2048) {
            unsigned i, j;
            SrfWideCameraFace *face = &srf_wide_camera[srf_wide_camera_count++];
            memset(face, 0, sizeof(*face));
            face->count = count;
            face->color = srf_wide_pending_color;
            face->center_x = (int16_t)srf_wide_word(0x0034);
            face->center_y = (int16_t)srf_wide_word(0x0036);
            for (i = 0; i < count; ++i) {
                unsigned index = GSU.pvRomBank[(R14 - count + i) & 65535u];
                for (j = 0; j < 3; ++j)
                    face->xyz[i*3+j] = (int16_t)(srf_wide_word(0x0340+index*6+j*2) +
                        srf_wide_word(0x0026+j*2));
            }
            if ((SCBR==0x0b || SCBR==0x25) && face->center_x==104 && face->center_y==64) {
                int slot=SCBR==0x0b ? 0 : 1;
                if (slot!=srf_wide_building_slot) {
                    srf_wide_building_slot=slot;
                    srf_wide_building_count[slot]=0;
                }
                if(srf_wide_building_count[slot]<2048)
                    srf_wide_building[slot][srf_wide_building_count[slot]++]=*face;
            }
        }
        if (!(R5 & 0x10)) srf_wide_append(count, pointer, srf_wide_pending_color);
        else srf_wide_pending = 1;
    } else if (key == 0x0191e0 && PIPE == 0x3d && srf_wide_pending) {
        srf_wide_append(R0 & 65535u, srf_wide_word(0x005a), srf_wide_pending_color);
        srf_wide_pending = 0;
    }
}
