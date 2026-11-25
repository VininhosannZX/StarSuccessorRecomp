#include "abi_bridge.h"
#include "star_host.h"
#include "star_rso.h"
#include "timebase_contract.h"
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <utility>
#ifndef _WIN32
#include <pthread.h>
#else
#include <windows.h>
#endif
namespace StarHost {
namespace {
struct HostThread {
 CpuContext owned{}; CpuContext* cpu=&owned; uint32_t context=0; unsigned slot=0;
 std::condition_variable cv;
};
std::mutex mutex;
std::map<uint32_t,HostThread*> threads;
HostThread* running=nullptr;
thread_local HostThread* self=nullptr;
thread_local uint32_t delivering=0;
thread_local unsigned pollDepth=0;
bool stopped=false;
unsigned nextSlot=0;
std::function<void(CpuContext*)> pollDevice;
auto origin=std::chrono::steady_clock::now();
uint64_t decDeadline=UINT64_MAX;
void LoadRegs(CpuContext& c,uint32_t p) {
 for(unsigned i=0;i<32;++i){c.gpr[i]=Memory::Read32(p+i*4);c.fpr[i].raw=Memory::Read64(p+0x90+i*8);}
 c.cr=Memory::Read32(p+0x80);c.lr=Memory::Read32(p+0x84);c.ctr=Memory::Read32(p+0x88);c.xer=Memory::Read32(p+0x8C);
 c.fpscr=Memory::Read32(p+0x194);c.srr0=Memory::Read32(p+0x198);c.srr1=Memory::Read32(p+0x19C);c.msr=c.srr1;
 for(unsigned i=0;i<8;++i)c.gqr[i]=Memory::Read32(p+0x1A4+4*i);
 c.pc=c.srr0;
}
void Await(std::unique_lock<std::mutex>& lock) {
 while(running!=self && !stopped)self->cv.wait(lock);
 if(stopped)throw std::runtime_error("guest scheduler stopped");
}
void Run(HostThread* t) {
 self=t;
 try {
  {std::unique_lock<std::mutex> l(mutex);Await(l);}
  std::fprintf(stderr,"[kernel] starting guest context=0x%08X entry=0x%08X\n",t->context,t->cpu->pc);
  CpuContextScope scope(t->cpu);
  InvokeIndirectCpu(t->cpu->pc,t->cpu);
  if(TranslatedFunctionRegistry::FindByAddressPtr(t->cpu->lr)) InvokeIndirectCpu(t->cpu->lr,t->cpu);
  std::fprintf(stderr,"[kernel] guest host thread returned context=0x%08X\n",t->context);
 } catch(const std::exception& e) {
  std::fprintf(stderr,"[kernel] thread error: %s\n",e.what());
  std::lock_guard<std::mutex> l(mutex);stopped=true;
  for(auto& [k,v]:threads)v->cv.notify_one();
 }
}
#ifndef _WIN32
void* Entry(void* p){Run(static_cast<HostThread*>(p));return nullptr;}
void Spawn(HostThread* t){pthread_attr_t a;pthread_attr_init(&a);pthread_attr_setstacksize(&a,64u<<20);pthread_t id;int r=pthread_create(&id,&a,Entry,t);pthread_attr_destroy(&a);if(r)throw std::runtime_error("host thread creation failed");pthread_detach(id);}
#else
DWORD WINAPI Entry(void* p){Run(static_cast<HostThread*>(p));return 0;}
void Spawn(HostThread* t){HANDLE h=CreateThread(nullptr,64u<<20,Entry,t,STACK_SIZE_PARAM_IS_A_RESERVATION,nullptr);if(!h)throw std::runtime_error("host thread creation failed");CloseHandle(h);}
#endif
}
uint64_t TimeBase(){uint32_t hi,lo,again; do {hi=PPC_Mftbu();lo=PPC_Mftb();again=PPC_Mftbu();}while(hi!=again);return (uint64_t(hi)<<32)|lo;}
void SetDecrementer(uint32_t ticks){decDeadline=TimeBase()+uint64_t(ticks);}
uint32_t GetDecrementer(){return decDeadline==UINT64_MAX?0xFFFFFFFFu:uint32_t(decDeadline-TimeBase());}
void Notify(){if(self)self->cv.notify_one();}
void Initialize(CpuContext* cpu){self=new HostThread;self->cpu=cpu;self->slot=nextSlot++;running=self;origin=std::chrono::steady_clock::now();stopped=false;}
void SetPoll(std::function<void(CpuContext*)> f){pollDevice=std::move(f);}
void Poll(CpuContext* cpu){
 if(pollDepth || !cpu)return;
 ++pollDepth;
 try {
  if(pollDevice)pollDevice(cpu);
  if((cpu->msr&0x8000u) && decDeadline!=UINT64_MAX && TimeBase()>=decDeadline){
   decDeadline=UINT64_MAX;
   uint32_t h=Memory::Read32(0x80003020u);
   if(h)Deliver(cpu,h,8);
  }
 }catch(...){--pollDepth;throw;}
 --pollDepth;
}
void SaveContext(CpuContext* cpu){
 uint32_t p=cpu->gpr[3];
 if(!self)throw std::runtime_error("OSSaveContext outside host thread");
 if(!self->context){std::lock_guard<std::mutex> l(mutex);self->context=p;threads[p]=self;std::fprintf(stderr,"[kernel] adopted main context=0x%08X\n",p);}
 for(unsigned i=13;i<32;++i)Memory::Write32(p+4*i,cpu->gpr[i]);
 for(unsigned i=1;i<8;++i)Memory::Write32(p+0x1A4+4*i,cpu->gqr[i]);
 Memory::Write32(p+0x80,cpu->cr);Memory::Write32(p+0x84,cpu->lr);Memory::Write32(p+0x198,cpu->lr);
 Memory::Write32(p+0x19C,cpu->msr);Memory::Write32(p+0x88,cpu->ctr);Memory::Write32(p+0x8C,cpu->xer);
 Memory::Write32(p+4,cpu->gpr[1]);Memory::Write32(p+8,cpu->gpr[2]);Memory::Write32(p+12,1);
 cpu->gpr[0]=1;cpu->gpr[3]=0;
}
void LoadContext(CpuContext* cpu){
 uint32_t p=cpu->gpr[3];
 if(!self)throw std::runtime_error("OSLoadContext outside host thread");
 Memory::Write16(p+0x1A2,Memory::Read16(p+0x1A2)&~2u);
 if(p==delivering || p==self->context){cpu->gpr[3]=1;return;}
 HostThread* next;
 {std::unique_lock<std::mutex> l(mutex);
  auto it=threads.find(p);
  if(it==threads.end()){
   if(!Memory::Contains(p,0x310))throw std::runtime_error("invalid guest context");
   next=new HostThread;next->slot=nextSlot++;if(next->slot>=64)throw std::runtime_error("too many guest threads");next->context=p;LoadRegs(next->owned,p);
   next->cpu->pc=StarRsoVirtualTarget(next->cpu->pc);
   if(!TranslatedFunctionRegistry::FindByAddressPtr(next->cpu->pc))throw std::runtime_error("new guest thread has unregistered entry");
   threads[p]=next;Spawn(next);
  } else next=it->second;
  if(std::getenv("STAR_SCHED_TRACE"))std::fprintf(stderr,"[kernel] switch 0x%08X -> 0x%08X\n",self->context,p);
  running=next;next->cv.notify_one();Await(l);
 }
 cpu->gpr[3]=1;
}
void Deliver(CpuContext* cpu,uint32_t handler,uint32_t number){
 if(delivering || !(cpu->msr&0x8000u))return;
 if(!TranslatedFunctionRegistry::FindByAddressPtr(handler))throw std::runtime_error("IRQ target not translated");
 const uint32_t context=Memory::Read32(0x800000D4u);
 if(!context||!Memory::Contains(context,0x2C8))throw std::runtime_error("interrupt without a valid OS current context");
 const uint32_t stack=0x82000000u+(self?self->slot:0)*0x10000u+0xF000u;
 for(unsigned i=0;i<32;++i){Memory::Write32(context+4*i,cpu->gpr[i]);Memory::Write64(context+0x90+8*i,cpu->fpr[i].raw);}
 Memory::Write32(context+0x80,cpu->cr);Memory::Write32(context+0x84,cpu->lr);Memory::Write32(context+0x88,cpu->ctr);Memory::Write32(context+0x8C,cpu->xer);
 for(unsigned i=0;i<8;++i)Memory::Write32(context+0x1A4+4*i,cpu->gqr[i]);
 Memory::Write32(context+0x194,cpu->fpscr);
 Memory::Write32(context+0x198,cpu->pc);Memory::Write32(context+0x19C,cpu->msr);Memory::Write16(context+0x1A2,2);
 CpuContext irq=*cpu;irq.gpr[1]=stack;irq.gpr[3]=number;irq.gpr[4]=context;irq.msr&=~0x8000u;
 uint32_t old=delivering;delivering=context;
 try{CpuContextScope scope(&irq);InvokeIndirectCpu(handler,&irq);}catch(...){delivering=old;throw;}
 delivering=old;
}
}
