#include "../../port/star_launcher.hpp"
#include "star_cache.h"
// Diagnostic harness for the user's translated Star Successor build.
#include "abi_bridge.h"
#include "recomp_mod_loader.h"
#include "RuntimeConfig.h"
#include "star_host.h"
#include "rt.h"
#include "disc.h"
#include "video.h"
#include <thread>
#include <chrono>
#include <cstring>
#ifndef _WIN32
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <pthread.h>
#endif
#include "boot_platform.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace GuestSmallData {
extern const uint32_t kSda1Base = RuntimeConfig::SDA1_BASE;
extern const uint32_t kSda2Base = RuntimeConfig::SDA2_BASE;
}

namespace {
int exitCode = 0;
void DumpRegisters(std::ostream& out, const CpuContext* cpu) {
    if (!cpu) return;
    out << std::hex << std::setfill('0')
        << "[cpu] pc=0x" << std::setw(8) << cpu->pc
        << " lr=0x" << std::setw(8) << cpu->lr
        << " ctr=0x" << std::setw(8) << cpu->ctr
        << " msr=0x" << std::setw(8) << cpu->msr << '\n';
    for (unsigned i = 0; i < 32; ++i) {
        out << "r" << std::dec << i << "=0x" << std::hex
            << std::setw(8) << cpu->gpr[i] << ((i % 4 == 3) ? '\n' : ' ');
    }
    out << std::dec << std::flush;
}
[[noreturn]] void StopAtGraphics(const char* name) {
    std::cerr << "[stop] Graphics interface reached: " << name << '\n';
    DumpRegisters(std::cerr, &GetPersistentCpuContext());
    std::exit(20);
}
}

void SetRuntimeExitCode(int code) { exitCode = code; }
void MarkFatalErrorReported() {}
void ShowRuntimeFatalPopup(std::string_view category, std::string_view details) noexcept {
    std::cerr << "[fatal] " << category << ": " << details << std::endl;
}
void SystemBridge::DumpCpuState(const CpuContext* cpu) { DumpRegisters(std::cerr, cpu); }
void SystemBridge::DumpCpuState(std::ostream& out, const CpuContext* cpu) { DumpRegisters(out, cpu); }
extern "C" void DumpHostStackTraceForRuntimeHelper() {
    DumpRegisters(std::cerr, &GetPersistentCpuContext());
}

namespace RuntimeCrash {
void WriteCrashArtifacts(std::string_view reason, std::string_view details,
                         const uint32_t* target) noexcept {
    std::cerr << "[crash] " << reason << ": " << details;
    if (target) std::cerr << " target=0x" << std::hex << *target << std::dec;
    std::cerr << std::endl;
    DumpRegisters(std::cerr, &GetPersistentCpuContext());
}
[[noreturn]] void FatalMissingGuestTarget(uint32_t target, CpuContext* cpu) noexcept {
    WriteCrashArtifacts("missing_guest_target", "", &target);
    if(std::getenv("STAR_CAPTURE_RAM")) {
      for(auto region : {std::pair<uint32_t,size_t>{0x80000000u,0x1800000u}, {0x90000000u,0x4000000u}}){
       const char* path=region.first==0x80000000u?"combined-build/native/missing-mem1.bin":"combined-build/native/missing-mem2.bin";
       if(FILE* f=std::fopen(path,"wb")){std::fwrite(Memory::GetPointer(region.first,region.second),1,region.second,f);std::fclose(f);}
      }
    }
    DumpRegisters(std::cerr, cpu);
    std::_Exit(21);
}
}

namespace RecompMod {
// This harness has no mod loader. Executable writes are ordinary guest memory writes.
// Runtime-generated guest instructions are not translated by this harness.
std::atomic<bool> g_executableWriteGuardEnabled{false};
std::atomic<uint8_t> g_executableWriteGuardPages[kExecutableWriteGuardPageCount]{};
std::atomic<uint8_t> g_executableWriteGuardCoarsePages[kExecutableWriteGuardCoarsePageCount]{};
std::atomic<uint8_t> g_executableWriteGuardMidPages[kExecutableWriteGuardMidPageCount]{};
uint32_t CurrentTranslatedExecutionAddress() noexcept {
    return g_currentTranslatedExecutionAddress;
}
bool HandleExecutableWrite(uint32_t, size_t, uint64_t) {
    std::cerr << "[stop] Executable-write guard was invoked unexpectedly." << std::endl;
    std::exit(22);
}
void CheckExecutableWrite(uint32_t address, size_t length, uint64_t value) {
    if (ExecutableWriteGuardMayHit(address, length))
        HandleExecutableWrite(address, length, value);
}
}

