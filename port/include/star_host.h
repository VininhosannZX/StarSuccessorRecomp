#pragma once
#include "guest_abi/cpu_context.h"
#include <functional>
#include <cstdint>
namespace StarHost {
void Initialize(CpuContext* cpu);
void SetPoll(std::function<void(CpuContext*)> poll);
void Poll(CpuContext* cpu);
void SaveContext(CpuContext* cpu);
void LoadContext(CpuContext* cpu);
void Deliver(CpuContext* cpu,uint32_t handler,uint32_t interruptNumber);
void SetDecrementer(uint32_t ticks);
uint32_t GetDecrementer();
uint64_t TimeBase();
void Notify();
}
