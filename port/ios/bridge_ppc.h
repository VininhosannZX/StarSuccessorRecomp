#pragma once
#include "memory.h"
#include "guest_abi/cpu_context.h"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <atomic>
#include <functional>
struct RsoModule;
using PPCContext=CpuContext;
using PPCFunc=std::function<void(PPCContext&)>;
PPCFunc ppc_hook(const char*,PPCFunc);
extern std::atomic<uint32_t> g_ppc_pending;
void ppc_poll(PPCContext&);
void ppc_call_indirect(PPCContext&,uint32_t);
uint32_t ppc_mmio_read(uint32_t,int);
void ppc_mmio_write(uint32_t,uint32_t,int);
extern uint8_t* g_mem;
inline uint8_t ld8(uint32_t a){return Memory::Read8(a);}
inline uint16_t ld16(uint32_t a){return Memory::Read16(a);}
inline uint32_t ld32(uint32_t a){return Memory::Read32(a);}
inline uint64_t ld64(uint32_t a){return Memory::Read64(a);}
inline void st8(uint32_t a,uint8_t v){Memory::Write8(a,v);}
inline void st16(uint32_t a,uint16_t v){Memory::Write16(a,v);}
inline void st32(uint32_t a,uint32_t v){Memory::Write32(a,v);}
inline void st64(uint32_t a,uint64_t v){Memory::Write64(a,v);}
