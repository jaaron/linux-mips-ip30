/*
 *  linux/drivers/video/odyssey.h -- SGI Octane Odyssey graphics
 *
 *  Copyright (c) 2005 by Stanislaw Skowronek
 *  Copyright (c) 2011 by Joshua Kinard (Fixes, Maintenance)
 *  
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef ODYSSEY_H
#define ODYSSEY_H

/* Xtalk */
#define ODY_XTALK_MFGR			0x023
#define ODY_XTALK_PART			0xc013

/* Convenient access macros */
#define ODY_REG64(vma, off)		(*(volatile u64 *)((vma) + (off)))
#define ODY_REG32(vma, off)		(*(volatile u32 *)((vma) + (off)))

/* Odyssey registers */
#define ODY_CFIFO_D(vma)		ODY_REG64(vma, 0x110000)
#define ODY_CFIFO_W(vma)		ODY_REG32(vma, 0x110000)

#define ODY_DFIFO_D(vma)		ODY_REG64(vma, 0x400000)
#define ODY_DFIFO_W(vma)		ODY_REG32(vma, 0x400000)

#define ODY_STATUS0(vma)		ODY_REG32(vma, 0x001064)
#define ODY_STATUS0_CFIFO_HW		0x00008000
#define ODY_STATUS0_CFIFO_LW		0x00020000
#define ODY_DBESTAT(vma)		ODY_REG32(vma, 0x00106c)

/*
 * Logic operations for the PP1:
 *    - SI = source invert
 *    - DI = dest invert
 *    - RI = result invert
 */
#define ODY_LO_AND			0x01
#define ODY_LO_SI_AND			0x04
#define ODY_LO_DI_AND			0x02
#define ODY_LO_RI_AND			0x0e

#define ODY_LO_OR			0x07
#define ODY_LO_SI_OR			0x0d
#define ODY_LO_DI_OR			0x0b
#define ODY_LO_RI_OR			0x08

#define ODY_LO_XOR			0x06
#define ODY_LO_RI_XOR			0x09

#define ODY_LO_NOP			0x05
#define ODY_LO_RI_NOP			0x0a

#define ODY_LO_COPY			0x03
#define ODY_LO_RI_COPY			0x0c

#define ODY_LO_CLEAR			0x00
#define ODY_LO_SET			0x0f


static inline void odyssey_wait_cfifo(unsigned long mmio)
{
	while (!(ODY_STATUS0(mmio) & ODY_STATUS0_CFIFO_LW));
}

static inline void odyssey_wait_dfifo(u64 mmio, int lw)
{
	while ((ODY_DBESTAT(mmio) & 0x7f) > lw);
}

static inline void odyssey_dfifo_write(u64 mmio, unsigned reg, unsigned val)
{
	ODY_DFIFO_D(mmio) = (((u64)(0x30000001 | (reg << 14)) << 32) | val);
}


#endif /* ODYSSEY_H */
