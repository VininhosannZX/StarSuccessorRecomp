#include "star_rso.h"
#include "guest_abi/memory.h"
#include "guest_abi/cpu_context.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
namespace {
struct Module {
 const char* name; const char* path;
 uint32_t base, codeOffset, codeSize, prolog, bssSize, sectionCount, sectionTable;
 uint32_t header=0, code=0; uint8_t* pointer=nullptr;
};
Module modules[]={
#include "rso_modules.inc"
};
bool Valid(Module& m,uint32_t header){
 const uint64_t imageEnd=uint64_t(header)+m.codeOffset+m.codeSize;
 const bool mem1=header>=0x80004000u && imageEnd<=Memory::kMem1CachedBase+Memory::kMem1Size;
 const bool mem2=header>=Memory::kMem2CachedBase && imageEnd<=Memory::kMem2CachedEnd;
 if((!mem1&&!mem2)||!Memory::Contains(header,m.codeOffset+m.codeSize))return false;
 if(Memory::Read32(header+8)!=m.sectionCount || Memory::Read32(header+12)!=header+m.sectionTable || Memory::Read32(header+24)!=1 || Memory::Read32(header+28)!=m.bssSize)return false;
 if(Memory::Read32(header+36)!=header+m.prolog-m.base)return false;
 uint32_t name=Memory::Read32(header+16);size_t len=std::strlen(m.path)+1;
 if(!Memory::Contains(name,len)||std::memcmp(Memory::GetPointer(name,len),m.path,len))return false;
 // Every RSO's prolog section is given in byte 0x20.
 unsigned sec=Memory::Read8(header+32);
 if(sec>=m.sectionCount)return false;
 uint32_t table=header+m.sectionTable+sec*8;
 if((Memory::Read32(table)&~1u)!=header+m.codeOffset || Memory::Read32(table+4)!=m.codeSize)return false;
 return true;
}
void Map(Module& m,uint32_t header){
 m.header=header;m.code=header+m.codeOffset;m.pointer=Memory::GetPointer(m.code,m.codeSize);
 std::fprintf(stderr,"[rso] mapped %s runtime=%08X code=%08X analysis=%08X\n",m.name,header,m.code,m.base+m.codeOffset);
}
}
uint32_t StarRsoVirtualTarget(uint32_t address){
 for(auto& m:modules)if(m.header && address>=m.code && address-m.code<m.codeSize && Valid(m,m.header))return m.base+m.codeOffset+address-m.code;
 for(auto& m:modules){
  uint32_t delta=m.prolog-m.base;if(address<delta)continue;
  uint32_t header=address-delta;
  if(Valid(m,header)){Map(m,header);return m.prolog;}
 }
 return address;
}
uint32_t PPC_ReadRelocatedImmediate(uint32_t virtualPc,uint32_t shifted){
 for(auto& m:modules)if(virtualPc>=m.base+m.codeOffset && virtualPc-(m.base+m.codeOffset)<m.codeSize){
  if(!m.pointer)throw std::runtime_error("RSO code executed before runtime module registration");
  auto* p=m.pointer+virtualPc-(m.base+m.codeOffset)+2;
  uint16_t imm=uint16_t(p[0])<<8|p[1];
  return shifted?uint32_t(imm)<<16:uint32_t(int32_t(int16_t(imm)));
 }
 throw std::runtime_error("relocated instruction is outside RSO code");
}
