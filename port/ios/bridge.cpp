#include "rt.h"
#include "star_host.h"
#include "abi_bridge.h"
#include "guest_abi/flat_memory.h"
#include <cstdarg>
#include <stdexcept>
#include <map>
uint8_t* g_mem=MKW_FLAT_GUEST_BASE;
bool g_gamecube=false;
uint32_t g_bus_mhz=243;

void rt_log(const char* fmt,...){va_list args;va_start(args,fmt);std::fputs("[ios] ",stderr);std::vfprintf(stderr,fmt,args);std::fputc('\n',stderr);va_end(args);}
[[noreturn]] void rt_die(const char* fmt,...){char b[1024];va_list args;va_start(args,fmt);std::vsnprintf(b,sizeof(b),fmt,args);va_end(args);throw std::runtime_error(b);}
std::string guest_cstr(uint32_t a,size_t n){std::string s;for(size_t i=0;i<n;++i){char c=char(Memory::Read8(a+uint32_t(i)));if(!c)break;s+=c;}return s;}
std::atomic<uint32_t> g_ppc_pending{0};
thread_local PPCContext* t_ppc=nullptr;
void os_raise(){g_ppc_pending.store(1);StarHost::Notify();}
uint64_t os_tb_now(){return StarHost::TimeBase();}
void ppc_poll(PPCContext& c){StarHost::Poll(&c);}
std::string rt_name(uint32_t a){char b[20];std::snprintf(b,sizeof(b),"0x%08X",a);return b;}
void rt_backtrace(const PPCContext& c,FILE* out,int){std::fprintf(out,"guest lr=%08X sp=%08X\n",c.lr,c.gpr[1]);}
void os_report_running(int){if(t_ppc)rt_backtrace(*t_ppc,stderr,0);}
PPCFunc ppc_lookup(uint32_t a){if(!TranslatedFunctionRegistry::FindByAddressPtr(a))return {};return [a](PPCContext& c){InvokeIndirectCpu(a,&c);};}
void ppc_call_indirect(PPCContext& c,uint32_t a){InvokeIndirectCpu(a,&c);}
std::map<std::string,PPCFunc> inputHooks;
PPCFunc ppc_hook(const char* name,PPCFunc f){inputHooks[name]=std::move(f);return {};}
void StarInputCall(const char* name,CpuContext* c){static std::map<std::string,uint64_t> seen;if(++seen[name]==1)rt_log("input: native %s",name);auto i=inputHooks.find(name);if(i==inputHooks.end())throw std::runtime_error(std::string("input hook missing: ")+name);i->second(*c);}


uint32_t (*g_efb_peek)(uint32_t)=nullptr;

void StarPollInvoke(CpuContext* c){
 CpuContext before=*c;
 StarHost::Poll(c);
 if(std::memcmp(before.gpr,c->gpr,sizeof(c->gpr))||std::memcmp(before.fpr,c->fpr,sizeof(c->fpr))||before.msr!=c->msr)
  rt_die("interrupt bridge modified interrupted architectural registers");
}
