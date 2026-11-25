// Locked-cache DMA semantics adapted from MIT-licensed wiikit/runtime/core.cpp.
#include "star_cache.h"
#include "guest_abi/memory.h"
#include <cstring>
#include <stdexcept>
extern void ppc_mmio_write(uint32_t,uint32_t,int);
namespace { uint32_t dmaUpper=0,dmaLower=0; }
uint32_t StarCacheReadSpr(uint32_t spr){return spr==922?dmaUpper:dmaLower;}
void StarCacheWriteSpr(uint32_t spr,uint32_t value){
 if(spr==922){dmaUpper=value;return;}
 dmaLower=value;
 if(!(value&2))return;
 uint32_t lines=(dmaUpper&31)<<2|((value>>2)&3);if(!lines)lines=128;
 uint32_t mem=dmaUpper&0xFFFFFFE0u,lc=value&0xFFFFFFE0u,n=lines*32;
 if(lc<0xE0000000u || uint64_t(lc)+n>0xE0004000ull)throw std::runtime_error("locked-cache DMA range overflow");
 auto* cache=Memory::GetPointer(lc,n);
 uint32_t ram=(mem&0x1FFFFFFFu)|0x80000000u;
 if(value&16)std::memcpy(cache,Memory::GetPointer(ram,n),n);
 else if((mem&0x0FFFF000u)==0x0C008000u){
  for(uint32_t i=0;i<n;i+=4){auto* p=cache+i;uint32_t word=uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];ppc_mmio_write(0xCC008000u,word,4);}
 }else std::memcpy(Memory::GetPointer(ram,n),cache,n);
 dmaLower&=~2u;
}
