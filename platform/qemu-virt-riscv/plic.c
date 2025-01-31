/*
 * Copyright (c) 2018 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include "platform_p.h"

#include <assert.h>
#include <lk/err.h>
#include <lk/debug.h>
#include <lk/reg.h>
#include <lk/trace.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <platform/interrupts.h>
#include <platform/virt.h>

#define LOCAL_TRACE 0

// Driver for PLIC implementation for qemu riscv virt machine
#define PLIC_PRIORITY(irq) (PLIC_BASE_VIRT + 4 * (irq))
#define PLIC_PENDING(irq)  (PLIC_BASE_VIRT + 0x1000 + (4 * ((irq) / 32)))
#define PLIC_ENABLE(irq, hart)      (PLIC_BASE_VIRT + 0x2000 + (0x80 * PLIC_HART_IDX(hart)) + (4 * ((irq) / 32)))
#define PLIC_THRESHOLD(hart)        (PLIC_BASE_VIRT + 0x200000 + (0x1000 * PLIC_HART_IDX(hart)))
#define PLIC_COMPLETE(hart)         (PLIC_BASE_VIRT + 0x200004 + (0x1000 * PLIC_HART_IDX(hart)))
#define PLIC_CLAIM(hart)            PLIC_COMPLETE(hart)

static struct int_handlers {
    int_handler handler;
    void *arg;
} handlers[NUM_IRQS];

void plic_early_init(void) {
    // mask all irqs and set their priority to 1
    // TODO: mask on all the other cpus too
    for (int i = 1; i < NUM_IRQS; i++) {
        *REG32(PLIC_ENABLE(i, riscv_current_hart())) &= ~(1 << (i % 32));
        *REG32(PLIC_PRIORITY(i)) = 1;
    }

    // set global priority threshold to 0
    *REG32(PLIC_THRESHOLD(riscv_current_hart())) = 0;
}

void plic_init(void) {
}

status_t mask_interrupt(unsigned int vector) {
    *REG32(PLIC_ENABLE(vector, riscv_current_hart())) &= ~(1 << (vector % 32));
    return NO_ERROR;
}

status_t unmask_interrupt(unsigned int vector) {
    *REG32(PLIC_ENABLE(vector, riscv_current_hart())) |= (1 << (vector % 32));
    return NO_ERROR;
}

void register_int_handler(unsigned int vector, int_handler handler, void *arg) {
    LTRACEF("vector %u handler %p arg %p\n", vector, handler, arg);

    DEBUG_ASSERT(vector < NUM_IRQS);

    handlers[vector].handler = handler;
    handlers[vector].arg = arg;
}

void register_int_handler_msi(unsigned int vector, int_handler handler, void *arg, bool edge) {
    PANIC_UNIMPLEMENTED;
}

enum handler_return riscv_platform_irq(void) {
    // see what irq triggered it
    uint32_t vector = *REG32(PLIC_CLAIM(riscv_current_hart()));
    LTRACEF("vector %u\n", vector);

    if (unlikely(vector == 0)) {
        // nothing pending
        return INT_NO_RESCHEDULE;
    }

    THREAD_STATS_INC(interrupts);
    KEVLOG_IRQ_ENTER(vector);

    enum handler_return ret = INT_NO_RESCHEDULE;
    if (handlers[vector].handler) {
        ret = handlers[vector].handler(handlers[vector].arg);
    }

    // ack the interrupt
    *REG32(PLIC_COMPLETE(riscv_current_hart())) = vector;

    KEVLOG_IRQ_EXIT(vector);

    return ret;
}

status_t platform_pci_int_to_vector(unsigned int pci_int, unsigned int *vector) {
    // at the moment there's no translation between PCI IRQs and native irqs
    *vector = pci_int;
    return NO_ERROR;
}

status_t platform_allocate_interrupts(size_t count, uint align_log2, bool msi, unsigned int *vector) {
    return ERR_NOT_SUPPORTED;
}

status_t platform_compute_msi_values(unsigned int vector, unsigned int cpu, bool edge,
        uint64_t *msi_address_out, uint16_t *msi_data_out) {
    return ERR_NOT_SUPPORTED;
}
