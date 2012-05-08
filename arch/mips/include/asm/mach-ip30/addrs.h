/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2004-2007 Stanislaw Skowronek
 *               2007 Joshua Kinard <kumba@gentoo.org>
 */
#ifndef __ASM_MACH_IP30_ADDRS_H
#define __ASM_MACH_IP30_ADDRS_H

#include <asm/sn/types.h>

#define IP30_WIDGET_XBOW	0
#define IP30_WIDGET_HEART	8
#define IP30_WIDGET_GFX_1	12
#define IP30_WIDGET_BASEIO	15

#define NODE_SWIN_BASE(nasid, widget) \
	(0x0000000010000000 | (((unsigned long)(widget)) << 24))
#define NODE_BWIN_BASE(nasid, widget) \
	(0x0000001000000000 | (((unsigned long)(widget)) << 36))
#define RAW_NODE_SWIN_BASE(nasid, widget) \
	(0x9000000010000000 | (((unsigned long)(widget)) << 24))
#define RAW_NODE_BWIN_BASE(nasid, widget) \
	(0x9000001000000000 | (((unsigned long)(widget)) << 36))

#define SWIN_SIZE		0x1000000
#define BWIN_SIZE		0x1000000000L

#define IP30_IO_PORT_BASE	0x9000000000000000

#define IP30_MAX_PROM_MEM	0x40000000
#define IP30_MEM_BASE		0x20000000

#endif /* __ASM_MACH_IP30_ADDRS_H */
