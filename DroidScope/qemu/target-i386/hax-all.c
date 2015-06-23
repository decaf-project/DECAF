/*
** Copyright (c) 2011, Intel Corporation
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/

/*
 * HAX common code for both Windows and Darwin
 * Some portion of code from KVM is used in this file.
 */

#include "target-i386/hax-i386.h"

#define HAX_EMUL_ONE    0x1
#define HAX_EMUL_REAL   0x2
#define HAX_EMUL_HLT    0x4
#define HAX_EMUL_EXITLOOP    0x5

#define HAX_EMULATE_STATE_MMIO  0x1
#define HAX_EMULATE_STATE_REAL  0x2
#define HAX_EMULATE_STATE_NONE  0x3
#define HAX_EMULATE_STATE_INITIAL       0x4

struct hax_state hax_global;

int hax_support = -1;

/* Called after hax_init */
int hax_enabled()
{
    return (!hax_disabled && hax_support);
}

/* Currently non-PG modes are emulated by QEMU */
int hax_vcpu_emulation_mode(CPUState *env)
{
    return !(env->cr[0] & CR0_PG_MASK);
}

static int hax_prepare_emulation(CPUState *env)
{
    /* Flush all emulation states */
    tlb_flush(env, 1);
    tb_flush(env);
    /* Sync the vcpu state from hax kernel module */
    hax_vcpu_sync_state(env, 0);
    return 0;
}

/*
 * Check whether to break the translation block loop
 * Break tbloop after one MMIO emulation, or after finish emulation mode
 */
static int hax_stop_tbloop(CPUState *env)
{
    switch (env->hax_vcpu->emulation_state)
    {
        case HAX_EMULATE_STATE_MMIO:
            return 1;
        case HAX_EMULATE_STATE_INITIAL:
        case HAX_EMULATE_STATE_REAL:
            if (!hax_vcpu_emulation_mode(env))
                return 1;
            break;
        default:
            dprint("Invalid emulation state in hax_sto_tbloop state %x\n",
              env->hax_vcpu->emulation_state);
            break;
    }

    return 0;
}

int hax_stop_emulation(CPUState *env)
{
    if (hax_stop_tbloop(env))
    {
        env->hax_vcpu->emulation_state =  HAX_EMULATE_STATE_NONE;
        /*
         * QEMU emulation changes vcpu state,
         * Sync the vcpu state to HAX kernel module
         */
        hax_vcpu_sync_state(env, 1);
        return 1;
    }

    return 0;
}

int hax_stop_translate(CPUState *env)
{
    struct hax_vcpu_state *vstate;

    vstate = env->hax_vcpu;
    assert(vstate->emulation_state);
    if (vstate->emulation_state == HAX_EMULATE_STATE_MMIO )
        return 1;

    return 0;
}

int valid_hax_tunnel_size(uint16_t size)
{
    return size >= sizeof(struct hax_tunnel);
}

hax_fd hax_vcpu_get_fd(CPUState *env)
{
    struct hax_vcpu_state *vcpu = env->hax_vcpu;
    if (!vcpu)
        return HAX_INVALID_FD;
    return vcpu->fd;
}

/* Current version */
uint32_t hax_cur_version = 0x2;
/* Least HAX kernel version */
uint32_t hax_lest_version = 0x1;

static int hax_get_capability(struct hax_state *hax)
{
    int ret;
    struct hax_capabilityinfo capinfo, *cap = &capinfo;

    ret = hax_capability(hax, cap);
    if (ret)
        return -ENOSYS;

    if ( ((cap->wstatus & HAX_CAP_WORKSTATUS_MASK) ==
          HAX_CAP_STATUS_NOTWORKING ))
    {
        if (cap->winfo & HAX_CAP_FAILREASON_VT)
            dprint("VT feature is not enabled, HAXM not working.\n");
        else if (cap->winfo & HAX_CAP_FAILREASON_NX)
            dprint("NX feature is not enabled, HAXM not working.\n");
        return -ENXIO;
    }

    if (cap->wstatus & HAX_CAP_MEMQUOTA)
    {
        if (cap->mem_quota < hax->mem_quota)
        {
            dprint("The memory needed by this VM exceeds the driver limit.\n");
            return -ENOSPC;
        }
    }

    return 0;
}

