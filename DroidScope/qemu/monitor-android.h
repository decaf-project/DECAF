/* This file is included from monitor.c, it's purpose is to hold as much
 * Android-specific stuff as possible to ease upstream integrations.
 */

Monitor*
monitor_fake_new(void* opaque, MonitorFakeFunc cb)
{
    Monitor* mon;

    assert(cb != NULL);
    mon = qemu_mallocz(sizeof(*mon));
    mon->fake_opaque = opaque;
    mon->fake_func   = cb;
    mon->fake_count  = 0;

    return mon;
}

int
monitor_fake_get_bytes(Monitor* mon)
{
    assert(mon->fake_func != NULL);
    return mon->fake_count;
}

void
monitor_fake_free(Monitor* mon)
{
    assert(mon->fake_func != NULL);
    free(mon);
}

/* This replaces the definition in monitor.c which is in a
 * #ifndef CONFIG_ANDROID .. #endif block.
 */
void monitor_flush(Monitor *mon)
{
    if (!mon)
        return;

    if (mon->fake_func != NULL) {
        mon->fake_func(mon->fake_opaque, (void*)mon->outbuf, mon->outbuf_index);
        mon->outbuf_index = 0;
        mon->fake_count += mon->outbuf_index;
    } else if (!mon->mux_out) {
        qemu_chr_write(mon->chr, mon->outbuf, mon->outbuf_index);
        mon->outbuf_index = 0;
    }
}
