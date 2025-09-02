#pragma once
void pic_remap(void);
void pic_send_eoi(int irq_vector);
void pic_set_mask(int irq);
void pic_clear_mask(int irq);