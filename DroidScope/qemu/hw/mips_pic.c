/*
 * MIPS CPU interrupt support.
 *
 */

#include "hw.h"

/* Stub functions for hardware that don't exist.  */
void pic_info(void)
{
}

void irq_info(void)
{
}

static void mips_cpu_irq_handler(void *opaque, int irq, int level)
{
    CPUState *env = (CPUState *)opaque;
    int causebit;

    if (irq < 0 || 7 < irq)
        cpu_abort(env, "mips_pic_cpu_handler: Bad interrupt line %d\n",
                  irq);

    causebit = 0x00000100 << irq;
    if (level) {
        env->CP0_Cause |= causebit;
        cpu_interrupt(env, CPU_INTERRUPT_HARD);
    } else {
        env->CP0_Cause &= ~causebit;
        cpu_reset_interrupt(env, CPU_INTERRUPT_HARD);
    }
}

qemu_irq *mips_cpu_irq_init(CPUState *env)
{
    return qemu_allocate_irqs(mips_cpu_irq_handler, env, 8);
}
