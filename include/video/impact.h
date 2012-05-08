/*
 *  linux/drivers/video/impact.h -- SGI Octane MardiGras (IMPACTSR) graphics
 *                                  SGI Indigo2 MardiGras (IMPACT) graphics
 *
 *  Copyright (c) 2004 by Stanislaw Skowronek
 *  Copyright (c) 2005 by Peter Fuerst (Indigo2 Support)
 *  Copyright (c) 2011 by Joshua Kinard (Fixes, Maintenance)
 *  
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef IMPACT_H
#define IMPACT_H

#if defined(CONFIG_SGI_IP22) || defined(CONFIG_SGI_IP26) || defined(CONFIG_SGI_IP28)
  #define SGI_INDIGO2 1
#elif defined(CONFIG_SGI_IP30)
  #undef SGI_INDIGO2
#endif

/* Convenient access macros */
#define IMPACT_REG64(vma, off)		(*(volatile u64 *)((vma) + (off)))
#define IMPACT_REG32(vma, off)		(*(volatile u32 *)((vma) + (off)))
#define IMPACT_REG16(vma, off)		(*(volatile u16 *)((vma) + (off)))
#define IMPACT_REG8(vma, off)		(*(volatile u8 *)((vma) + (off)))


#ifdef SGI_INDIGO2
  /* Impact (HQ3) register offsets */
  #define IMPACT_CFIFO(vma)		IMPACT_REG64(vma, 0x70080)
  #define IMPACT_CFIFOW(vma)		IMPACT_REG32(vma, 0x70080)
  #define IMPACT_CFIFOW1(vma)		IMPACT_REG32(vma, 0x70084)
  #define IMPACT_CFIFOP(vma)		IMPACT_REG64(vma, 0x50080)
  #define IMPACT_CFIFOPW(vma)		IMPACT_REG32(vma, 0x50080)
  #define IMPACT_CFIFOPW1(vma)		IMPACT_REG32(vma, 0x50084)

  #define IMPACT_STATUS(vma)		IMPACT_REG32(vma, 0x70000)
  #define IMPACT_FIFOSTATUS(vma)	IMPACT_REG32(vma, 0x70004)
  #define IMPACT_GIOSTATUS(vma)		IMPACT_REG32(vma, 0x70100)
  #define IMPACT_DMABUSY(vma)		IMPACT_REG32(vma, 0x70104)

  #define IMPACT_CFIFO_HW(vma)		IMPACT_REG32(vma, 0x50020)
  #define IMPACT_CFIFO_LW(vma)		IMPACT_REG32(vma, 0x50024)
  #define IMPACT_CFIFO_DELAY(vma)	IMPACT_REG32(vma, 0x50028)
  #define IMPACT_DFIFO_HW(vma)		IMPACT_REG32(vma, 0x5002c)
  #define IMPACT_DFIFO_LW(vma)		IMPACT_REG32(vma, 0x50030)
  #define IMPACT_DFIFO_DELAY(vma)	IMPACT_REG32(vma, 0x50034)

  #define IMPACT_XMAP_OFF(off)		(0x61c00 + (off))
  #define IMPACT_VC3_OFF(off)		(0x62000 + (off))
  #define IMPACT_RSS_OFF(off)		(0x7c000 + (off))

