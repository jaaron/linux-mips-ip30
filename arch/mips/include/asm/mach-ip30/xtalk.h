/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004-2007 Stanislaw Skowronek <skylark@linux-mips.org>
 *		 2007 Joshua Kinard <kumba@gentoo.org>
 */

#ifndef __ASM_MACH_IP30_XTALK_H
#define __ASM_MACH_IP30_XTALK_H

#include <asm/sn/types.h>

#define IP30_XTALK_NUM_WID	16

/* Xtalk Device Mfgr IDs */
#define XTALK_XBOW_MFGR_ID	0x0	/* IP30 XBow Chip */
#define XTALK_XXBOW_MFGR_ID	0x0	/* IP35 Xbow + XBridge chip */
#define XTALK_ODYS_MFGR_ID	0x023	/* Odyssey / VPro */
#define XTALK_TPU_MFGR_ID	0x024	/* Tensor Processor Unit */
#define XTALK_XBRDG_MFGR_ID	0x024
#define XTALK_HEART_MFGR_ID	0x036	/* IP30 HEART Chip */
#define XTALK_BRIDG_MFGR_ID	0x036	/* PCI Bridge */
#define XTALK_HUB_MFGR_ID	0x036
#define XTALK_BDRCK_MFGR_ID	0x036	/* IP35 Hub Chip */
#define XTALK_IMPCT_MFGR_ID	0x2aa	/* HQ4 / ImpactSR */
#define XTALK_KONA_MFGR_ID	0x2aa
#define XTALK_NULL_MFGR_ID	-1	/* NULL */

/* Xtalk Device Part IDs */
#define XTALK_XBOW_PART_ID	0x0
#define XTALK_XXBOW_PART_ID	0xd000
#define XTALK_ODYS_PART_ID	0xc013
#define XTALK_TPU_PART_ID	0xc202
#define XTALK_XBRDG_PART_ID	0xd002
#define XTALK_HEART_PART_ID	0xc001
#define XTALK_BRIDG_PART_ID	0xc002
#define XTALK_HUB_PART_ID	0xc101
#define XTALK_BDRCK_PART_ID	0xc110
#define XTALK_IMPCT_PART_ID	0xc003
#define XTALK_KONA_PART_ID	0xc102
#define XTALK_NULL_PART_ID	-1

/* Xtalk Misc */
#define XTALK_NODEV		0xffffffff
#define XTALK_LOW_DEV		8	/* Lowest dev possible (HEART) */
#define XTALK_HIGH_DEV		15	/* Highest dev possible (BRIDGE) */
#define XTALK_XBOW		0	/* XBow is always 0 */
#define XTALK_HEART		8	/* HEART is always 8 */
#define XTALK_PCIBR		13	/* PCI Cage is always 13 */
#define XTALK_BRIDGE		15	/* Main Bridge is always 15 */
#define XTALK_XIO1		12	/* Main XIO Slot / GFX Card */

/* Xbow macros */
#define XBOW_REG_LINK_STAT_0		0x114
#define XBOW_REG_LINK_BLOCK_SIZE	0x40
#define XBOW_REG_LINK_ALIVE		0x80000000

/* For Xtalk Widget identification */
struct widget_ident {
	unsigned mfgr;
	unsigned part;
	char *name;
	char *revs[16];
};

unsigned ip30_xtalk_get_id(int wid);
unsigned long ip30_xtalk_swin(int wid);
int ip30_xtalk_find(unsigned mfgr, unsigned part, int last);
int bridge_probe(nasid_t nasid, int widget_id, int masterwid);

#endif /* __ASM_MACH_IP30_XTALK_H */