extern "C" void GX_HLE_FIFO_Write8(uint8_t v){ppc_mmio_write(0xCC008000,v,1);}
extern "C" void GX_HLE_FIFO_Write16(uint16_t v){ppc_mmio_write(0xCC008000,v,2);}
extern "C" void GX_HLE_FIFO_Write32(uint32_t v){ppc_mmio_write(0xCC008000,v,4);}
extern "C" void GX_HLE_FIFO_WriteFloat(float v){uint32_t b;std::memcpy(&b,&v,4);ppc_mmio_write(0xCC008000,b,4);}
extern "C" void GX_HLE_FIFO_WriteBurst(const uint8_t* p,uint32_t n){for(uint32_t i=0;i<n;++i)ppc_mmio_write(0xCC008000,p[i],1);}

// One diagnostic model covers the complete interrupt-mask initialization routine.
#include "boot_devices.h"
#include <map>
#include <tuple>
namespace {
StarBoot::Devices devices;
using AccessKey = std::tuple<uint32_t, unsigned, char>;
std::map<AccessKey, unsigned> accessCounts;
std::string consoleLine;
uint64_t moduleFingerprint = 0;
bool platformLoaded = false;
uint64_t HashModuleImage() {
    const auto* bytes = Memory::GetPointer(0x81800000u, 0x7C0000u);
    uint64_t hash = 14695981039346656037ull;
    for (unsigned n = 0; n < 0x7C0000u; ++n) hash = (hash ^ bytes[n]) * 1099511628211ull;
    return hash;
}
void FlushConsole() {
    for (char c : devices.TakeConsoleOutput()) {
        if (c == '\r' || c == '\n') {
            if (!consoleLine.empty()) std::cout << "[guest] " << consoleLine << '\n';
            consoleLine.clear();
        } else consoleLine += c;
    }
}
void TraceAccess(uint32_t address, unsigned bits, char mode, uint32_t value) {
    auto& count = accessCounts[{address, bits, mode}];
    ++count;
    if (count <= 4 || (count & (count - 1)) == 0) {
        std::fprintf(stderr, "[mmio] %c%u 0x%08X value=0x%08X count=%u lr=0x%08X\n",
                     mode, bits, address, value, count, (TryGetCpuContext() ? TryGetCpuContext()->lr : GetPersistentCpuContext().lr));
    }
}
void DumpAccessSummary() {
    if (platformLoaded) {
        std::cerr << "[layout] static module bytes retained: " << (HashModuleImage() == moduleFingerprint ? "yes" : "changed") << '\n';
    }
    std::cerr << "[dsp] verified INIT boot completions: " << devices.DspInitBoots() << '\n';
    std::cerr << "[mmio-summary] modeled accesses only\n";
    for (const auto& item : accessCounts) {
        std::fprintf(stderr, "  %c%u 0x%08X count=%u\n",
                     std::get<2>(item.first), std::get<1>(item.first),
                     std::get<0>(item.first), item.second);
    }
}
}
extern "C" uint32_t __real__ZN12MemoryInline10Read32SlowEj(uint32_t);
extern "C" void __real__ZN12MemoryInline11Write32SlowEjj(uint32_t, uint32_t);
extern "C" uint16_t __real__ZN12MemoryInline10Read16SlowEj(uint32_t);
extern "C" void __real__ZN12MemoryInline11Write16SlowEjt(uint32_t, uint16_t);
extern "C" uint32_t __wrap__ZN12MemoryInline10Read32SlowEj(uint32_t address) {
    if((address&0xFE000000u)==0xCC000000u){t_ppc=TryGetCpuContext();return ppc_mmio_read(address,4);}
    uint32_t value = 0;
    if (devices.Read32(address, value)) {
        TraceAccess(address, 32, 'R', value);
        return value;
    }
    return __real__ZN12MemoryInline10Read32SlowEj(address);
}
extern "C" void __wrap__ZN12MemoryInline11Write32SlowEjj(uint32_t address, uint32_t value) {
    if((address&0xFE000000u)==0xCC000000u){t_ppc=TryGetCpuContext();ppc_mmio_write(address,value,4);return;}
    if (devices.Write32(address, value)) {
        TraceAccess(address, 32, 'W', value);
        FlushConsole();
        return;
    }
    __real__ZN12MemoryInline11Write32SlowEjj(address, value);
}
extern "C" uint16_t __wrap__ZN12MemoryInline10Read16SlowEj(uint32_t address) {
    if((address&0xFE000000u)==0xCC000000u){t_ppc=TryGetCpuContext();return ppc_mmio_read(address,2);}
    uint16_t value = 0;
    if (devices.Read16(address, value)) {
        TraceAccess(address, 16, 'R', value);
        return value;
    }
    return __real__ZN12MemoryInline10Read16SlowEj(address);
}
extern "C" void __wrap__ZN12MemoryInline11Write16SlowEjt(uint32_t address, uint16_t value) {
    if((address&0xFE000000u)==0xCC000000u){t_ppc=TryGetCpuContext();ppc_mmio_write(address,value,2);return;}
    if (devices.Write16(address, value)) {
        TraceAccess(address, 16, 'W', value);
        return;
    }
    __real__ZN12MemoryInline11Write16SlowEjt(address, value);
}