#else
  /* Xtalk on IP30 */
  #define IMPACTSR_XTALK_MFGR		0x2aa
  #define IMPACTSR_XTALK_PART		0xc003

  /* ImpactSR (HQ4) register offsets */
  #define IMPACT_CFIFO(vma)		IMPACT_REG64(vma, 0x20400)
  #define IMPACT_CFIFOW(vma)		IMPACT_REG32(vma, 0x20400)
  #define IMPACT_CFIFOW1(vma)		IMPACT_REG32(vma, 0x20404)
  #define IMPACT_CFIFOP(vma)		IMPACT_REG64(vma, 0x130400)
  #define IMPACT_CFIFOPW(vma)		IMPACT_REG32(vma, 0x130400)
  #define IMPACT_CFIFOPW1(vma)		IMPACT_REG32(vma, 0x130404)

  #define IMPACT_STATUS(vma)		IMPACT_REG32(vma, 0x20000)
  #define IMPACT_FIFOSTATUS(vma)	IMPACT_REG32(vma, 0x20008)
  #define IMPACT_GIOSTATUS(vma)		IMPACT_REG32(vma, 0x20100)
  #define IMPACT_DMABUSY(vma)		IMPACT_REG32(vma, 0x20200)

  #define IMPACT_CFIFO_HW(vma)		IMPACT_REG32(vma, 0x40000)
  #define IMPACT_CFIFO_LW(vma)		IMPACT_REG32(vma, 0x40008)
  #define IMPACT_CFIFO_DELAY(vma)	IMPACT_REG32(vma, 0x40010)
  #define IMPACT_DFIFO_HW(vma)		IMPACT_REG32(vma, 0x40020)
  #define IMPACT_DFIFO_LW(vma)		IMPACT_REG32(vma, 0x40028)
  #define IMPACT_DFIFO_DELAY(vma)	IMPACT_REG32(vma, 0x40030)


  #define IMPACT_
  #define IMPACT_XMAP_OFF(off)		(0x71c00 + (off))
  #define IMPACT_VC3_OFF(off)		(0x72000 + (off))
  #define IMPACT_RSS_OFF(off)		(0x2c000 + (off))
#endif

#define IMPACT_RESTATUS(vma)		IMPACT_REG32(vma, IMPACT_RSS_OFF(0x578))

#define IMPACT_XMAP_PP1SELECT(vma)	IMPACT_REG8(vma, IMPACT_XMAP_OFF(0x008))
#define IMPACT_XMAP_INDEX(vma)		IMPACT_REG8(vma, IMPACT_XMAP_OFF(0x088))
#define IMPACT_XMAP_CONFIG(vma)		IMPACT_REG32(vma, IMPACT_XMAP_OFF(0x100))
#define IMPACT_XMAP_CONFIGB(vma)	IMPACT_REG8(vma, IMPACT_XMAP_OFF(0x108))
#define IMPACT_XMAP_BUF_SELECT(vma)	IMPACT_REG32(vma, IMPACT_XMAP_OFF(0x180))
#define IMPACT_XMAP_MAIN_MODE(vma)	IMPACT_REG32(vma, IMPACT_XMAP_OFF(0x200))
#define IMPACT_XMAP_OVERLAY_MODE(vma)	IMPACT_REG32(vma, IMPACT_XMAP_OFF(0x280))
#define IMPACT_XMAP_DIB(vma)		IMPACT_REG32(vma, IMPACT_XMAP_OFF(0x300))
#define IMPACT_XMAP_DIB_DW(vma)		IMPACT_REG32(vma, IMPACT_XMAP_OFF(0x340))
#define IMPACT_XMAP_RE_RAC(vma)		IMPACT_REG32(vma, IMPACT_XMAP_OFF(0x380))

#define IMPACT_VC3_INDEX(vma)		IMPACT_REG8(vma, IMPACT_VC3_OFF(0x008))
#define IMPACT_VC3_INDEXDATA(vma)	IMPACT_REG32(vma, IMPACT_VC3_OFF(0x038))
#define IMPACT_VC3_DATA(vma)		IMPACT_REG16(vma, IMPACT_VC3_OFF(0x0b0))
#define IMPACT_VC3_RAM(vma)		IMPACT_REG16(vma, IMPACT_VC3_OFF(0x190))

#define IMPACT_BDVERS0(vma)		IMPACT_REG8(vma, IMPACT_VC3_OFF(0x408))
#define IMPACT_BDVERS1(vma)		IMPACT_REG8(vma, IMPACT_VC3_OFF(0x488))