static int hax_version_support(struct hax_state *hax)
{
    int ret;
    struct hax_module_version version;

    ret = hax_mod_version(hax, &version);
    if (ret < 0)
        return 0;

    if ( (hax_lest_version > version.cur_version) ||
         (hax_cur_version < version.compat_version) )
        return 0;

    return 1;
}

int hax_vcpu_create(int id)
{
    struct hax_vcpu_state *vcpu = NULL;
    int ret;

    if (!hax_global.vm)
    {
        dprint("vcpu %x created failed, vm is null\n", id);
        return -1;
    }

    if (hax_global.vm->vcpus[id])
    {
        dprint("vcpu %x allocated already\n", id);
        return 0;
    }

    vcpu = qemu_malloc(sizeof(struct hax_vcpu_state));
    if (!vcpu)
    {
        dprint("Failed to alloc vcpu state\n");
        return -ENOMEM;
    }

    memset(vcpu, 0, sizeof(struct hax_vcpu_state));

    ret = hax_host_create_vcpu(hax_global.vm->fd, id);
    if (ret)
    {
        dprint("Failed to create vcpu %x\n", id);
        goto error;
    }

    vcpu->fd = hax_host_open_vcpu(hax_global.vm->id, id);
    if (hax_invalid_fd(vcpu->fd))
    {
        dprint("Failed to open the vcpu\n");
        ret = -ENODEV;
        goto error;
    }

    hax_global.vm->vcpus[id] = vcpu;

    ret = hax_host_setup_vcpu_channel(vcpu);
    if (ret)
    {
        dprint("Invalid HAX tunnel size \n");
        ret = -EINVAL;
        goto error;
    }
    return 0;

error:
    /* vcpu and tunnel will be closed automatically */
    if (vcpu && !hax_invalid_fd(vcpu->fd))
        hax_close_fd(vcpu->fd);

    hax_global.vm->vcpus[id] = NULL;
    qemu_free(vcpu);
    return -1;
}

int hax_vcpu_destroy(CPUState *env)
{
    struct hax_vcpu_state *vcpu = env->hax_vcpu;

    if (!hax_global.vm)
    {
        dprint("vcpu %x destroy failed, vm is null\n", vcpu->vcpu_id);
        return -1;
    }

    if (!vcpu)
        return 0;

    /*
     * 1. The hax_tunnel is also destroyed at vcpu_destroy
     * 2. hax_close_fd will require the HAX kernel module to free vcpu
     */
    hax_close_fd(vcpu->fd);
    hax_global.vm->vcpus[vcpu->vcpu_id] = NULL;
    qemu_free(vcpu);
    return 0;
}

int hax_init_vcpu(CPUState *env)
{
    int ret;

    ret = hax_vcpu_create(env->cpu_index);
    if (ret < 0)
    {
        dprint("Failed to create HAX vcpu\n");
        exit(-1);
    }

    env->hax_vcpu = hax_global.vm->vcpus[env->cpu_index];
    env->hax_vcpu->emulation_state = HAX_EMULATE_STATE_INITIAL;

    return ret;
}

struct hax_vm *hax_vm_create(struct hax_state *hax)
{
    struct hax_vm *vm;
    int vm_id = 0, ret;
    char *vm_name = NULL;

    if (hax_invalid_fd(hax->fd))
        return NULL;

    if (hax->vm)
        return hax->vm;

    vm = qemu_malloc(sizeof(struct hax_vm));
    if (!vm)
        return NULL;
    memset(vm, 0, sizeof(struct hax_vm));
    ret = hax_host_create_vm(hax, &vm_id);
    if (ret) {
        dprint("Failed to create vm %x\n", ret);
        goto error;
    }
    vm->id = vm_id;
    vm->fd = hax_host_open_vm(hax, vm_id);
    if (hax_invalid_fd(vm->fd))
    {
        dprint("Open vm device error:%s\n", vm_name);
        goto error;
    }

    hax->vm = vm;
    return vm;

error:
    qemu_free(vm);
    hax->vm = NULL;
    return NULL;
}

