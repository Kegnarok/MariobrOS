#ifndef ISR_H
#define ISR_H

#include "idt.h"
#include "logging.h"
#include "isr_asm.h"

void isr_install_handler(int srq, void (*handler)(struct regs *r));
void isr_uninstall_handler(int isr);

void isrs_install();

#endif
