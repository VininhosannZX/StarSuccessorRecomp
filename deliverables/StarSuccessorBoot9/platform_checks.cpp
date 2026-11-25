#include "boot_devices.h"
#include <cstdio>
#include <stdexcept>
void Check(bool value) { if (!value) throw std::runtime_error("platform check failed"); }
int main() {
    StarBoot::Devices d;
    uint32_t v=0; uint16_t h=0;
    auto mustStop=[](auto f) {try {f();} catch(const std::exception&) {return;} throw std::runtime_error("Unsupported operation accepted");};
    auto select=[&](uint32_t command) {
        Check(d.Write32(0xCD006800,0)); Check(d.Write32(0xCD006800,0x100));
        Check(d.Write32(0xCD006810,command)); Check(d.Write32(0xCD00680C,0x35));
    };
    select(0xB0000000); Check(d.Write32(0xCD006810,0xF2000000)); Check(d.Write32(0xCD00680C,5));
    Check(d.Write32(0xCD00680C,1)); Check(d.Read32(0xCD006810,v) && v==0xFF000000);
    select(0x30000100); Check(d.Write32(0xCD00680C,0x31)); Check(d.Read32(0xCD006810,v) && v==0);
    select(0xB0000100); Check(d.Write32(0xCD006810,0x4F4B0D00)); Check(d.Write32(0xCD00680C,0x35));
    Check(d.TakeConsoleOutput()=="OK\r"); Check(d.TakeConsoleOutput().empty());
    select(0xA1000800); Check(d.Write32(0xCD006810,0x03000000)); Check(d.Write32(0xCD00680C,5));
    select(0x21000800); Check(d.Write32(0xCD00680C,0x31)); Check(d.Read32(0xCD006810,v) && v==0x03030303);
    Check(d.Read16(0xCC00206C,h) && h==1); Check(d.Write16(0xCC002030,0x1234));
    Check(d.Read16(0xCC002030,h) && h==0x1234); Check(d.Write16(0xCC002002,3));
    Check(d.Read16(0xCC002002,h) && h==1); Check(d.Read16(0xCC002030,h) && h==0);
    Check(!d.Read16(0xCC002076,h)); mustStop([&]{d.Read16(0xCC00202C,h);});
    select(0); mustStop([&]{d.Write32(0xCD00680C,1);});
    select(0x20000100); mustStop([&]{d.Write32(0xCD00680C,3);});
    std::array<uint8_t,256> ram{};
    d.SetDmaHandlers([&](uint32_t a){return ram.at(a);},[&](uint32_t a,uint8_t b){ram.at(a)=b;});
    select(0x20000100); Check(d.Write32(0xCD006804,32)); Check(d.Write32(0xCD006808,64)); Check(d.Write32(0xCD00680C,3));
    Check(ram[32]==0 && ram[33]==12 && ram[34]==0xFF && ram[35]==0xF0 && ram[51]==0x0C);
    Check(d.Read32(0xCD00680C,v) && v==2);
    select(0xA0000100); ram[50]=2; Check(d.Write32(0xCD00680C,7)); ram.fill(0);
    select(0x20000100); Check(d.Write32(0xCD00680C,3)); Check(ram[50]==2);
    for(unsigned c=0;c<4;++c) {
        Check(d.Write32(0xCD006438,0x0F0F0F0F)); Check(d.Write32(0xCD006434,0xC0010301 | c<<1));
        Check(d.Read32(0xCD006434,v) && !(v&1) && (v&0xA0000000)==0xA0000000);
        Check(d.Read32(0xCD006438,v) && v==(0x08000000u>>(8*c)));
        Check(d.Write32(0xCD006434,0x80000000)); Check(d.Read32(0xCD006434,v) && !(v&0x80000000));
    }
    Check(d.Write32(0xCD006480,0x12345678)); Check(d.Read32(0xCD006480,v) && v==0x12345678);
    Check(!d.Read32(0xCD006440,v)); Check(d.Read32(0xCD800180,v) && v==0);
    Check(d.Write16(0xCC005012,0xFFFF)); Check(d.Read16(0xCC005012,h) && h==0x7F);
    Check(d.Write16(0xCC00501A,0xFFFF)); Check(d.Read16(0xCC00501A,h) && h==0x7FF);
    Check(d.Write16(0xCC005016,0)); Check(d.Read16(0xCC005016,h) && h==1);
    Check(d.Write16(0xCC00500A,0x0804)); mustStop([&]{d.Write16(0xCC00500A,4);});
    Check(d.Write16(0xCC004020,0x1234)); Check(d.Read16(0xCC004020,h) && h==0x1234);
    Check(d.Write32(0xCC003024,0x7)); Check(d.Read32(0xCC003024,v) && v==7);
    Check(d.Read32(0xCC00302C,v) && v==0x246500B1u);
    Check(d.Write32(0xCD000004,0x38)); Check(d.Read32(0xCD000004,v) && v==0x38);
    Check(d.Write32(0xCD000000,32)); Check(d.Read32(0xCD000000,v) && v==32);
    ram.fill(0); ram[35]=1;
    mustStop([&]{d.Write32(0xCD000004,1);});
    std::puts("IPL/UART/RTC, SRAM DMA round-trip, VI config/reset, SI no-device/W1C and unsupported-operation checks passed");
}