int hax_vm_destroy(struct hax_vm *vm)
{
    int i;

    for (i = 0; i < HAX_MAX_VCPU; i++)
        if (vm->vcpus[i])
        {
            dprint("VCPU should be cleaned before vm clean\n");
            return -1;
        }
    hax_close_fd(vm->fd);
    qemu_free(vm);
    hax_global.vm = NULL;
    return 0;
}

int hax_set_ramsize(uint64_t ramsize)
{
    struct hax_state *hax = &hax_global;

    memset(hax, 0, sizeof(struct hax_state));
    hax->mem_quota = ram_size;

    return 0;
}

int hax_init(int smp_cpus)
{
    struct hax_state *hax = NULL;
    struct hax_qemu_version qversion;
    int ret;

    hax_support = 0;

    hax = &hax_global;

    hax->fd = hax_mod_open();
    if (hax_invalid_fd(hax->fd))
    {
        hax->fd = 0;
        ret = -ENODEV;
        goto error;
    }

    ret = hax_get_capability(hax);
    /* In case HAXM have no such capability support */
    if (ret && (ret != -ENOSYS))
    {
        ret = -EINVAL;
        goto error;
    }

    if (!hax_version_support(hax))
    {
        dprint("Incompatible HAX version. Qemu current version %x ", hax_cur_version );
        dprint("requires least HAX version %x\n", hax_lest_version);
        ret = -EINVAL;
        goto error;
    }

    hax->vm = hax_vm_create(hax);
    if (!hax->vm)
    {
        dprint("Failed to create HAX VM\n");
        ret = -EINVAL;
        goto error;
    }

    qversion.cur_version = hax_cur_version;
    qversion.least_version = hax_lest_version;
    hax_notify_qemu_version(hax->vm->fd, &qversion);
    hax_support = 1;
    qemu_register_reset( hax_reset_vcpu_state, 0, NULL);

    return 0;
error:
    if (hax->vm)
        hax_vm_destroy(hax->vm);
    if (hax->fd)
        hax_mod_close(hax);

    return ret;
}

int  hax_handle_fastmmio(CPUState *env, struct hax_fastmmio *hft)
{
    uint64_t buf = 0;

    /*
     * With fast MMIO, QEMU need not sync vCPU state with HAXM
     * driver because it will only invoke MMIO handler
     * However, some MMIO operations utilize virtual address like qemu_pipe
     * Thus we need to sync the CR0, CR3 and CR4 so that QEMU
     * can translate the guest virtual address to guest physical
     * address
     */
    env->cr[0] = hft->_cr0;
    env->cr[2] = hft->_cr2;
    env->cr[3] = hft->_cr3;
    env->cr[4] = hft->_cr4;

    buf = hft->value;
    cpu_physical_memory_rw(hft->gpa, &buf, hft->size, hft->direction);
    if (hft->direction == 0)
        hft->value = buf;

    return 0;
}

int hax_handle_io(CPUState *env, uint32_t df, uint16_t port, int direction,
  int size, int count, void *buffer)
{
    uint8_t *ptr;
    int i;

    if (!df)
        ptr = (uint8_t *)buffer;
    else
        ptr = buffer + size * count - size;
    for (i = 0; i < count; i++)
    {
        if (direction == HAX_EXIT_IO_IN) {
            switch (size) {
                case 1:
                    stb_p(ptr, cpu_inb(port));
                    break;
                case 2:
                    stw_p(ptr, cpu_inw(port));
                    break;
                case 4:
                    stl_p(ptr, cpu_inl(port));
                    break;
            }
        } else {
            switch (size) {
                case 1:
                    cpu_outb(port, ldub_p(ptr));
                    break;
                case 2:
                    cpu_outw(port, lduw_p(ptr));
                    break;
                case 4:
                    cpu_outl(port, ldl_p(ptr));
                    break;
            }
        }
        if (!df)
            ptr += size;
        else
            ptr -= size;
    }

    return 0;
}

