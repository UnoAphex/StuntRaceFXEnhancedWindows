#ifndef DSP1_H
#define DSP1_H

#include <stdint.h>
#include <stdbool.h>

#include "statehandler.h"

typedef struct {
    // Command interface
    bool waiting4command;
    bool first_parameter;
    uint8_t command;
    uint32_t in_count, in_index, out_count, out_index;
    uint8_t parameters[512];
    uint8_t output[512];

    // Projection state
    int16_t CentreX, CentreY, VOffset;
    int16_t VPlane_C, VPlane_E;
    int16_t SinAas, CosAas, SinAzs, CosAzs;
    int16_t SinAZS, CosAZS, SecAZS_C1, SecAZS_E1, SecAZS_C2, SecAZS_E2;
    int16_t Nx, Ny, Nz, Gx, Gy, Gz;
    int16_t C_Les, E_Les, G_Les;
    int16_t matrixA[3][3], matrixB[3][3], matrixC[3][3];

    // Per-command I/O fields
    int16_t Op00Multiplicand, Op00Multiplier, Op00Result;
    int16_t Op20Multiplicand, Op20Multiplier, Op20Result;
    int16_t Op10Coefficient, Op10Exponent, Op10CoefficientR, Op10ExponentR;
    int16_t Op04Angle, Op04Radius, Op04Sin, Op04Cos;
    int16_t Op0CA, Op0CX1, Op0CY1, Op0CX2, Op0CY2;

    int16_t Op02FX, Op02FY, Op02FZ, Op02LFE, Op02LES, Op02AAS, Op02AZS;
    int16_t Op02VOF, Op02VVA, Op02CX, Op02CY;

    int16_t Op0AVS, Op0AA, Op0AB, Op0AC, Op0AD;

    int16_t Op06X, Op06Y, Op06Z, Op06H, Op06V, Op06M;

    int16_t Op01m, Op01Zr, Op01Xr, Op01Yr;
    int16_t Op11m, Op11Zr, Op11Xr, Op11Yr;
    int16_t Op21m, Op21Zr, Op21Xr, Op21Yr;

    int16_t Op0DX, Op0DY, Op0DZ, Op0DF, Op0DL, Op0DU;
    int16_t Op1DX, Op1DY, Op1DZ, Op1DF, Op1DL, Op1DU;
    int16_t Op2DX, Op2DY, Op2DZ, Op2DF, Op2DL, Op2DU;

    int16_t Op03F, Op03L, Op03U, Op03X, Op03Y, Op03Z;
    int16_t Op13F, Op13L, Op13U, Op13X, Op13Y, Op13Z;
    int16_t Op23F, Op23L, Op23U, Op23X, Op23Y, Op23Z;

    int16_t Op14Zr, Op14Xr, Op14Yr, Op14U, Op14F, Op14L;
    int16_t Op14Zrr, Op14Xrr, Op14Yrr;

    int16_t Op0EH, Op0EV, Op0EX, Op0EY;

    int16_t Op0BX, Op0BY, Op0BZ, Op0BS;
    int16_t Op1BX, Op1BY, Op1BZ, Op1BS;
    int16_t Op2BX, Op2BY, Op2BZ, Op2BS;

    int16_t Op28X, Op28Y, Op28Z, Op28R;

    int16_t Op1CX, Op1CY, Op1CZ;
    int16_t Op1CXBR, Op1CYBR, Op1CZBR;
    int16_t Op1CXAR, Op1CYAR, Op1CZAR;
    int16_t Op1CX1, Op1CY1, Op1CZ1;
    int16_t Op1CX2, Op1CY2, Op1CZ2;

    uint16_t Op0FRamsize, Op0FPass;

    int16_t Op2FUnknown, Op2FSize;

    int16_t Op08X, Op08Y, Op08Z, Op08Ll, Op08Lh;

    int16_t Op18X, Op18Y, Op18Z, Op18R, Op18D;
    int16_t Op38X, Op38Y, Op38Z, Op38R, Op38D;

    // Memory mapping
    uint16_t boundary;
} Dsp1;

extern Dsp1 dsp1;

void dsp1_init(void);
void dsp1_reset(void);
uint8_t dsp1_read(uint16_t address);
void dsp1_write(uint16_t address, uint8_t value);
void dsp1_handleState(StateHandler *sh);

#endif
