/*
 * QEMU live migration
 *
 * Copyright IBM, Corp. 2008
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */

#include "qemu-common.h"
#include "migration.h"
#include "monitor.h"
#include "buffered_file.h"
#include "sysemu.h"
#include "block.h"
#include "qemu_socket.h"

//#define DEBUG_MIGRATION

#ifdef DEBUG_MIGRATION
#define dprintf(fmt, ...) \
    do { printf("migration: " fmt, ## __VA_ARGS__); } while (0)
#else
#define dprintf(fmt, ...) \
    do { } while (0)
#endif

void qemu_start_incoming_migration(const char *uri)
{
    fprintf(stderr, "migration not supported !!\n");
}

void do_migrate(Monitor *mon, int detach, const char *uri)
{
	return;
}

void do_migrate_cancel(Monitor *mon)
{
	return;
}

void do_migrate_set_speed(Monitor *mon, const char *value)
{
	return;
}

uint64_t migrate_max_downtime(void)
{
    return 0;
}

void do_migrate_set_downtime(Monitor *mon, const char *value)
{
    return;
}

void do_info_migrate(Monitor *mon)
{
	monitor_printf(mon, "No Migration support\n");
}