static int hax_vcpu_interrupt(CPUState *env)
{
    struct hax_vcpu_state *vcpu = env->hax_vcpu;
    struct hax_tunnel *ht = vcpu->tunnel;

    /*
     * Try to inject an interrupt if the guest can accept it
     * Unlike KVM, the HAX kernel module checks the eflags, instead.
     */
    if (ht->ready_for_interrupt_injection &&
      (env->interrupt_request & CPU_INTERRUPT_HARD))
    {
        int irq;

        env->interrupt_request &= ~CPU_INTERRUPT_HARD;
        irq = cpu_get_pic_interrupt(env);
        if (irq >= 0) {
            hax_inject_interrupt(env, irq);
        }
    }

    /* 
     * If we have an interrupt pending but the guest is not ready to
     * receive it, request an interrupt window exit.  This will cause
     * a return to userspace as soon as the guest is ready to receive
     * an interrupt.
     */
    if ((env->interrupt_request & CPU_INTERRUPT_HARD))
        ht->request_interrupt_window = 1;
    else
        ht->request_interrupt_window = 0;
    return 0;
}

void hax_raise_event(CPUState *env)
{
    struct hax_vcpu_state *vcpu = env->hax_vcpu;

    if (!vcpu)
        return;
    vcpu->tunnel->user_event_pending = 1;
}

/*
 * Request the HAX kernel module to run the CPU for us until one of
 * the following occurs:
 * 1. Guest crashes or is shut down
 * 2. We need QEMU's emulation like when the guest executes a MMIO
 *    instruction or guest enters emulation mode (non-PG mode)
 * 3. Guest executes HLT
 * 4. Qemu has Signal/event pending
 * 5. An unknown VMX-exit happens
 */
extern void qemu_system_reset_request(void);
static int hax_vcpu_hax_exec(CPUState *env)
{
    int ret = 0;
    struct hax_vcpu_state *vcpu = env->hax_vcpu;
    struct hax_tunnel *ht = vcpu->tunnel;

    if (hax_vcpu_emulation_mode(env))
    {
        dprint("Trying to vcpu execute at eip:%lx\n", env->eip);
        return  HAX_EMUL_EXITLOOP;
    }

    do {
        int hax_ret;

        if (env->exit_request) {
            ret = HAX_EMUL_EXITLOOP ;
            break;
        }

        hax_vcpu_interrupt(env);

        hax_ret = hax_vcpu_run(vcpu);

        /* Simply continue the vcpu_run if system call interrupted */
        if (hax_ret == -EINTR || hax_ret == -EAGAIN) {
            dprint("io window interrupted\n");
            continue;
        }

        if (hax_ret < 0)
        {
            dprint("vcpu run failed for vcpu  %x\n", vcpu->vcpu_id);
            abort();
        }
        switch (ht->_exit_status)
        {
            case HAX_EXIT_IO:
                {
                    ret = hax_handle_io(env, ht->pio._df, ht->pio._port,
                      ht->pio._direction,
                      ht->pio._size, ht->pio._count, vcpu->iobuf);
                }
                break;
            case HAX_EXIT_MMIO:
                ret = HAX_EMUL_ONE;
                break;
            case HAX_EXIT_FAST_MMIO:
                ret = hax_handle_fastmmio(env,
                        (struct hax_fastmmio *)vcpu->iobuf);
                break;
            case HAX_EXIT_REAL:
                ret = HAX_EMUL_REAL;
                break;
                /* Guest state changed, currently only for shutdown */
            case HAX_EXIT_STATECHANGE:
                dprint("VCPU shutdown request\n");
                qemu_system_reset_request();
                hax_prepare_emulation(env);
                cpu_dump_state(env, stderr, fprintf, 0);
                ret = HAX_EMUL_EXITLOOP;
                break;
            case HAX_EXIT_UNKNOWN_VMEXIT:
                dprint("Unknown VMX exit %x from guest\n", ht->_exit_reason);
                qemu_system_reset_request();
                hax_prepare_emulation(env);
                cpu_dump_state(env, stderr, fprintf, 0);
                ret = HAX_EMUL_EXITLOOP;
                break;
            case HAX_EXIT_HLT:
                if (!(env->interrupt_request & CPU_INTERRUPT_HARD) &&
                  !(env->interrupt_request & CPU_INTERRUPT_NMI)) {
                    /* hlt instruction with interrupt disabled is shutdown */
                    env->eflags |= IF_MASK;
                    env->halted = 1;
                    env->exception_index = EXCP_HLT;
                    ret = HAX_EMUL_HLT;
                }
                break;
                /* these situation will continue to hax module */
            case HAX_EXIT_INTERRUPT:
            case HAX_EXIT_PAUSED:
                break;
            default:
                dprint("Unknow exit %x from hax\n", ht->_exit_status);
                qemu_system_reset_request();
                hax_prepare_emulation(env);
                cpu_dump_state(env, stderr, fprintf, 0);
                ret = HAX_EMUL_EXITLOOP;
                break;
        }
    }while (!ret);

    if (env->exit_request) {
        env->exit_request = 0;
        env->exception_index = EXCP_INTERRUPT;
    }
    return ret;
}

