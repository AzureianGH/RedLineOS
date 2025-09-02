#pragma once

#include <isr.h>

// Print diagnostics (registers, vector, error) and halt CPU.
void kernel_panic(const char* reason, const isr_frame_t* frame);