/* FIFO status */
#ifdef SGI_INDIGO2
  #define IMPACT_CFIFO_MAX		64
#else
  #define IMPACT_CFIFO_MAX		128
#endif
#define IMPACT_BFIFO_MAX		16


/* Commands for CFIFO */
#define IMPACT_CMD_WRITERSS(reg, val)				\
	(((0x00180004L | ((reg) << 8)) << 32) | ((unsigned)(val) & 0xffffffff))

#define IMPACT_CMD_EXECRSS(reg, val)				\
	(((0x001c0004L | ((reg) << 8)) << 32) | ((unsigned)(val) & 0xffffffff))

#define IMPACT_CMD_GLINE_XSTARTF(v)	IMPACT_CMD_WRITERSS(0x00c, v)
#define IMPACT_CMD_IR_ALIAS(v)		IMPACT_CMD_EXECRSS(0x045, v)
#define IMPACT_CMD_BLOCKXYSTARTI(x, y)	IMPACT_CMD_WRITERSS(0x046, ((x) << 16) | (y))
#define IMPACT_CMD_BLOCKXYENDI(x, y)	IMPACT_CMD_WRITERSS(0x047, ((x) << 16) | (y))
#define IMPACT_CMD_PACKEDCOLOR(v)	IMPACT_CMD_WRITERSS(0x05b, v)
#define IMPACT_CMD_RED(v)		IMPACT_CMD_WRITERSS(0x05c, v)
#define IMPACT_CMD_ALPHA(v)		IMPACT_CMD_WRITERSS(0x05f, v)
#define IMPACT_CMD_CHAR(v)		IMPACT_CMD_EXECRSS(0x070, v)
#define IMPACT_CMD_CHAR_H(v)		IMPACT_CMD_WRITERSS(0x070, v)
#define IMPACT_CMD_CHAR_L(v)		IMPACT_CMD_EXECRSS(0x071, v)
#define IMPACT_CMD_XFRCONTROL(v)	IMPACT_CMD_WRITERSS(0x102, v)
#define IMPACT_CMD_FILLMODE(v)		IMPACT_CMD_WRITERSS(0x110, v)
#define IMPACT_CMD_CONFIG(v)		IMPACT_CMD_WRITERSS(0x112, v)
#define IMPACT_CMD_XYWIN(x, y)		IMPACT_CMD_WRITERSS(0x115, ((y) << 16) | (x))
#define IMPACT_CMD_BKGRD_RG(v)		IMPACT_CMD_WRITERSS(0x140, ((v) << 8))
#define IMPACT_CMD_BKGRD_BA(v)		IMPACT_CMD_WRITERSS(0x141, ((v) << 8))
#define IMPACT_CMD_WINMODE(v)		IMPACT_CMD_WRITERSS(0x14f, v)
#define IMPACT_CMD_XFRSIZE(x, y)	IMPACT_CMD_WRITERSS(0x153, ((y) << 16) | (x))
#define IMPACT_CMD_XFRMASKLO(v)		IMPACT_CMD_WRITERSS(0x156, v)
#define IMPACT_CMD_XFRMASKHI(v)		IMPACT_CMD_WRITERSS(0x157, v)
#define IMPACT_CMD_XFRCOUNTERS(x, y)	IMPACT_CMD_WRITERSS(0x158, ((y) << 16) | (x))
#define IMPACT_CMD_XFRMODE(v)		IMPACT_CMD_WRITERSS(0x159, v)
#define IMPACT_CMD_RE_TOGGLECNTX(v)	IMPACT_CMD_WRITERSS(0x15f, v)
#define IMPACT_CMD_PIXCMD(v)		IMPACT_CMD_WRITERSS(0x160, v)
#define IMPACT_CMD_PP1FILLMODE(m, o)	IMPACT_CMD_WRITERSS(0x161, (m) | (o << 26))
#define IMPACT_CMD_COLORMASKMSBS(v)	IMPACT_CMD_WRITERSS(0x162, v)
#define IMPACT_CMD_COLORMASKLSBSA(v)	IMPACT_CMD_WRITERSS(0x163, v)
#define IMPACT_CMD_COLORMASKLSBSB(v)	IMPACT_CMD_WRITERSS(0x164, v)
#define IMPACT_CMD_BLENDFACTOR(v)	IMPACT_CMD_WRITERSS(0x165, v)
#define IMPACT_CMD_DRBPOINTERS(v)	IMPACT_CMD_WRITERSS(0x16d, v)