/*
 * return 1 when need to emulate, 0 when need to exit loop
 */
int hax_vcpu_exec(CPUState *env)
{
    int next = 0, ret = 0;
    struct hax_vcpu_state *vcpu;

    if (env->hax_vcpu->emulation_state != HAX_EMULATE_STATE_NONE)
        return 1;

    vcpu = env->hax_vcpu;
    next = hax_vcpu_hax_exec(env);
    switch (next)
    {
        case HAX_EMUL_ONE:
            ret = 1;
            env->hax_vcpu->emulation_state = HAX_EMULATE_STATE_MMIO;
            hax_prepare_emulation(env);
            break;
        case HAX_EMUL_REAL:
            ret = 1;
            env->hax_vcpu->emulation_state =
              HAX_EMULATE_STATE_REAL;
            hax_prepare_emulation(env);
            break;
        case HAX_EMUL_HLT:
        case HAX_EMUL_EXITLOOP:
            break;
        default:
            dprint("Unknown hax vcpu exec return %x\n", next);
            abort();
    }

    return ret;
}

#define HAX_RAM_INFO_ROM 0x1

static void set_v8086_seg(struct segment_desc_t *lhs, const SegmentCache *rhs)
{
    memset(lhs, 0, sizeof(struct segment_desc_t ));
    lhs->selector = rhs->selector;
    lhs->base = rhs->base;
    lhs->limit = rhs->limit;
    lhs->type = 3;
    lhs->present = 1;
    lhs->dpl = 3;
    lhs->operand_size = 0;
    lhs->desc = 1;
    lhs->long_mode = 0;
    lhs->granularity = 0;
    lhs->available = 0;
}

static void get_seg(SegmentCache *lhs, const struct segment_desc_t *rhs)
{
    lhs->selector = rhs->selector;
    lhs->base = rhs->base;
    lhs->limit = rhs->limit;
    lhs->flags =
      (rhs->type << DESC_TYPE_SHIFT)
      | (rhs->present * DESC_P_MASK)
      | (rhs->dpl << DESC_DPL_SHIFT)
      | (rhs->operand_size << DESC_B_SHIFT)
      | (rhs->desc * DESC_S_MASK)
      | (rhs->long_mode << DESC_L_SHIFT)
      | (rhs->granularity * DESC_G_MASK)
      | (rhs->available * DESC_AVL_MASK);
}

static void set_seg(struct segment_desc_t *lhs, const SegmentCache *rhs)
{
    unsigned flags = rhs->flags;

    memset(lhs, 0, sizeof(struct segment_desc_t));
    lhs->selector = rhs->selector;
    lhs->base = rhs->base;
    lhs->limit = rhs->limit;
    lhs->type = (flags >> DESC_TYPE_SHIFT) & 15;
    lhs->present = (flags & DESC_P_MASK) != 0;
    lhs->dpl = rhs->selector & 3;
    lhs->operand_size = (flags >> DESC_B_SHIFT) & 1;
    lhs->desc = (flags & DESC_S_MASK) != 0;
    lhs->long_mode = (flags >> DESC_L_SHIFT) & 1;
    lhs->granularity = (flags & DESC_G_MASK) != 0;
    lhs->available = (flags & DESC_AVL_MASK) != 0;
}

static void hax_getput_reg(uint64_t *hax_reg, target_ulong *qemu_reg, int set)
{
    target_ulong reg = *hax_reg;

    if (set)
        *hax_reg = *qemu_reg;
    else
        *qemu_reg = reg;
}

