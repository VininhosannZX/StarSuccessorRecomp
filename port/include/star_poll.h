#pragma once
#include "guest_abi/cpu_context.h"
#include <atomic>
extern std::atomic<uint32_t> g_ppc_pending;
void StarPollInvoke(CpuContext*);
inline bool StarPollPending(const CpuContext* c){return (c->msr&0x8000u)&&g_ppc_pending.load(std::memory_order_relaxed);}
#include "star_rso.h"