#define IMPACT_UNSIGNED(v)		((unsigned)(v) & 0xffffffff)

#define	IMPACT_CMD_HQ_PIXELFORMAT(v)	(0x000c000400000000L | IMPACT_UNSIGNED(v))
#define	IMPACT_CMD_HQ_SCANWIDTH(v)	(0x000a020400000000L | IMPACT_UNSIGNED(v))
#define	IMPACT_CMD_HQ_DMATYPE(v)	(0x000a060400000000L | IMPACT_UNSIGNED(v))
#define	IMPACT_CMD_HQ_PG_LIST_0(v)	(0x0008000400000000L | IMPACT_UNSIGNED(v))
#define	IMPACT_CMD_HQ_PG_WIDTH(v)	(0x0008040400000000L | IMPACT_UNSIGNED(v))
#define	IMPACT_CMD_HQ_PG_OFFSET(v)	(0x0008050400000000L | IMPACT_UNSIGNED(v))
#define	IMPACT_CMD_HQ_PG_STARTADDR(v)	(0x0008060400000000L | IMPACT_UNSIGNED(v))
#define	IMPACT_CMD_HQ_PG_LINECNT(v)	(0x0008070400000000L | IMPACT_UNSIGNED(v))
#define	IMPACT_CMD_HQ_PG_WIDTHA(v)	(0x0008080400000000L | IMPACT_UNSIGNED(v))
#define IMPACT_CMD_HQ_PGSIZE(v)		(0x00482a0400000000L | IMPACT_UNSIGNED(v))
#define IMPACT_CMD_HQ_STACKPTR(v)	(0x00483a0400000000L | IMPACT_UNSIGNED(v))
#define IMPACT_CMD_HQ_TXBASE(p)		(0x00482008 | ((p) << 9))

#define IMPACT_CMD_HQ_TXMAX(p, v)				\
	(0x0048300400000000L | IMPACT_UNSIGNED(v) |		\
	 ((unsigned long)(p) << 40))

#define IMPACT_CMD_HQ_PGBITS(p, v)				\
	(0x00482b0400000000L | IMPACT_UNSIGNED(v) |		\
	 ((unsigned long)(p) << 40))

#define IMPACT_CMD_HQ_484A(p, v)				\
	((0x00484a0400000000L + ((unsigned long)(p) << 40)) |	\
	 IMPACT_UNSIGNED(v))

#define IMPACT_CMD_HQ_484B(p, v)				\
	((0x00484b0400000000L + ((unsigned long)(p) << 40)) |	\
	 IMPACT_UNSIGNED(v))


/*
 * Logic operations for the PP1:
 *    - SI = source invert
 *    - DI = dest invert
 *    - RI = result invert
 */
#define IMPACT_LO_AND			0x01
#define IMPACT_LO_SI_AND		0x04
#define IMPACT_LO_DI_AND		0x02
#define IMPACT_LO_RI_AND		0x0e

#define IMPACT_LO_OR			0x07
#define IMPACT_LO_SI_OR			0x0d
#define IMPACT_LO_DI_OR			0x0b
#define IMPACT_LO_RI_OR			0x08

#define IMPACT_LO_XOR			0x06
#define IMPACT_LO_RI_XOR		0x09