// Preserve Star Successor arguments: the runtime's renderer-option filter
// refers to a Mario Kart Wii function address.
extern "C" uint32_t __wrap__Z31FilterScnRendererPathMaskOptionj(uint32_t pathMask) {
    return pathMask;
}
extern "C" void InitializeDataSections();

std::atomic<bool> guestReady{false};
static std::string starDiscRoot="game/extracted/DATA",starNandRoot="game/nand";
int GuestMain() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    std::cout << "[boot] Star Successor native port - development build\n";
    std::atexit(DumpAccessSummary);
    try {
        auto config = Memory::Config::WiiDefaults();
        for (auto& region : config.regions)
            if (region.name == "MEM1_KAMEK_OVERLAY") region.sizeBytes = 0x800000u;
            else if (region.name.rfind("MEM2", 0) == 0) region.sizeBytes = 0x4000000u;
        config.regions.push_back(Memory::RegionConfig{"STAR_KERNEL_SCRATCH",0x82000000u,0x400000u});
        Memory::Init(config);
        if (!disc_open(starDiscRoot.c_str())) throw std::runtime_error("extracted disc not found");
        SysconfOptions options;options.aspect=1;options.language=1;
        sysconf_prepare(starNandRoot.c_str(),options);ios_init(starDiscRoot.c_str(),starNandRoot.c_str());
        hw_init();hw_vi_preset(false);gx_init();audio_init(true);wpad_set_nunchuk(true);wpad_install();
        g_mmio_log=std::getenv("STAR_MMIO")!=nullptr;
        StarHost::SetPoll([](CpuContext* c){
            t_ppc=c;g_ppc_pending.store(0);hw_tick();gx_submit_pending();
            if((c->msr&0x8000u)&&hw_external_pending()){
                uint32_t h=Memory::Read32(0x80003010u);if(h)StarHost::Deliver(c,h,4);
                if(hw_external_pending())g_ppc_pending.store(1);
            }
        });
        std::cout << "[boot] Guest memory initialized\n";
        InitializeDataSections();
        StarBoot::SeedBootPlatform();
        if (!Memory::Contains(0x81800000u, 0x7C0000u)) throw std::runtime_error("RSO overlay is not mapped");
        moduleFingerprint = HashModuleImage(); platformLoaded = true;
        std::cout << "[layout] MEM1 ends at 0x81800000; module image 0x81800000..0x81FC0000; FST reserved at 0x93D00000\n";
        std::cout << "[boot] DOL sections initialized\n";
    TranslatedFunctionRegistry::Finalize();
        InitializePersistentCpuContext();
        CpuContext& cpu = GetPersistentCpuContext();
        StarHost::Initialize(&cpu);
        cpu.gpr[1] = 0x81700000u;
        cpu.pc = 0x80006310u;
        const auto* entry = TranslatedFunctionRegistry::FindByAddressPtr(cpu.pc);
        if (!entry) {
            std::cerr << "[stop] Entry point is not registered\n";
            return 23;
        }
        std::cout << "[boot] Calling guest entry 0x80006310\n";
        std::thread([]{
            auto period=hw_vi_field_period();auto next=std::chrono::steady_clock::now()+period;
            for(;;){
                auto now=std::chrono::steady_clock::now();
                if(now>=next){hw_vi_retrace();period=hw_vi_field_period();next+=period;if(next<=now)next=now+period;}
                auto wake=std::min(next,hw_tick());
                std::this_thread::sleep_until(wake);
            }
        }).detach();
        guestReady.store(true);
        InvokeIndirectCpu(cpu.pc, &cpu);
        std::cout << "[boot] Guest entry returned\n";
        return exitCode;
    } catch (const std::exception& error) {
        std::cerr << "[exception] " << error.what() << '\n';
        DumpRegisters(std::cerr, &GetPersistentCpuContext());
        return 24;
    }
}