/* The sregs has been synced with HAX kernel already before this call */
static int hax_get_segments(CPUState *env, struct vcpu_state_t *sregs)
{
    get_seg(&env->segs[R_CS], &sregs->_cs);
    get_seg(&env->segs[R_DS], &sregs->_ds);
    get_seg(&env->segs[R_ES], &sregs->_es);
    get_seg(&env->segs[R_FS], &sregs->_fs);
    get_seg(&env->segs[R_GS], &sregs->_gs);
    get_seg(&env->segs[R_SS], &sregs->_ss);

    get_seg(&env->tr, &sregs->_tr);
    get_seg(&env->ldt, &sregs->_ldt);
    env->idt.limit = sregs->_idt.limit;
    env->idt.base = sregs->_idt.base;
    env->gdt.limit = sregs->_gdt.limit;
    env->gdt.base = sregs->_gdt.base;
    return 0;
}

static int hax_set_segments(CPUState *env, struct vcpu_state_t *sregs)
{
    if ((env->eflags & VM_MASK)) {
        set_v8086_seg(&sregs->_cs, &env->segs[R_CS]);
        set_v8086_seg(&sregs->_ds, &env->segs[R_DS]);
        set_v8086_seg(&sregs->_es, &env->segs[R_ES]);
        set_v8086_seg(&sregs->_fs, &env->segs[R_FS]);
        set_v8086_seg(&sregs->_gs, &env->segs[R_GS]);
        set_v8086_seg(&sregs->_ss, &env->segs[R_SS]);
    } else {
        set_seg(&sregs->_cs, &env->segs[R_CS]);
        set_seg(&sregs->_ds, &env->segs[R_DS]);
        set_seg(&sregs->_es, &env->segs[R_ES]);
        set_seg(&sregs->_fs, &env->segs[R_FS]);
        set_seg(&sregs->_gs, &env->segs[R_GS]);
        set_seg(&sregs->_ss, &env->segs[R_SS]);

        if (env->cr[0] & CR0_PE_MASK) {
            /* force ss cpl to cs cpl */
            sregs->_ss.selector = (sregs->_ss.selector & ~3) |
              (sregs->_cs.selector & 3);
            sregs->_ss.dpl = sregs->_ss.selector & 3;
        }
    }

    set_seg(&sregs->_tr, &env->tr);
    set_seg(&sregs->_ldt, &env->ldt);
    sregs->_idt.limit = env->idt.limit;
    sregs->_idt.base = env->idt.base;
    sregs->_gdt.limit = env->gdt.limit;
    sregs->_gdt.base = env->gdt.base;
    return 0;
}

/*
 * After get the state from the kernel module, some
 * qemu emulator state need be updated also
 */
static int hax_setup_qemu_emulator(CPUState *env)
{

#define HFLAG_COPY_MASK ~( \
  HF_CPL_MASK | HF_PE_MASK | HF_MP_MASK | HF_EM_MASK | \
  HF_TS_MASK | HF_TF_MASK | HF_VM_MASK | HF_IOPL_MASK | \
  HF_OSFXSR_MASK | HF_LMA_MASK | HF_CS32_MASK | \
  HF_SS32_MASK | HF_CS64_MASK | HF_ADDSEG_MASK)

    uint32_t hflags;

    hflags = (env->segs[R_CS].flags >> DESC_DPL_SHIFT) & HF_CPL_MASK;
    hflags |= (env->cr[0] & CR0_PE_MASK) << (HF_PE_SHIFT - CR0_PE_SHIFT);
    hflags |= (env->cr[0] << (HF_MP_SHIFT - CR0_MP_SHIFT)) &
      (HF_MP_MASK | HF_EM_MASK | HF_TS_MASK);
    hflags |= (env->eflags & (HF_TF_MASK | HF_VM_MASK | HF_IOPL_MASK));
    hflags |= (env->cr[4] & CR4_OSFXSR_MASK) <<
      (HF_OSFXSR_SHIFT - CR4_OSFXSR_SHIFT);

    if (env->efer & MSR_EFER_LMA) {
        hflags |= HF_LMA_MASK;
    }

    if ((hflags & HF_LMA_MASK) && (env->segs[R_CS].flags & DESC_L_MASK)) {
        hflags |= HF_CS32_MASK | HF_SS32_MASK | HF_CS64_MASK;
    } else {
        hflags |= (env->segs[R_CS].flags & DESC_B_MASK) >>
          (DESC_B_SHIFT - HF_CS32_SHIFT);
        hflags |= (env->segs[R_SS].flags & DESC_B_MASK) >>
          (DESC_B_SHIFT - HF_SS32_SHIFT);
        if (!(env->cr[0] & CR0_PE_MASK) ||
          (env->eflags & VM_MASK) ||
          !(hflags & HF_CS32_MASK)) {
            hflags |= HF_ADDSEG_MASK;
        } else {
            hflags |= ((env->segs[R_DS].base |
                  env->segs[R_ES].base |
                  env->segs[R_SS].base) != 0) <<
              HF_ADDSEG_SHIFT;
        }
    }
    env->hflags = (env->hflags & HFLAG_COPY_MASK) | hflags;
    return 0;
}