#define IMPACT_LO_NOP			0x05
#define IMPACT_LO_RI_NOP		0x0a

#define IMPACT_LO_COPY			0x03
#define IMPACT_LO_RI_COPY		0x0c

#define IMPACT_LO_CLEAR			0x00
#define IMPACT_LO_SET			0x0f

/* Blending factors */
#define IMPACT_BLEND_ALPHA		0x0704c900


/*
 * For the future.  These will replace the above
 * macros to make the code look more C-like.
 */
#if 0
static inline void ImpactCFifoCmd64(unsigned long vma, unsigned cmd,
                                    unsigned reg, unsigned val)
{
/* #if (_MIPS_SZLONG == 64) see Xmd.h */
#if 1
	IMPACT_CFIFO(vma) = (unsigned long long)(cmd | reg << 8) << 32 | val;
#else
	IMPACT_CFIFOW(vma) = cmd | reg << 8;
	IMPACT_CFIFOW(vma) = val;
#endif
}

static inline void ImpactCFifoPCmd32(unsigned long vma, unsigned cmd,
                                     unsigned reg)
{
	IMPACT_CFIFOPW(vma) = cmd | reg << 8;
}

static inline void ImpactCFifoPCmd32lo(unsigned long vma, unsigned cmd,
                                       unsigned reg)
{
	IMPACT_CFIFOPW1(vma) = cmd | reg << 8;
}

static inline void ImpactCmdWriteRss(unsigned long vma, unsigned reg,
                                     unsigned val)
{
	ImpactCFifoCmd64(vma, 0x00180004, reg, val);
}

static inline void ImpactCmdExecRss(unsigned long vma, unsigned reg,
                                    unsigned val)
{
	ImpactCFifoCmd64(vma, 0x001c0004, reg, val);
}

#define impact_cmd_gline_xstartf(a, v)		ImpactCmdWriteRss(a, 0x000c, v)
#define impact_cmd_ir_alias(a, v)		ImpactCmdExecRss( a, 0x0045, v)
#define impact_cmd_blockxystarti(a, x, y)	ImpactCmdWriteRss(a, 0x0046, (x) << 16 | (y))
#define impact_cmd_blockxyendi(a, x, y)		ImpactCmdWriteRss(a, 0x0047, (x) << 16 | (y))
#define impact_cmd_packedcolor(a, v)		ImpactCmdWriteRss(a, 0x005b, v)
#define impact_cmd_red(a, v)			ImpactCmdWriteRss(a, 0x005c, v)
#define impact_cmd_alpha(a, v)			ImpactCmdWriteRss(a, 0x005f, v)
#define impact_cmd_char(a, v)			ImpactCmdExecRss( a, 0x0070, v)
#define impact_cmd_char_h(a, v)			ImpactCmdWriteRss(a, 0x0070, v)
#define impact_cmd_char_l(a, v)			ImpactCmdExecRss( a, 0x0071, v)
#define impact_cmd_xfrcontrol(a, v)		ImpactCmdWriteRss(a, 0x0102, v)
#define impact_cmd_fillmode(a, v)		ImpactCmdWriteRss(a, 0x0110, v)
#define impact_cmd_config(a, v)			ImpactCmdWriteRss(a, 0x0112, v)
#define impact_cmd_xywin(a, x, y)		ImpactCmdWriteRss(a, 0x0115, (y) << 16 | (x))
#define impact_cmd_bkgrd_rg(a, v)		ImpactCmdWriteRss(a, 0x0140, (v) << 8)
#define impact_cmd_bkgrd_ba(a, v)		ImpactCmdWriteRss(a, 0x0141, (v) << 8)
#define impact_cmd_winmode(a, v)		ImpactCmdWriteRss(a, 0x014f, v)
#define impact_cmd_xfrsize(a, x, y)		ImpactCmdWriteRss(a, 0x0153, (y) << 16 | (x))
#define impact_cmd_xfrmasklo(a, v)		ImpactCmdWriteRss(a, 0x0156, v)
#define impact_cmd_xfrmaskhi(a, v)		ImpactCmdWriteRss(a, 0x0157, v)
#define impact_cmd_xfrcounters(a, x, y)		ImpactCmdWriteRss(a, 0x0158, (y) << 16 | (x))
#define impact_cmd_xfrmode(a, v)		ImpactCmdWriteRss(a, 0x0159, v)
#define impact_cmd_re_togglecntx(a, v)		ImpactCmdWriteRss(a, 0x015f, v)
#define impact_cmd_pixcmd(a, v)			ImpactCmdWriteRss(a, 0x0160, v)
#define impact_cmd_pp1fillmode(a, m, o)		ImpactCmdWriteRss(a, 0x0161, (m) | (o) << 26)
#define impact_cmd_colormaskmsbs(a, v)		ImpactCmdWriteRss(a, 0x0162, v)
#define impact_cmd_colormasklsbsa(a, v)		ImpactCmdWriteRss(a, 0x0163, v)
#define impact_cmd_colormasklsbsb(a, v)		ImpactCmdWriteRss(a, 0x0164, v)
#define impact_cmd_blendfactor(a, v)		ImpactCmdWriteRss(a, 0x0165, v)
#define impact_cmd_drbpointers(a, v)		ImpactCmdWriteRss(a, 0x016d, v)

