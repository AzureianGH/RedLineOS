#ifndef DISPLAYSTANDARD_H
#define DISPLAYSTANDARD_H

#include <fb.h>

void displaystandard_init(struct fb *fb);
void displaystandard_putc(char c);

#endif /* DISPLAYSTANDARD_H */