static int hax_sync_vcpu_register(CPUState *env, int set)
{
    struct vcpu_state_t regs;
    int ret;
    memset(&regs, 0, sizeof(struct vcpu_state_t));

    if (!set)
    {
        ret = hax_sync_vcpu_state(env, &regs, 0);
        if (ret < 0)
            return -1;
    }

    /*generic register */
    hax_getput_reg(&regs._rax, &env->regs[R_EAX], set);
    hax_getput_reg(&regs._rbx, &env->regs[R_EBX], set);
    hax_getput_reg(&regs._rcx, &env->regs[R_ECX], set);
    hax_getput_reg(&regs._rdx, &env->regs[R_EDX], set);
    hax_getput_reg(&regs._rsi, &env->regs[R_ESI], set);
    hax_getput_reg(&regs._rdi, &env->regs[R_EDI], set);
    hax_getput_reg(&regs._rsp, &env->regs[R_ESP], set);
    hax_getput_reg(&regs._rbp, &env->regs[R_EBP], set);

    hax_getput_reg(&regs._rflags, &env->eflags, set);
    hax_getput_reg(&regs._rip, &env->eip, set);

    if (set)
    {

        regs._cr0 = env->cr[0];
        regs._cr2 = env->cr[2];
        regs._cr3 = env->cr[3];
        regs._cr4 = env->cr[4];
        hax_set_segments(env, &regs);
    }
    else
    {
        env->cr[0] = regs._cr0;
        env->cr[2] = regs._cr2;
        env->cr[3] = regs._cr3;
        env->cr[4] = regs._cr4;
        hax_get_segments(env, &regs);
    }

    if (set)
    {
        ret = hax_sync_vcpu_state(env, &regs, 1);
        if (ret < 0)
            return -1;
    }
    if (!set)
        hax_setup_qemu_emulator(env);
    return 0;
}

static void hax_msr_entry_set(struct vmx_msr *item,
  uint32_t index, uint64_t value)
{
    item->entry = index;
    item->value = value;
}

static int hax_get_msrs(CPUState *env)
{
    struct hax_msr_data md;
    struct vmx_msr *msrs = md.entries;
    int ret, i, n;

    n = 0;
    msrs[n++].entry = MSR_IA32_SYSENTER_CS;
    msrs[n++].entry = MSR_IA32_SYSENTER_ESP;
    msrs[n++].entry = MSR_IA32_SYSENTER_EIP;
    msrs[n++].entry = MSR_IA32_TSC;
    md.nr_msr = n;
    ret = hax_sync_msr(env, &md, 0);
    if (ret < 0)
        return ret;

    for (i = 0; i < md.done; i++) {
        switch (msrs[i].entry) {
            case MSR_IA32_SYSENTER_CS:
                env->sysenter_cs = msrs[i].value;
                break;
            case MSR_IA32_SYSENTER_ESP:
                env->sysenter_esp = msrs[i].value;
                break;
            case MSR_IA32_SYSENTER_EIP:
                env->sysenter_eip = msrs[i].value;
                break;
            case MSR_IA32_TSC:
                env->tsc = msrs[i].value;
                break;
        }
    }

    return 0;
}

static int hax_set_msrs(CPUState *env)
{
    struct hax_msr_data md;
    struct vmx_msr *msrs;
    msrs = md.entries;
    int n = 0;

    memset(&md, 0, sizeof(struct hax_msr_data));
    hax_msr_entry_set(&msrs[n++], MSR_IA32_SYSENTER_CS, env->sysenter_cs);
    hax_msr_entry_set(&msrs[n++], MSR_IA32_SYSENTER_ESP, env->sysenter_esp);
    hax_msr_entry_set(&msrs[n++], MSR_IA32_SYSENTER_EIP, env->sysenter_eip);
    hax_msr_entry_set(&msrs[n++], MSR_IA32_TSC, env->tsc);
    md.nr_msr = n;
    md.done = 0;

    return hax_sync_msr(env, &md, 1);

}