#define	impact_cmd_hq_pixelformat(a, v)		ImpactCFifoCmd64(a, 0x000c0004, 0, v)
#define	impact_cmd_hq_scanwidth(a, v)		ImpactCFifoCmd64(a, 0x000a0204, 0, v)
#define	impact_cmd_hq_dmatype(a, v)		ImpactCFifoCmd64(a, 0x000a0604, 0, v)
#define	impact_cmd_hq_pg_list_0(a, v)		ImpactCFifoCmd64(a, 0x00080004, 0, v)
#define	impact_cmd_hq_pg_width(a, v)		ImpactCFifoCmd64(a, 0x00080404, 0, v)
#define	impact_cmd_hq_pg_offset(a, v)		ImpactCFifoCmd64(a, 0x00080504, 0, v)
#define	impact_cmd_hq_pg_startaddr(a, v)	ImpactCFifoCmd64(a, 0x00080604, 0, v)
#define	impact_cmd_hq_pg_linecnt(a, v)		ImpactCFifoCmd64(a, 0x00080704, 0, v)
#define	impact_cmd_hq_pg_widtha(a, v)		ImpactCFifoCmd64(a, 0x00080804, 0, v)
#define	impact_cmd_hq_dmactrl_0(a, v)		ImpactCFifoCmd64(a, 0x00080b04, 0, 0xb1 | (v) & 8)

#define impact_cmd_hq_txbase(a, p)		ImpactCFifoPCmd32lo(a, (0x00482008 | (p) << 9), 0)
#define impact_cmd_hq_txmax(a, p, v)		ImpactCFifoPCmd64(a, (0x00483004 | (p) << 8), 0, v)
#define impact_cmd_hq_pgbits(a, p, v)		ImpactCFifoPCmd64(a, (0x00482b04 + ((p) << 8)), 0, v)
#define impact_cmd_hq_pgsize(a, v)		ImpactCFifoPCmd64(a, 0x00482a04, 0, v)
#define impact_cmd_hq_stackptr(a, v)		ImpactCFifoPCmd64(a, 0x00483a04, 0, v)

#define impact_cmd_hq_484A(a, p, v)		ImpactCFifoPCmd64(a, (0x00484a04 + ((p) << 8)), 0, v)
#define impact_cmd_hq_484B(a, p, v)		ImpactCFifoPCmd64(a, (0x00484b04 + ((p) << 8)), 0, v)
#endif


#endif /* IMPACT_H */
