#pragma once

#include <cstdint>
#include <cstddef>

namespace lapic {

// Init initializes Local APIC.
void Init() noexcept;

bool IsInitialized() noexcept;

// StartTimer initializes APIC timer. Should be called after lapic_calibrate_timer.
void StartTimer() noexcept;

// CalibrateTimer calibrates APIC timer.
void CalibrateTimer() noexcept;

// StartCpu wake AP CPU.
void StartCpu(uint32_t lapic_id) noexcept;

// Eoi signals end-of-interrupt to the LAPIC. Must be called before interrupt handler finishes.
void Eoi() noexcept;

void SendIpi(uint32_t lapic_id, int ipi) noexcept;
void BroadcastIpi(int ipi) noexcept;

uint32_t CpuId() noexcept;

}
