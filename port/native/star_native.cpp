#include "abi_bridge.h"
#include "star_host.h"
#include "hle_stubs.h"
#include <thread>
#include <chrono>
#undef REGISTER_NATIVE_FUNCTION
#define REGISTER_NATIVE_FUNCTION(address, fn) \
 static AbiTrampoline<decltype(fn)> MKW_DETAIL_MAKE_UNIQUE(_star_native_raw_, __COUNTER__)(address, #fn, fn, FunctionKind::Native, false, kPpcAllNonvolatileFprMask, 0, 0, &AbiRawCpuThunk<&fn>::Invoke)
// Star Successor native addresses are literal; the MKW region mapper is not applicable.
#undef REGISTER_NATIVE_FUNCTION_AS
#define REGISTER_NATIVE_FUNCTION_AS(address, fn, pretty_name) \
 static AbiTrampoline<decltype(fn)> MKW_DETAIL_MAKE_UNIQUE(_star_native_, __COUNTER__)(address, pretty_name, fn, FunctionKind::Native, false, kPpcAllNonvolatileFprMask, 0, 0, &AbiRawCpuThunk<&fn>::Invoke)
extern "C" void Star_OSDisableInterrupts(CpuContext* ctx) {
 uint32_t before=(ctx->msr>>15)&1u; ctx->msr &= ~0x8000u; ctx->gpr[3]=before;
}
PPC_NATIVE_OVERRIDE_VOID(801ACAF0, Star_OSDisableInterrupts, (CpuContext* ctx), (ctx));
extern "C" void Star_OSEnableInterrupts(CpuContext* ctx) {
 uint32_t before=(ctx->msr>>15)&1u; ctx->msr |= 0x8000u; ctx->gpr[3]=before;
 StarHost::Poll(ctx);
 // SDK SelectThread idle loop has no calls between its run-queue loads.
 // Keep this enable safe point active until an interrupt makes a thread runnable.
 if(ctx->lr==0x801AFCFCu) {
  while(Memory::Read32(ctx->gpr[13]-0x5458u)==0) {
   std::this_thread::sleep_for(std::chrono::microseconds(100));
   StarHost::Poll(ctx);
  }
 }
}
PPC_NATIVE_OVERRIDE_VOID(801ACB10, Star_OSEnableInterrupts, (CpuContext* ctx), (ctx));
extern "C" void Star_OSRestoreInterrupts(CpuContext* ctx) {
 uint32_t before=(ctx->msr>>15)&1u;
 if(ctx->gpr[3])ctx->msr|=0x8000u;else ctx->msr&=~0x8000u;
 ctx->gpr[3]=before; StarHost::Poll(ctx);
}
PPC_NATIVE_OVERRIDE_VOID(801ACB30, Star_OSRestoreInterrupts, (CpuContext* ctx), (ctx));
extern "C" void Star_OSSaveContext(CpuContext* ctx) { StarHost::SaveContext(ctx); }
PPC_NATIVE_OVERRIDE_VOID(801A8800, Star_OSSaveContext, (CpuContext* ctx), (ctx));
extern "C" void Star_OSLoadContext(CpuContext* ctx) { StarHost::LoadContext(ctx); }
PPC_NATIVE_OVERRIDE_VOID(801A8880, Star_OSLoadContext, (CpuContext* ctx), (ctx));
extern "C" void Star_OSGetTime(CpuContext* ctx) {
 StarHost::Poll(ctx); uint64_t t=StarHost::TimeBase();ctx->gpr[3]=uint32_t(t>>32);ctx->gpr[4]=uint32_t(t);
}
PPC_NATIVE_OVERRIDE_VOID(801B0CC0, Star_OSGetTime, (CpuContext* ctx), (ctx));

extern void StarInputCall(const char*,CpuContext*);
extern "C" void Star_WPADInit(CpuContext* ctx){StarInputCall("WPADInit",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C0C10, Star_WPADInit, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADRegisterAllocator(CpuContext* ctx){StarInputCall("WPADRegisterAllocator",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C0CB0, Star_WPADRegisterAllocator, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADGetStatus(CpuContext* ctx){StarInputCall("WPADGetStatus",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C0CC0, Star_WPADGetStatus, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADGetRadioSensitivity(CpuContext* ctx){StarInputCall("WPADGetRadioSensitivity",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C0CD0, Star_WPADGetRadioSensitivity, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADGetSpeakerVolume(CpuContext* ctx){StarInputCall("WPADGetSpeakerVolume",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C0D20, Star_WPADGetSpeakerVolume, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADProbe(CpuContext* ctx){StarInputCall("WPADProbe",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C2020, Star_WPADProbe, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADSetSamplingCallback(CpuContext* ctx){StarInputCall("WPADSetSamplingCallback",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C20B0, Star_WPADSetSamplingCallback, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADSetConnectCallback(CpuContext* ctx){StarInputCall("WPADSetConnectCallback",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C2140, Star_WPADSetConnectCallback, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADSetExtensionCallback(CpuContext* ctx){StarInputCall("WPADSetExtensionCallback",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C21D0, Star_WPADSetExtensionCallback, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADGetDataFormat(CpuContext* ctx){StarInputCall("WPADGetDataFormat",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C2230, Star_WPADGetDataFormat, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADSetDataFormat(CpuContext* ctx){StarInputCall("WPADSetDataFormat",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C2280, Star_WPADSetDataFormat, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADControlMotor(CpuContext* ctx){StarInputCall("WPADControlMotor",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C25C0, Star_WPADControlMotor, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADEnableMotor(CpuContext* ctx){StarInputCall("WPADEnableMotor",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C2690, Star_WPADEnableMotor, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADIsMotorEnabled(CpuContext* ctx){StarInputCall("WPADIsMotorEnabled",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C26D0, Star_WPADIsMotorEnabled, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADSaveConfig(CpuContext* ctx){StarInputCall("WPADSaveConfig",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C2900, Star_WPADSaveConfig, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADRead(CpuContext* ctx){StarInputCall("WPADRead",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C29B0, Star_WPADRead, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADSetAutoSamplingBuf(CpuContext* ctx){StarInputCall("WPADSetAutoSamplingBuf",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C2AA0, Star_WPADSetAutoSamplingBuf, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADGetDpdSensitivity(CpuContext* ctx){StarInputCall("WPADGetDpdSensitivity",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C4970, Star_WPADGetDpdSensitivity, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADIsDpdEnabled(CpuContext* ctx){StarInputCall("WPADIsDpdEnabled",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C49B0, Star_WPADIsDpdEnabled, (CpuContext* ctx), (ctx));
extern "C" void Star_WPADSetCallbackByKPAD(CpuContext* ctx){StarInputCall("WPADSetCallbackByKPAD",ctx);}
PPC_NATIVE_OVERRIDE_VOID(801C6530, Star_WPADSetCallbackByKPAD, (CpuContext* ctx), (ctx));
extern "C" void Star_KPADRead(CpuContext* ctx){StarInputCall("KPADRead",ctx);}
PPC_NATIVE_OVERRIDE_VOID(8019DE10, Star_KPADRead, (CpuContext* ctx), (ctx));
extern "C" void Star_KPADReadEx(CpuContext* ctx){StarInputCall("KPADReadEx",ctx);}
PPC_NATIVE_OVERRIDE_VOID(8019DE20, Star_KPADReadEx, (CpuContext* ctx), (ctx));
extern "C" void Star_KPADInit(CpuContext* ctx){StarInputCall("KPADInit",ctx);}
PPC_NATIVE_OVERRIDE_VOID(8019E550, Star_KPADInit, (CpuContext* ctx), (ctx));
extern "C" void Star_KPADInitEx(CpuContext* ctx){StarInputCall("KPADInitEx",ctx);}
PPC_NATIVE_OVERRIDE_VOID(8019E560, Star_KPADInitEx, (CpuContext* ctx), (ctx));
extern "C" void Star_KPADEnableDPD(CpuContext* ctx){StarInputCall("KPADEnableDPD",ctx);}
PPC_NATIVE_OVERRIDE_VOID(8019EB60, Star_KPADEnableDPD, (CpuContext* ctx), (ctx));
extern "C" void Star_KPADDisableDPD(CpuContext* ctx){StarInputCall("KPADDisableDPD",ctx);}
PPC_NATIVE_OVERRIDE_VOID(8019EB80, Star_KPADDisableDPD, (CpuContext* ctx), (ctx));
