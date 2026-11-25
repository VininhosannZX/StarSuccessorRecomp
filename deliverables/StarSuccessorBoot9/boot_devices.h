#pragma once
#include "dsp_boot.h"
#include <array>
#include <cstdio>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <functional>
#include <chrono>
#include <utility>

namespace StarBoot {
class Devices {
public:
    Devices() {
        vi_[0] = 6; vi_[1] = 1; vi_[0x6C / 2] = 1;
        sram_[0x17] = 0x0C;
        sram_[5] = 12; sram_[6] = 0xFF; sram_[7] = 0xF0;
    }
    void SetDmaHandlers(std::function<uint8_t(uint32_t)> read,
                        std::function<void(uint32_t,uint8_t)> write) {
        dsp_.SetMemory(read,write);
        dmaRead_ = std::move(read); dmaWrite_ = std::move(write);
    }
    void SetIpcHandlers(std::function<uint32_t(uint32_t)> read,
                        std::function<void(uint32_t,uint32_t)> write,
                        std::function<bool()> pending) {
        ipcRead_ = std::move(read); ipcWrite_ = std::move(write); ipcPending_ = std::move(pending);
    }
    unsigned DspInitBoots() const { return dsp_.InitBoots(); }
    std::string TakeConsoleOutput() { return std::exchange(consoleOutput_, {}); }
    bool Read16(uint32_t address, uint16_t& value) {
        if (dsp_.Read16(address,value)) return true;
        if (IsMiConfig(address)) { value=miConfig_[(address-0xCC004000u)/2]; return true; }
        if (IsViRegister(address)) {
            auto off = address - 0xCC002000u;
            if (off == 0x2C || off == 0x2E) throw std::runtime_error("VI scan timing backend required");
            value = vi_[off / 2]; return true;
        }
        switch (address) {
        case 0xCC00401Cu: value = miMask_; return true;
        case 0xCC00401Eu: value = miCause_; return true;
        default: return false;
        }
    }
    bool Write16(uint32_t address, uint16_t value) {
        if (dsp_.Write16(address,value)) return true;
        if (IsMiConfig(address)) { miConfig_[(address-0xCC004000u)/2]=value; return true; }
        if (IsViRegister(address)) {
            auto off = address - 0xCC002000u;
            if (off == 0x2C || off == 0x2E || off == 0x6E) throw std::runtime_error("VI write to read-only status");
            if (off == 2 && (value & 2u)) {
                for (unsigned i = 0x30 / 2; i < 0x40 / 2; ++i) vi_[i] = 0;
                value &= ~2u;
            }
            vi_[off / 2] = value; return true;
        }
        switch (address) {
        case 0xCC00401Cu: miMask_ = value & 0x1Fu; return true;
        case 0xCC00401Eu: miCause_ &= ~(value & 0x1Fu); return true;
        default: return false;
        }
    }
    bool Read32(uint32_t address, uint32_t& value) {
        if (dsp_.Read32(address,value)) return true;
        if (ipcRead_ && IsIpcRegister(address)) { value=ipcRead_(address); return true; }
        if (IsSiRegister(address)) { value = si_[(address - 0xCD006400u) / 4]; return true; }
        switch (address) {
        case 0xCD800180u: case 0xCD8001CCu: case 0xCD8001D0u: value = 0; return true; // Dolphin's unknown Hollywood probe register.
        case 0xCD000000u: value = ipcMessage_; return true;
        case 0xCD000004u: value = ipcControl_; return true;
        case 0xCD000008u: value = 0; return true;
        case 0xCC003024u: value = piResetCode_; return true;
        case 0xCC00302Cu: value = 0x246500B1u; return true;
        case 0xCC003000u: value = piCause_ | (dsp_.InterruptPending() ? 0x40u : 0u) | (ipcPending_ && ipcPending_() ? 0x4000u : 0u); return true;
        case 0xCC003004u: value = piMask_; return true;
        case 0xCD000030u: value = hollywoodCause_; return true;
        case 0xCD000034u: value = hollywoodMask_; return true;
        case 0xCD006024u: value = 1; return true; // DI config: bootrom descrambler disabled.
        case 0xCD006C00u: value = aiControl_; return true;
        case 0xCD006C04u: value = aiVolume_; return true;
        case 0xCD006C08u: value = aiSamples_; return true;
        case 0xCD006C0Cu: value = aiTiming_; return true;
        default:
            if (address == 0xCD006800u || address == 0xCD006814u || address == 0xCD006828u) {
                value = exi_[(address - 0xCD006800u) / 0x14u];
                return true;
            }
            for (unsigned channel = 0; channel < 3; ++channel) {
                const uint32_t base = 0xCD006800u + channel * 0x14u;
                if (address >= base + 4 && address <= base + 16 && (address & 3u) == 0) {
                    value = exiTransfers_[channel][(address - base - 4) / 4];
                    return true;
                }
            }
            return false;
        }
    }
    bool Write32(uint32_t address, uint32_t value) {
        if (dsp_.Write32(address,value)) return true;
        if (ipcWrite_ && IsIpcRegister(address)) { ipcWrite_(address,value); return true; }
        if (IsSiRegister(address)) {
            const auto off = address - 0xCD006400u;
            auto& reg = si_[off / 4];
            if (off == 0x34) {
                reg = (reg & 0xB0000001u & ~(value & 0x90000000u)) | (value & 0x487F7F06u);
                if (value & 1u) {
                    reg = (reg & ~1u) | 0xA0000000u;
                    si_[0x38 / 4] |= 0x08000000u >> (8 * ((value >> 1) & 3u));
                }
            } else if (off == 0x38) {
                reg &= ~(value & 0x0F0F0F0Fu);
                if (value & 0x80000000u) reg |= 0x08080808u;
            } else reg = value;
            return true;
        }
        switch (address) {
        case 0xCD800180u: case 0xCD8001CCu: case 0xCD8001D0u: return true; // Probe register: writes are ignored.
        case 0xCD000000u: ipcMessage_ = value; return true;
        case 0xCD000004u:
            if (value & 1u) {
                uint32_t command = 0;
                if (dmaRead_) for (unsigned i=0;i<4;++i) command=(command<<8)|dmaRead_(ipcMessage_+i);
                std::string path;
                if (command == 1 && dmaRead_) {
                    uint32_t pointer = 0;
                    for (unsigned i=0;i<4;++i) pointer=(pointer<<8)|dmaRead_(ipcMessage_+12+i);
                    for (unsigned i=0;i<96;++i) { char c=char(dmaRead_(pointer+i)); if(!c) break; path += c; }
                }
                throw std::runtime_error("IOS IPC service backend required: request=" + Hex(ipcMessage_) + "; command=" + Hex(command) + "; path=" + path);
            }
            ipcControl_ = value & 0x38u; return true;
        case 0xCC003024u: piResetCode_ = value; return true;
        case 0xCC003000u: piCause_ &= ~value; return true;
        case 0xCC003004u: piMask_ = value; return true;
        case 0xCD000030u: hollywoodCause_ &= ~value; return true;
        case 0xCD000034u: hollywoodMask_ = value; return true;
        case 0xCD006C00u:
            if (value & 1u)
                throw std::runtime_error("AI playback requested: audio clock/backend required");
            aiControl_ = (value & 0x56u) | ((aiControl_ & 8u) & ~value);
            if (value & 0x20u) aiSamples_ = 0;
            return true;
        case 0xCD006C04u: aiVolume_ = value & 0xFFFFu; return true;
        case 0xCD006C0Cu: aiTiming_ = value; return true;
        default:
            if (address == 0xCD006800u || address == 0xCD006814u || address == 0xCD006828u) {
                const unsigned channel = (address - 0xCD006800u) / 0x14u;
                auto& csr = exi_[channel];
                if (channel == 0 && (value & 0x380u) != (csr & 0x380u) &&
                    (value & 0x380u) != 0) {
                    ad16Position_ = 0; iplCommandBytes_ = 0; iplCursor_ = 0; iplCommand_ = 0;
                }
                // IRQ flags are write-one-to-clear. No connected device or pending transfer.
                csr = (value & 0x27F5u) | ((csr & 0x80Au) & ~value);
                return true;
            }
            for (unsigned channel = 0; channel < 3; ++channel) {
                const uint32_t base = 0xCD006800u + channel * 0x14u;
                if (address >= base + 4 && address <= base + 16 && (address & 3u) == 0) {
                    if (address == base + 12 && (value & 1u)) {
                        const auto selection = exi_[channel] & 0x380u;
                        if (channel != 0 || (selection != 0x200u && selection != 0x100u))
                            throw std::runtime_error("EXI device backend required");
                        if (value & 2u) {
                            if (selection != 0x100u) throw std::runtime_error("AD16 DMA unsupported");
                            const unsigned dir = (value >> 2) & 3u;
                            if (dir > 1 || (dir == 0 && !dmaWrite_) || (dir == 1 && !dmaRead_))
                                throw std::runtime_error("Invalid DMA direction or missing memory handlers");
                            const uint32_t addr = exiTransfers_[channel][0] & ~31u;
                            const uint32_t size = exiTransfers_[channel][1] & ~31u;
                            if (size > 0x100000u || uint64_t(addr) + size > 0x100000000ull)
                                throw std::runtime_error("EXI DMA range exceeds diagnostic limits");
                            for (uint32_t n = 0; n < size; ++n) {
                                uint8_t byte = TransferIPL(dir ? dmaRead_(addr + n) : 0xFF);
                                if (!dir) dmaWrite_(addr + n, byte);
                            }
                            exiTransfers_[channel][2] = value & ~1u;
                            exi_[channel] |= 8u; return true;
                        }
                        const unsigned mode = (value >> 2) & 3u;
                        if (mode == 3)
                            throw std::runtime_error("EXI reserved transfer mode");
                        const unsigned length = ((value >> 4) & 3u) + 1u;
                        uint32_t result = 0;
                        for (unsigned i = 0; i < length; ++i) {
                            uint8_t byte = mode == 0 ? 0xFF :
                                static_cast<uint8_t>(exiTransfers_[0][3] >> (24 - i * 8));
                            byte = selection == 0x200u ? TransferAD16(byte) : TransferIPL(byte);
                            result |= uint32_t(byte) << (24 - i * 8);
                        }
                        if (mode != 1) exiTransfers_[0][3] = result;
                        exiTransfers_[0][2] = value & ~1u;
                        exi_[0] |= 8u; // transfer-complete status, W1C.
                        return true;
                    }
                    exiTransfers_[channel][(address - base - 4) / 4] = value;
                    return true;
                }
            }
            return false;
        }
    }
private:
    DspBoot dsp_;
    std::function<uint32_t(uint32_t)> ipcRead_;
    std::function<void(uint32_t,uint32_t)> ipcWrite_;
    std::function<bool()> ipcPending_;
    static bool IsIpcRegister(uint32_t a) {
        return (!(a&3u) && a>=0xCD000000u && a<=0xCD00000Cu) || a==0xCD000030u || a==0xCD000034u;
    }
    static bool IsMiConfig(uint32_t a) {
        return !(a&1u) && ((a>=0xCC004000u && a<=0xCC004010u) || (a>=0xCC004020u && a<=0xCC004024u));
    }
    std::array<uint16_t,0x26/2> miConfig_{};
    static std::string Hex(uint32_t value) {
        char text[11]; std::snprintf(text, sizeof(text), "0x%08X", value); return text;
    }
    static bool IsViRegister(uint32_t a) { return a >= 0xCC002000u && a <= 0xCC002074u && !(a & 1u); }
    static bool IsSiRegister(uint32_t a) {
        return !(a & 3u) && ((a >= 0xCD006400u && a <= 0xCD00643Cu) || (a >= 0xCD006480u && a <= 0xCD0064FCu));
    }
    std::array<uint16_t, 0x76 / 2> vi_{};
    std::array<uint32_t, 0x100 / 4> si_{}; // No GameCube pads attached.
    std::array<uint8_t, 0x44> sram_{};
    std::function<uint8_t(uint32_t)> dmaRead_;
    std::function<void(uint32_t,uint8_t)> dmaWrite_;
    uint32_t iplCommand_ = 0;
    unsigned iplCommandBytes_ = 0, iplCursor_ = 0;
    uint8_t rtcFlags_ = 0;
    std::string consoleOutput_;
    void UpdateRtc() {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        uint32_t seconds = uint32_t(std::chrono::duration_cast<std::chrono::seconds>(now).count() - 946684800);
        for (unsigned i = 0; i < 4; ++i) sram_[i] = uint8_t(seconds >> (24 - 8 * i));
    }
    uint8_t TransferIPL(uint8_t byte) {
        if (iplCommandBytes_ < 4) {
            iplCommand_ = (iplCommand_ << 8) | byte;
            if (++iplCommandBytes_ == 4) UpdateRtc();
            return 0xFF;
        }
        const auto address = (iplCommand_ >> 6) & 0x1FFFFFFu;
        const bool write = (iplCommand_ >> 31) != 0;
        const unsigned cursor = iplCursor_++;
        if (address >= 0x800000u && address < 0x800044u) {
            auto& item = sram_[(address - 0x800000u + cursor) % sram_.size()];
            if (write) item = byte;
            return item;
        }
        if (address == 0xC00000u) return byte;
        if (address == 0xC00004u || address == 0x800400u) {
            if (!write) return 0;
            if (byte) consoleOutput_ += char(byte);
            return 0;
        }
        if (address == 0x840020u) { if (write) rtcFlags_ = byte; return rtcFlags_; }
        if (address == 0x80040Cu || address == 0x80044Cu) return byte;
        throw std::runtime_error("IPL operation not implemented; command=" + std::to_string(iplCommand_));
    }
    uint8_t TransferAD16(uint8_t byte) {
        if (ad16Position_ == 0) {
            if (byte != 0 && byte != 0xA0 && byte != 0xA2)
                throw std::runtime_error("Unknown AD16 command");
            ad16Command_ = byte;
        } else if (ad16Command_ == 0) {
            ad16Register_ = 0x04120000u;
            if (ad16Position_ == 1 && byte != 0)
                throw std::runtime_error("Invalid AD16 init padding");
            if (ad16Position_ >= 2 && ad16Position_ <= 5)
                byte = static_cast<uint8_t>(ad16Register_ >> (8 * (5 - ad16Position_)));
        } else if (ad16Position_ <= 4) {
            const unsigned shift = 8 * (4 - ad16Position_);
            if (ad16Command_ == 0xA0)
                ad16Register_ = (ad16Register_ & ~(0xFFu << shift)) | (uint32_t(byte) << shift);
            else
                byte = static_cast<uint8_t>(ad16Register_ >> shift);
        }
        ++ad16Position_;
        return byte;
    }
    unsigned ad16Position_ = 0;
    uint8_t ad16Command_ = 0;
    uint32_t ad16Register_ = 0;
    // Diagnostic cold-start assumptions, with no pending device events.
    uint32_t piCause_ = 0x10000u; // Reset button released.
    uint32_t piResetCode_ = 0;
    uint32_t ipcMessage_ = 0, ipcControl_ = 0;
    uint32_t piMask_ = 0;
    uint32_t hollywoodCause_ = 0;
    uint32_t hollywoodMask_ = 0;
    uint16_t miMask_ = 0;
    uint16_t miCause_ = 0;
    uint32_t aiControl_ = 0; // Audio paused.
    uint32_t aiVolume_ = 0;
    uint32_t aiSamples_ = 0;
    uint32_t aiTiming_ = 0;
    std::array<uint32_t, 3> exi_{};
    std::array<std::array<uint32_t, 4>, 3> exiTransfers_{};
};
}
