/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * ip30-xtalk.c
 * Copyright (C) 2004-2007 Stanislaw Skowronek <skylark@linux-mips.org>
 *		 2007 Joshua Kinard <kumba@gentoo.org>
 *		 2009 Johannes Dickgreber <tanzy@gmx.de>
 *
 * XIO bus probing code
 */

#include <linux/init.h>
#include <linux/kernel.h>

#include <asm/xtalk/xwidget.h>

#include <asm/mach-ip30/heart.h>
#include <asm/mach-ip30/addrs.h>
#include <asm/mach-ip30/pcibr.h>
#include <asm/mach-ip30/xtalk.h>

//extern int bridge_probe(nasid_t nasid, int widget_id, int masterwid);

static struct widget_ident widget_idents[] = {
	{
		XTALK_HEART_MFGR_ID,
		XTALK_HEART_PART_ID,
		"heart",
		{NULL, "A", "B", "C", "D", "E", "F", NULL}
	},
	{
		XTALK_XBOW_MFGR_ID,
		XTALK_XBOW_PART_ID,
		"XBow",
		{NULL, "1.0", "1.1", "1.2", "1.3", "2.0", NULL}
	},
	{
		XTALK_XXBOW_MFGR_ID,
		XTALK_XXBOW_PART_ID,
		"XXBow",
		{NULL, "1.0", "2.0", NULL}
	},
	{
		XTALK_ODYS_MFGR_ID,
		XTALK_ODYS_PART_ID,
		"Buzz / Odyssey",
		{NULL, "A", "B", NULL}
	},
	{
		XTALK_TPU_MFGR_ID,
		XTALK_TPU_PART_ID,
		"TPU",
		{"0", NULL}
	},
	{
		XTALK_XBRDG_MFGR_ID,
		XTALK_XBRDG_PART_ID,
		"XBridge",
		{NULL, "A", "B", NULL}
	},
	{
		XTALK_HEART_MFGR_ID,
		XTALK_HEART_PART_ID,
		"Heart",
		{NULL, "A", "B", "C", "D", "E", "F", NULL}
	},
	{
		XTALK_BRIDG_MFGR_ID,
		XTALK_BRIDG_PART_ID,
		"Bridge",
		{NULL, "A", "B", "C", "D", NULL}
	},
	{
		XTALK_HUB_MFGR_ID,
		XTALK_HUB_PART_ID,
		"Hub",
		{NULL, "1.0", "2.0", "2.1", "2.2", "2.3", "2.4", NULL}
	},
	{
		XTALK_BDRCK_MFGR_ID,
		XTALK_BDRCK_PART_ID,
		"Bedrock",
		{NULL, "1.0", "1.1", NULL}
	},
	{
		XTALK_IMPCT_MFGR_ID,
		XTALK_IMPCT_PART_ID,
		"HQ4 / ImpactSR",
		{NULL, "A", "B", NULL}
	},
	{
		XTALK_KONA_MFGR_ID,
		XTALK_KONA_PART_ID,
		"XG / KONA",
		{NULL}
	},
	{
		XTALK_NULL_MFGR_ID,
		XTALK_NULL_PART_ID,
		NULL,
		{NULL}
	}
};

unsigned long xtalk_get_swin(int node, int wid)
{
	return NODE_SWIN_BASE(node, wid);
}

unsigned ip30_xtalk_get_id(int wid)
{
	unsigned int link_stat;
	if (wid != XTALK_XBOW &&
		(wid < XTALK_LOW_DEV || wid > XTALK_HIGH_DEV))
			return XTALK_NODEV;

	if (wid) {
		link_stat = *(volatile unsigned int *)(RAW_NODE_SWIN_BASE(0, 0) + 
				XBOW_REG_LINK_STAT_0 + 
				XBOW_REG_LINK_BLOCK_SIZE * (wid - XTALK_LOW_DEV));
		if (!(link_stat & XBOW_REG_LINK_ALIVE))	/* is the link alive? */
			return XTALK_NODEV;
	}

	return *(volatile unsigned int *)(RAW_NODE_SWIN_BASE(0, wid) + WIDGET_ID);
}

int ip30_xtalk_find(unsigned mfgr, unsigned part, int last)
{
	unsigned wid_id;
	while (last > 0) {
		last--;
		wid_id = ip30_xtalk_get_id(last);
		if (XWIDGET_MFG_NUM(wid_id) == mfgr &&
			XWIDGET_PART_NUM(wid_id) == part)
				return last;
	}
	return -1;
}

void __init ip30_xtalk_setup(void)
{
	int i;
	unsigned int wid_id;
	unsigned int wid_part, wid_mfgr, wid_rev;
	struct widget_ident *res;

	for (i = 0; i < IP30_XTALK_NUM_WID; i++) {
		wid_id = ip30_xtalk_get_id(i);
		if (wid_id != XTALK_NODEV) {
			printk(KERN_INFO "xtalk: Detected ");
			wid_mfgr = XWIDGET_MFG_NUM(wid_id);
			wid_part = XWIDGET_PART_NUM(wid_id);
			wid_rev = XWIDGET_REV_NUM(wid_id);

			for (res = widget_idents; res->name; res++)
				if(res->mfgr == wid_mfgr && res->part == wid_part)
					break;

			if (res->name) {
				printk(res->name);
				if (res->revs[wid_rev])
					printk(" (revision %s)", res->revs[wid_rev]);
				else
					printk(" (unknown revision %d)", wid_rev);
			} else
				printk("unknown widget 0x%08x", wid_id);
			printk(" at widget %d.\n", i);
		}
	}

	i = IP30_XTALK_NUM_WID;
	while ((i = ip30_xtalk_find(PCIBR_XTALK_MFGR, PCIBR_XTALK_PART, i)) != -1)
		bridge_probe(0, i, IP30_WIDGET_HEART);
}
