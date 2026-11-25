#include "dsp_boot.h"
#include <cstdio>
#include <stdexcept>
#include <unordered_map>
void Check(bool v) { if(!v) throw std::runtime_error("DSP boot assertion failed"); }
int main() {
    StarBoot::DspBoot d;
    std::unordered_map<uint32_t,uint8_t> m;
    d.SetMemory([&](uint32_t a){return m[a];},[&](uint32_t a,uint8_t v){m[a]=v;});
    for(unsigned i=0;i<32;++i)m[0x81000000u+i]=StarBoot::kInitProgram[i];
    uint16_t h=0;uint32_t v=0;
    Check(d.Write16(0xCC00500A,0x8AC)); Check(d.Write16(0xCC00500A,0x8AD));
    Check(d.Read16(0xCC00500A,h) && h==0x804);
    Check(d.Write32(0xCC005020,0x01000000)); Check(d.Write32(0xCC005024,0)); Check(d.Write32(0xCC005028,32));
    Check(d.Read16(0xCC00500A,h) && (h&0x20));
    Check(d.Write16(0xCC00500A,h)); Check(d.Read16(0xCC00500A,h) && !(h&0x20));
    for(unsigned i=0;i<32;++i)Check(m[0x90000000u+i]==StarBoot::kInitProgram[i]);
    Check(d.Write16(0xCC00500A,4)); Check(d.Write16(0xCC00500A,0));
    Check(d.InitBoots()==1); Check(d.Read16(0xCC005004,h) && h==0x8054);
    Check(d.Read16(0xCC005004,h) && h==0x8054); // High read doesn't consume mail.
    Check(d.Read16(0xCC005006,h) && h==0x4348);
    Check(d.Read16(0xCC005004,h) && h==0);
    Check(d.Write16(0xCC00500A,0x805)); Check(d.Write16(0xCC00500A,0x800));
    Check(d.Read32(0xCC005004,v) && v==0x8071FEED);
    bool rejected=false;try {d.Write16(0xCC005002,0xA001);}catch(const std::runtime_error&){rejected=true;}
    Check(rejected);
    StarBoot::DspBoot bad;bad.SetMemory([&](uint32_t a){return m[a];},[&](uint32_t a,uint8_t v){m[a]=v;});
    Check(bad.Write16(0xCC00500A,0x804));
    m[0x81000000]^=1;
    Check(bad.Write32(0xCC005020,0x01000000)); Check(bad.Write32(0xCC005024,0));Check(bad.Write32(0xCC005028,32));
    rejected=false;try{bad.Write16(0xCC00500A,4);}catch(const std::runtime_error&){rejected=true;}
    Check(rejected);
    std::puts("DSP boot: reset, ARAM DMA, W1C, verified INIT payload, mailbox and unsupported-task rejection passed");
}