static int hax_get_fpu(CPUState *env)
{
    struct fx_layout fpu;
    int i, ret;

    ret = hax_sync_fpu(env, &fpu, 0);
    if (ret < 0)
        return ret;

    env->fpstt = (fpu.fsw >> 11) & 7;
    env->fpus = fpu.fsw;
    env->fpuc = fpu.fcw;
    for (i = 0; i < 8; ++i)
        env->fptags[i] = !((fpu.ftw >> i) & 1);
    memcpy(env->fpregs, fpu.st_mm, sizeof(env->fpregs));

    memcpy(env->xmm_regs, fpu.mmx_1, sizeof(fpu.mmx_1));
    memcpy((XMMReg *)(env->xmm_regs) + 8, fpu.mmx_2, sizeof(fpu.mmx_2));
    env->mxcsr = fpu.mxcsr;

    return 0;
}

static int hax_set_fpu(CPUState *env)
{
    struct fx_layout fpu;
    int i;

    memset(&fpu, 0, sizeof(fpu));
    fpu.fsw = env->fpus & ~(7 << 11);
    fpu.fsw |= (env->fpstt & 7) << 11;
    fpu.fcw = env->fpuc;

    for (i = 0; i < 8; ++i)
        fpu.ftw |= (!env->fptags[i]) << i;

    memcpy(fpu.st_mm, env->fpregs, sizeof (env->fpregs));
    memcpy(fpu.mmx_1, env->xmm_regs, sizeof (fpu.mmx_1));
    memcpy(fpu.mmx_2, (XMMReg *)(env->xmm_regs) + 8, sizeof (fpu.mmx_2));

    fpu.mxcsr = env->mxcsr;

    return hax_sync_fpu(env, &fpu, 1);
}

int hax_arch_get_registers(CPUState *env)
{
    int ret;

    ret = hax_sync_vcpu_register(env, 0);
    if (ret < 0)
        return ret;

    ret = hax_get_fpu(env);
    if (ret < 0)
        return ret;

    ret = hax_get_msrs(env);
    if (ret < 0)
        return ret;

    return 0;
}

static int hax_arch_set_registers(CPUState *env)
{
    int ret;
    ret = hax_sync_vcpu_register(env, 1);

    if (ret < 0)
    {
        dprint("Failed to sync vcpu reg\n");
        return ret;
    }
    ret = hax_set_fpu(env);
    if (ret < 0)
    {
        dprint("FPU failed\n");
        return ret;
    }
    ret = hax_set_msrs(env);
    if (ret < 0)
    {
        dprint("MSR failed\n");
        return ret;
    }

    return 0;
}

void hax_vcpu_sync_state(CPUState *env, int modified)
{
    if (hax_enabled()) {
        if (modified)
            hax_arch_set_registers(env);
        else
            hax_arch_get_registers(env);
    }
}

/*
 * This is simpler than the one for KVM because we don't support
 * direct I/O device assignment at this point.
 */
int hax_sync_vcpus(void)
{
    if (hax_enabled())
    {
        CPUState *env;

        env = first_cpu;
        if (!env)
            return 0;

        for (; env != NULL; env = env->next_cpu) {
            int ret;

            ret = hax_arch_set_registers(env);
            if (ret < 0)
            {
                dprint("Failed to sync HAX vcpu context\n");
                exit(1);
            }
        }
    }

    return 0;
}

void hax_reset_vcpu_state(void *opaque)
{
    CPUState *env;
    for (env = first_cpu; env != NULL; env = env->next_cpu)
    {
        if (env->hax_vcpu)
        {
            env->hax_vcpu->emulation_state  = HAX_EMULATE_STATE_INITIAL;
            env->hax_vcpu->tunnel->user_event_pending = 0;
            env->hax_vcpu->tunnel->ready_for_interrupt_injection = 0;
        }
    }
}
