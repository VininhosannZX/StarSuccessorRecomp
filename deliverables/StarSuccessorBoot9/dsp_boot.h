#pragma once
#include "dsp_boot_program.h"
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <stdexcept>
#include <utility>

namespace StarBoot {
// Minimal DSP boot protocol, not an AX mixer or an audio output backend.
// Hardware/protocol reference: Dolphin DSP.cpp and INIT.cpp;
// also wiikit/runtime/hw.cpp (MIT, Samuele Voltan, 2026).
class DspBoot {
public:
    using Read = std::function<uint8_t(uint32_t)>;
    using Write = std::function<void(uint32_t,uint8_t)>;
    void SetMemory(Read read, Write write) { read_=std::move(read); write_=std::move(write); }
    bool Read16(uint32_t addr, uint16_t& value) {
        switch(addr) {
        case 0xCC005000: value=uint16_t(mailToDsp_ >> 16); return true;
        case 0xCC005002: value=uint16_t(mailToDsp_); return true;
        case 0xCC005004: value=fromDsp_.empty()?0:uint16_t(fromDsp_.front() >> 16); return true;
        case 0xCC005006:
            value=fromDsp_.empty()?0:uint16_t(fromDsp_.front());
            if(!fromDsp_.empty()) fromDsp_.pop_front();
            return true;
        case 0xCC00500A: value=control_; return true;
        case 0xCC005012: value=aramInfo_; return true;
        case 0xCC005016: value=1; return true;
        case 0xCC00501A: value=refresh_; return true;
        default:
            if(IsDmaRegister(addr)) { value=dma_[(addr-0xCC005020)/2]; return true; }
            return false;
        }
    }
    bool Write16(uint32_t addr, uint16_t value) {
        switch(addr) {
        case 0xCC005000: mailToDsp_=(uint32_t(value)<<16)|(mailToDsp_&0xFFFF); return true;
        case 0xCC005002:
            // No task interpreter/mixer is installed. Do not report task completion.
            throw std::runtime_error("DSP mailbox task backend required; boot protocol alone cannot run AX");
        case 0xCC00500A: WriteControl(value); return true;
        case 0xCC005012: aramInfo_=value&0x7F; return true;
        case 0xCC005016: return true;
        case 0xCC00501A: refresh_=value&0x7FF; return true;
        default:
            if(IsDmaRegister(addr)) {
                unsigned i=(addr-0xCC005020)/2;
                dma_[i]=value & ((i==4)?0x83FF:(i%2==0?0x3FF:0xFFE0));
                if(i==5) TransferAram();
                return true;
            }
            return false;
        }
    }
    bool Read32(uint32_t addr,uint32_t& value) {
        if(addr&3u) return false;
        if(!IsDmaRegister(addr) && addr!=0xCC005000 && addr!=0xCC005004) return false;
        uint16_t hi=0,lo=0;
        if(!Read16(addr,hi)||!Read16(addr+2,lo)) return false;
        value=uint32_t(hi)<<16|lo; return true;
    }
    bool Write32(uint32_t addr,uint32_t value) {
        if(addr&3u) return false;
        if(!IsDmaRegister(addr) && addr!=0xCC005000) return false;
        return Write16(addr,uint16_t(value>>16)) && Write16(addr+2,uint16_t(value));
    }
    bool InterruptPending() const {
        return ((control_&8)&&(control_&0x10)) || ((control_&0x20)&&(control_&0x40)) ||
               ((control_&0x80)&&(control_&0x100));
    }
    unsigned InitBoots() const { return initBoots_; }
private:
    static bool IsDmaRegister(uint32_t a) { return a>=0xCC005020 && a<=0xCC00502A && !(a&1); }
    void TransferAram() {
        uint32_t count=((uint32_t(dma_[4]&0x7FFF)<<16)|dma_[5])&~31u;
        uint32_t mainAddr=(uint32_t(dma_[0])<<16)|dma_[1];
        uint32_t aramAddr=(uint32_t(dma_[2])<<16)|dma_[3];
        bool toMain=(dma_[4]&0x8000)!=0;
        if(!read_||!write_||count>0x100000 || uint64_t(mainAddr)+count>0x1800000 ||
           uint64_t(aramAddr)+count>0x4000000)
            throw std::runtime_error("DSP ARAM DMA range/handlers invalid");
        for(uint32_t i=0;i<count;++i) {
            uint32_t a=0x80000000u+mainAddr+i, b=0x90000000u+aramAddr+i;
            uint8_t v=read_(toMain?b:a); write_(toMain?a:b,v);
        }
        if(!toMain && aramAddr==0 && count>=32) initTransferred_=true;
        dma_[4]&=0x8000; dma_[5]=0;
        control_|=0x20; // ARINT, cleared by writing one to DSPCR.
    }
    void WriteControl(uint16_t value) {
        uint16_t old=control_;
        control_=(value&0x0954u)|(old&0xA8u&~value);
        if(value&1) { isInit_=false; fromDsp_.clear(); mailToDsp_=0; }
        if((old&0x800) && !(value&0x800)) {
            if(!initTransferred_) throw std::runtime_error("DSP INIT requested without boot DMA");
            uint32_t hash=2166136261u;
            for(unsigned i=0;i<32;++i) hash=(hash^read_(0x90000000u+i))*16777619u;
            if(hash!=kInitProgramHash) throw std::runtime_error("Unrecognized DSP INIT program");
            isInit_=true;
        }
        if((old&4) && !(control_&4)) {
            fromDsp_.push_back(isInit_?0x80544348u:0x8071FEEDu);
            if(isInit_) ++initBoots_;
        }
    }
    Read read_; Write write_;
    uint16_t control_=4, aramInfo_=0, refresh_=156;
    uint32_t mailToDsp_=0;
    std::array<uint16_t,6> dma_{};
    std::deque<uint32_t> fromDsp_;
    bool initTransferred_=false, isInit_=false;
    unsigned initBoots_=0;
};
}