extern void ppc_wpar_write(uint32_t);
extern "C" void __real_PPC_WriteSpr(uint32_t,uint32_t);
extern "C" uint32_t __real_PPC_ReadSpr(uint32_t);
extern "C" void __wrap_PPC_WriteSpr(uint32_t n,uint32_t v) { if(n==922||n==923)StarCacheWriteSpr(n,v);else if(n==22)StarHost::SetDecrementer(v);else if(n==921) {ppc_wpar_write(v);}else __real_PPC_WriteSpr(n,v); }
extern "C" uint32_t __wrap_PPC_ReadSpr(uint32_t n) { return n==922||n==923?StarCacheReadSpr(n):n==22?StarHost::GetDecrementer():__real_PPC_ReadSpr(n); }

int main(int argc,char** argv){
 int content=StarPrepareContent(argc,argv,starDiscRoot,starNandRoot);if(content>=0)return content;
#ifdef _WIN32
 { if(FILE* fresh=std::fopen("star-successor.log","wb"))std::fclose(fresh);
   std::freopen("star-successor.log","ab",stdout);std::freopen("star-successor.log","ab",stderr);
   std::setvbuf(stdout,nullptr,_IONBF,0);std::setvbuf(stderr,nullptr,_IONBF,0);
   std::fprintf(stdout,"[game] %s\n",starDiscRoot.c_str()); }
#endif
#ifndef _WIN32
 struct sigaction sa{};sa.sa_flags=SA_SIGINFO;
 sa.sa_sigaction=[](int sig,siginfo_t* info,void*){
  std::fprintf(stderr,"[host-signal] signal=%d address=%p guest-lr=%08X\n",sig,info->si_addr,TryGetCpuContext()?TryGetCpuContext()->lr:0);
  DumpRegisters(std::cerr,TryGetCpuContext());
  if(TryGetCpuContext()){auto* c=TryGetCpuContext();std::fprintf(stderr,"[debug] mutex=%08X current=%08X\n",c->gpr[28],Memory::Read32(0x800000E4u));}
  void* frames[40];int n=backtrace(frames,40);backtrace_symbols_fd(frames,n,2);if(sig!=SIGUSR1)std::_Exit(128+sig);
 };sigemptyset(&sa.sa_mask);sigaction(SIGSEGV,&sa,nullptr);sigaction(SIGABRT,&sa,nullptr);sigaction(SIGUSR1,&sa,nullptr);
#endif
 VideoOptions v;v.widescreen=true;v.window_w=1920;v.window_h=1080;v.scale=3;
 if(const char* p=std::getenv("STAR_TEST_SECONDS"))v.quit_after=std::atof(p);
 v.dump_every=0;
 v.shader_cache="shader-cache";v.shader_list="shader-cache/shaders.txt";
 if(const char* p=std::getenv("STAR_FRAME_DIR")){v.dump_dir=p;v.dump_every=60;}
 video_configure(v);
 std::thread guest([]{int code=GuestMain();if(code)std::_Exit(code);});
#ifndef _WIN32
 auto guestId=guest.native_handle();
 if(std::getenv("STAR_WATCH"))std::thread([guestId]{for(;;){std::this_thread::sleep_for(std::chrono::seconds(3));pthread_kill(guestId,SIGUSR1);}}).detach();
#endif
 guest.detach();
 while(!guestReady.load())std::this_thread::sleep_for(std::chrono::milliseconds(1));
 video_run("Sin & Punishment: Star Successor");
}
