// ROM-free lifecycle test of the production read-only geometry observer.
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
static uint8_t ram[65536], rom[65536];
static struct { uint8_t *pvRamBank,*pvRomBank; unsigned vPrgBankReg,vRamBankReg; }
    GSU{ram,rom,1,0};
static unsigned R0,R3,R5,R7,R14,R15,PIPE,SCBR;
#include "../../core/srf_wide_capture.h"
static void word(unsigned address, unsigned value) {
    ram[address]=value&255; ram[address+1]=value>>8;
}
int main() {
    R7=0x1000; R14=3; R15=0x918e; PIPE=0xb5; SCBR=0x0b;
    word(0x146,3); word(0x34,104); word(0x36,64); word(0xffe,0x3000);
    for(unsigned i=0;i<3;++i) { rom[i]=i; word(0x340+i*6+4,100); }
    S9xSRFStartWideCapture(); srf_observe_wide();
    S9xSRFStartWideCapture(); srf_observe_wide();
    S9xSRFWideDMA(0x70,0x2c00,0,0x2200,1);
    uint32_t size=0;
    assert(S9xSRFGetWideReady(0,&size)!=nullptr && size==2*sizeof(SrfWideCameraFace));
    assert(S9xSRFGetWideReady(2,&size)==nullptr && size==0);
    S9xSRFResetWideCapture();
    S9xSRFGetWideReady(0,&size);
    assert(size==0 && srf_wide_building_slot==-1 && !srf_wide_active);
    S9xSRFStartWideCapture(); srf_observe_wide();
    S9xSRFWideDMA(0x70,0x2c00,0,0x2200,1);
    S9xSRFGetWideReady(0,&size);
    assert(size==sizeof(SrfWideCameraFace));
    puts("Wide capture tests passed: multi-frame assembly, DMA freeze, invalid slot, reset/restart.");
}
