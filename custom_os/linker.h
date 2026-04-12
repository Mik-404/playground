#pragma once

// Those constants are defined by linker script.

extern uint8_t _phys_start_early;
extern uint8_t _phys_end_early;

extern uint8_t _phys_start_hh;
extern uint8_t _phys_end_hh;

extern uint8_t _phys_start_cpu_startup;
extern uint8_t _phys_end_cpu_startup;

extern uint8_t _phys_start_per_cpu;
extern uint8_t _phys_end_per_cpu;

extern uint8_t _rip_fixups_start[];
extern uint8_t _rip_fixups_end;

extern uint8_t _ctors_start;
extern uint8_t _ctors_end;
