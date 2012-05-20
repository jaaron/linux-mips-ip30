/*
 * linux/drivers/video/impact.c -- SGI Octane MardiGras (IMPACTSR) graphics
 *                                 SGI Indigo2 MardiGras (IMPACT) graphics
 *
 *  Copyright (c) 2004 by Stanislaw Skowronek
 *  Copyright (c) 2005 by Peter Fuerst (Indigo2 Support)
 *  Copyright (c) 2011 by Joshua Kinard (Fixes, Maintenance)
 *
 *  Based on linux/drivers/video/skeletonfb.c
 *
 *  This driver, as most of the IP30 (SGI Octane) port, is a result of massive
 *  amounts of reverse engineering and trial-and-error. If anyone is interested
 *  in helping with it, please contact me: <skylark@linux-mips.org>.
 *
 *  The basic functions of this driver are filling and blitting rectangles.
 *  To achieve the latter, two DMA operations are used on Impact. It is unclear
 *  to me, why is it so, but even Xsgi (the IRIX X11 server) does it this way.
 *  It seems that fb->fb operations are not operational on these cards.
 *
 *  For this purpose, a kernel DMA pool is allocated (pool number 0). This pool
 *  is (by default) 64kB in size. An ioctl could be used to set the value at
 *  run-time. Applications can use this pool, however proper locking has to be
 *  guaranteed. Kernel should be locked out from this pool by an ioctl.
 *
 *  The Impact[SR] is quite well worked-out currently, except for the Geometry
 *  Engines (GE11). Any information about use of those devices would be very
 *  useful. It would enable a Linux OpenGL driver, as most of OpenGL calls are
 *  supported directly by the hardware. So far, I can't initialize the GE11.
 *  Verification of microcode crashes the graphics.
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#define DRV_VERSION	"0.42"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/font.h>
#include <linux/platform_device.h>

#include <video/impact.h>

#ifdef SGI_INDIGO2
  /* Impact (HQ3) registers */
  #define VAL_CFIFO_HW		0x20 /* 0x18 ? */
  #define VAL_CFIFO_LW		0x14
  #define VAL_CFIFO_DELAY	0x64
  #define VAL_DFIFO_HW		0x28
  #define VAL_DFIFO_LW		0x14
  #define VAL_DFIFO_DELAY	0xfff
  #define MSK_CFIFO_CNT		0x7f
  #define USEPOOLS		4
#else  /* CONFIG_SGI_IP30 */
  /* ImpactSR (HQ4) registers */
  #define VAL_CFIFO_HW		0x47
  #define VAL_CFIFO_LW		0x14
  #define VAL_CFIFO_DELAY	0x64
  #define VAL_DFIFO_HW		0x40
  #define VAL_DFIFO_LW		0x10
  #define VAL_DFIFO_DELAY	0x00
  #define MSK_CFIFO_CNT		0xff
  #define USEPOOLS		5
#endif

#define IMPACT_NUM_POOLS	5
#define IMPACT_KPOOL_SIZE	65536

struct impact_par {
	/* physical mmio base in HEART XTalk space */
	unsigned long mmio_base;

	/* virtual mmio base in kernel space */
	unsigned long mmio_virt;

	/*
	 * DMA Pool Management
	 *    *pool_txtbl:	txtbl[p][i] = pgidx(phys[p][i])
	 *     pool_txnum:	valid: txtbl[p][0..txnum[p]-1]
	 *     pool_txmax:	alloc: txtbl[p][0..txmax[p]-1]
	 *     pool_txphys:	txphys[p] = dma_addr(txtbl[p])
	 *
	 * Kernel DMA Pools
	 *   **kpool_virt:	virt[p]: txnum[p] page-addresses
	 *    *kpool_phys:	phys[p][i] = dma_addr(virt[p][i])
	 */
	unsigned int *pool_txtbl[IMPACT_NUM_POOLS];
	unsigned int pool_txnum[IMPACT_NUM_POOLS];
	unsigned int pool_txmax[IMPACT_NUM_POOLS];
	unsigned long pool_txphys[IMPACT_NUM_POOLS];
	unsigned long **kpool_virt[IMPACT_NUM_POOLS];
	unsigned long *kpool_phys[IMPACT_NUM_POOLS];
	unsigned int kpool_size[IMPACT_NUM_POOLS];

	/* board config */
	unsigned int num_ge, num_rss;

	/* locks to prevent simultaneous user and kernel access */
	int open_flag;
	int mmap_flag;
	spinlock_t lock;
};

static struct fb_fix_screeninfo impact_fix = {
	.id =		"Impact",
	.smem_start = 	0,
	.smem_len =	0,
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR,
	.xpanstep =	0,
	.ypanstep =	0,
	.ywrapstep =	0,
	.line_length =	0,
	.accel =	FB_ACCEL_SGI_IMPACT,
};

static struct fb_var_screeninfo impact_var = {
	.xres =		1280,
	.yres =		1024,
	.xres_virtual =	1280,
	.yres_virtual =	1024,
	.bits_per_pixel = 24,
	.red =		{ .offset = 0, .length = 8 },
	.green =	{ .offset = 8, .length = 8 },
	.blue =		{ .offset = 16, .length = 8 },
	.transp =	{ .offset = 24, .length = 8 },
};

static struct fb_info info;
static unsigned int pseudo_palette[256];
static struct impact_par current_par;
int impact_init(void);


/* --------------------- Gory Details --------------------- */
#define MMIO (((struct impact_par *)p->par)->mmio_virt)
#define PAR (*((struct impact_par *)p->par))

#if 0 /* Unused? <kumba@gentoo.org> */
static inline void impact_wait_cfifo(struct fb_info *p, int nslots)
{
	while ((IMPACT_FIFOSTATUS(MMIO) & MSK_CFIFO_CNT) > 
	       (IMPACT_CFIFO_MAX - nslots));
}
#endif

static inline void impact_wait_cfifo_empty(struct fb_info *p)
{
	while (IMPACT_FIFOSTATUS(MMIO) & MSK_CFIFO_CNT);
}

#if 0 /* Unused? <kumba@gentoo.org> */
static inline void impact_wait_bfifo(struct fb_info *p, int nslots)
{
	while ((IMPACT_GIOSTATUS(MMIO) & 0x1f) > (IMPACT_BFIFO_MAX - nslots));
}

static inline void impact_wait_bfifo_empty(struct fb_info *p)
{
	while (IMPACT_GIOSTATUS(MMIO) & 0x1f);
}
#endif

static inline void impact_wait_dma(struct fb_info *p)
{
	while (IMPACT_DMABUSY(MMIO) & 0x1f);
	while (!(IMPACT_STATUS(MMIO) & 1));
	while (!(IMPACT_STATUS(MMIO) & 2));
	while (!(IMPACT_RESTATUS(MMIO) & 0x100));
}
static inline void impact_wait_dmaready(struct fb_info *p)
{
	IMPACT_CFIFOW(MMIO) = 0x000e0100;

	while (IMPACT_DMABUSY(MMIO) & 0x1eff);
	while (!(IMPACT_STATUS(MMIO) & 2));
}

static void impact_inithq(struct fb_info *p)
{
	/* CFIFO parameters */
	IMPACT_CFIFO_HW(MMIO) = VAL_CFIFO_HW;
	IMPACT_CFIFO_LW(MMIO) = VAL_CFIFO_LW;
	IMPACT_CFIFO_DELAY(MMIO) = VAL_CFIFO_DELAY;

	/* DFIFO parameters */
	IMPACT_DFIFO_HW(MMIO) = VAL_DFIFO_HW;
	IMPACT_DFIFO_LW(MMIO) = VAL_DFIFO_LW;
	IMPACT_DFIFO_DELAY(MMIO) = VAL_DFIFO_DELAY;
}

static void impact_initrss(struct fb_info *p)
{
	/* transfer mask registers */
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_COLORMASKLSBSA(0xffffff);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_COLORMASKLSBSB(0xffffff);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_COLORMASKMSBS(0);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRMASKLO(0xffffff);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRMASKHI(0xffffff);

	/* use the main plane */
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_DRBPOINTERS(0xc8240);

	/* set the RE into vertical flip mode */
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_CONFIG(0xcac);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XYWIN(0, 0x3ff);
}

static void impact_initxmap(struct fb_info *p)
{
	/* set XMAP into 24-bpp mode */
	IMPACT_XMAP_PP1SELECT(MMIO) = 0x01;
	IMPACT_XMAP_INDEX(MMIO) = 0x00;
	IMPACT_XMAP_MAIN_MODE(MMIO) = 0x07a4;
}

static void impact_initvc3(struct fb_info *p)
{
	/* cursor-b-gone (disable DISPLAY bit) */
	IMPACT_VC3_INDEXDATA(MMIO) = 0x1d000100;
}

static inline void impact_detachtxtbl(struct fb_info *p, unsigned long pool)
{
	/* clear DMA pool */
	impact_wait_cfifo_empty(p);
	//	impact_wait_dma(p);
	IMPACT_CFIFOPW1(MMIO) = IMPACT_CMD_HQ_TXBASE(pool);
	IMPACT_CFIFOP(MMIO) = 0x0000000000000009;
	IMPACT_CFIFOP(MMIO) = IMPACT_CMD_HQ_TXMAX(pool, 0);
	IMPACT_CFIFOP(MMIO) = IMPACT_CMD_HQ_PGBITS(pool, 0);

#ifndef SGI_INDIGO2
	IMPACT_CFIFOP(MMIO) = IMPACT_CMD_HQ_484B(pool, 0x00080000);
#endif

	impact_wait_cfifo_empty(p);
	impact_wait_dmaready(p);
}

static void impact_initdma(struct fb_info *p)
{
	unsigned long pool;

	/* clear DMA pools */
	for (pool = 0; pool < 5; pool++) {
		impact_detachtxtbl(p, pool);
		PAR.pool_txmax[pool] = 0;
		PAR.pool_txnum[pool] = 0;
	}

	/* set DMA parameters */
	impact_wait_cfifo_empty(p);
	IMPACT_CFIFOP(MMIO) = IMPACT_CMD_HQ_PGSIZE(0);
	IMPACT_CFIFOP(MMIO) = IMPACT_CMD_HQ_STACKPTR(0);

#ifndef SGI_INDIGO2
	IMPACT_CFIFOP(MMIO) = IMPACT_CMD_HQ_484A(0,0x00180000);
#endif

	IMPACT_CFIFOP(MMIO) = 0x00484a0400180000;
	IMPACT_CFIFOPW(MMIO) = 0x000e0100;
	IMPACT_CFIFOPW(MMIO) = 0x000e0100;
	IMPACT_CFIFOPW(MMIO) = 0x000e0100;
	IMPACT_CFIFOPW(MMIO) = 0x000e0100;
	IMPACT_CFIFOPW(MMIO) = 0x000e0100;

#ifdef SGI_INDIGO2
	/* probably the right thing to do... */
	/* Still? <kumba@gentoo.org> */
//	IMPACT_REG32(MMIO, 0x50900) = 0x00000000;
#else
	IMPACT_REG32(MMIO, 0x40918) = 0x00680000;
	IMPACT_REG32(MMIO, 0x40920) = 0x80280000;
	IMPACT_REG32(MMIO, 0x40928) = 0x00000000;
#endif
}

static void impact_alloctxtbl(struct fb_info *p, int pool, int pages)
{
	dma_addr_t dma_handle;
	int alloc_count;

	/* grow the pool - unlikely but supported */
	if (pages > PAR.pool_txmax[pool]) {
		alloc_count = pages;
		if (alloc_count < 1024)
			alloc_count = 1024;
		if (PAR.pool_txmax[pool])
			dma_free_coherent(NULL, (PAR.pool_txmax[pool] * 4),
			                  PAR.pool_txtbl[pool],
				          PAR.pool_txphys[pool]);
		PAR.pool_txtbl[pool] =
			dma_alloc_coherent(NULL, (alloc_count * 4),
			                   &dma_handle, GFP_KERNEL);
		PAR.pool_txphys[pool] = dma_handle;
		PAR.pool_txmax[pool] = alloc_count;
	}
	PAR.pool_txnum[pool] = pages;
}

static void impact_writetxtbl(struct fb_info *p, int pool)
{
	impact_wait_cfifo_empty(p);
	//impact_wait_dma(p);

	/* inform the card about a new DMA pool */
	IMPACT_CFIFOPW1(MMIO) = IMPACT_CMD_HQ_TXBASE(pool);
	IMPACT_CFIFOP(MMIO) = PAR.pool_txphys[pool];
	IMPACT_CFIFOP(MMIO) = IMPACT_CMD_HQ_TXMAX(pool, PAR.pool_txnum[pool]);
	IMPACT_CFIFOP(MMIO) = IMPACT_CMD_HQ_PGBITS(pool, 0x0a);

#ifndef SGI_INDIGO2
	IMPACT_CFIFOP(MMIO) = IMPACT_CMD_HQ_484B(pool, 0x00180000);
#endif

	IMPACT_CFIFOPW(MMIO) = 0x000e0100;
	IMPACT_CFIFOPW(MMIO) = 0x000e0100;
	IMPACT_CFIFOPW(MMIO) = 0x000e0100;
	IMPACT_CFIFOPW(MMIO) = 0x000e0100;
	IMPACT_CFIFOPW(MMIO) = 0x000e0100;
	/* impact_wait_cfifo_empty(p); */
	/* impact_wait_dmaready(p); */
}

#if 0 /* Unused? <kumba@gentoo.org> */
static void impact_settxtbl(struct fb_info *p, int pool, unsigned *txtbl,
                            int txmax)
{
	impact_alloctxtbl(p, pool, txmax);

#ifdef SGI_INDIGO2
	void *ca = (typeof(p)) TO_CAC((unsigned long)PAR.pool_txtbl[pool]);
	memcpy(ca, txtbl, (txmax * 4));
	dma_cache_wback_inv((unsigned long)ca, (txmax * 4));
#else
 	memcpy(PAR.pool_txtbl[pool], txtbl, (txmax * 4));
#endif
	impact_writetxtbl(p, pool);
 }
#endif

static void impact_resizekpool(struct fb_info *p, int pool, int size,
				 int growonly)
{
	int pages;
	int i;
	dma_addr_t dma_handle;
	typeof(PAR.pool_txtbl[pool]) txtbl;

	if (growonly && (PAR.kpool_size[pool] >= size))
		return;

	/* single line smallcopy (1280 * 4) _must_ work */
	if (size < 8192)
		size = 8192;
	pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;

	/* before manipulating the tbl, make it unknown to the card! */
	impact_detachtxtbl(p, pool);
	if (PAR.kpool_size[pool] > 0) {
		for (i = 0; i < PAR.pool_txnum[pool]; i++) {
			unsigned long x = (typeof(x))PAR.kpool_virt[pool][i];
#if defined(CONFIG_DMA_NONCOHERENT)
			/* virt_to_page() will blow up the driver if non-coherent. */
			x = (typeof(x))phys_to_virt(__pa(x));
#endif
			ClearPageReserved(virt_to_page(x));
			dma_free_coherent(NULL, PAGE_SIZE, PAR.kpool_virt[pool][i],
			                  PAR.kpool_phys[pool][i]);
		}
		vfree(PAR.kpool_phys[pool]);
		vfree(PAR.kpool_virt[pool]);
	}

	impact_alloctxtbl(p, pool, pages);
	txtbl = PAR.pool_txtbl[pool];

#ifdef SGI_INDIGO2
	txtbl = (typeof(txtbl)) TO_CAC((unsigned long) txtbl);
#endif

	PAR.kpool_virt[pool] = vmalloc(pages * sizeof(unsigned long));
	PAR.kpool_phys[pool] = vmalloc(pages * sizeof(unsigned long));

	for (i = 0; i < PAR.pool_txnum[pool]; i++) {
		unsigned long x;

		PAR.kpool_virt[pool][i] =
			dma_alloc_coherent(NULL, PAGE_SIZE, &dma_handle, GFP_KERNEL);
		x = (typeof(x))PAR.kpool_virt[pool][i];

#if defined(CONFIG_DMA_NONCOHERENT)
		/* virt_to_page() will blow up the driver if non-coherent. */
		x = (typeof(x))phys_to_virt(__pa(x));
#endif
		SetPageReserved(virt_to_page(x));

		PAR.kpool_phys[pool][i] = dma_handle;
		txtbl[i] = (dma_handle >> PAGE_SHIFT);
	}

#ifdef SGI_INDIGO2
	i = (sizeof(*txtbl) * PAR.pool_txnum[pool]);
	dma_cache_wback_inv((unsigned long)txtbl, i);
#endif

	impact_writetxtbl(p, pool);
	PAR.kpool_size[pool] = (pages * PAGE_SIZE);
}

static void impact_rect(struct fb_info *p, int x, int y, int w, int h,
                        unsigned c, int lo)
{
	impact_wait_cfifo_empty(p);

	if (lo == IMPACT_LO_COPY)
		IMPACT_CFIFO(MMIO) = IMPACT_CMD_PP1FILLMODE(0x6300, lo);
	else
		IMPACT_CFIFO(MMIO) = IMPACT_CMD_PP1FILLMODE(0x6304, lo);

	IMPACT_CFIFO(MMIO) = IMPACT_CMD_FILLMODE(0);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_PACKEDCOLOR(c);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_BLOCKXYSTARTI(x, y);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_BLOCKXYENDI((x + w - 1), (y + h - 1));
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_IR_ALIAS(0x18);
}

#if 0 /* Unused? kumba@gentoo.org> */
static void impact_framerect(struct fb_info *p, int x, int y, int w, int h,
                             unsigned c)
{
	impact_rect(p, x, y, w, 1, c, IMPACT_LO_COPY);
	impact_rect(p, x, (y + h - 1), w, 1, c, IMPACT_LO_COPY);
	impact_rect(p, x, y, 1, h, c, IMPACT_LO_COPY);
	impact_rect(p, (x + w - 1), y, 1, h, c, IMPACT_LO_COPY);
}
#endif


#if 0 /* Unused? <kumba@gentoo.org> */
static unsigned long dcntr;
static void impact_debug(struct fb_info *p,int v)
{
	int i;
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_PIXCMD(3);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_PIXELFORMAT(0xe00);

	switch (v) {
	case 0:
		for (i = 0; i < 64; i++)
			impact_rect(p, 4 * (i & 7), 28 - 4 * (i >> 3),
				    4, 4, (dcntr & (1L << i)) ? 0xa080ff : 0x100030,
				    IMPACT_LO_COPY);
		break;

	case 1:
		dcntr++;
		for (i = 0; i < 64; i++)
			impact_rect(p, 4 * (i & 7), 28 - 4 * (i >> 3),
				    4, 4, (dcntr & (1L << i)) ? 0xff80a0 : 0x300010,
				    IMPACT_LO_COPY);
		break;

	case 2:
		for (i = 0; i < 64; i++)
			impact_rect(p, 4 * (i & 7), 28 - 4 * (i >> 3),
				    4, 4, (dcntr & (1L << i)) ? 0xa0ff80 : 0x103000,
				    IMPACTR_LO_COPY);
	}
}
#endif

static void impact_smallcopy(struct fb_info *p, unsigned sx, unsigned sy,
                               unsigned dx, unsigned dy, unsigned w,
                               unsigned h)
{
	if (w < 1 || h < 1)
		return;

	w = ((w + 1) & ~1);

	/* setup and perform DMA from RE to HOST */
	impact_wait_dma(p);

	/* select RSS to read from */
	if (PAR.num_rss == 2 && (sy & 1))
		IMPACT_CFIFO(MMIO) = IMPACT_CMD_CONFIG(0xca5);
	else	/* Beware, only I2 MaxImpact has 2 REs, SI,HI will hang ! */
		IMPACT_CFIFO(MMIO) = IMPACT_CMD_CONFIG(0xca4);

	IMPACT_CFIFO(MMIO) = IMPACT_CMD_PIXCMD(2);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_PP1FILLMODE(0x2200, IMPACT_LO_COPY);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_COLORMASKLSBSA(0xffffff);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_COLORMASKLSBSB(0xffffff);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_COLORMASKMSBS(0);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_DRBPOINTERS(0xc8240);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_BLOCKXYSTARTI(sx, (sy + h - 1));
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_BLOCKXYENDI((sx + w - 1), sy);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRMASKLO(0xffffff);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRMASKHI(0xffffff);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRSIZE(w, h);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRCOUNTERS(w, h);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRMODE(0x00080);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_FILLMODE(0x01000000);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_PIXELFORMAT(0x200);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_SCANWIDTH(w << 2);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_DMATYPE(0x0a);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_PG_LIST_0(0x80000000);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_PG_WIDTH(w << 2);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_PG_OFFSET(0);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_PG_STARTADDR(0);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_PG_LINECNT(h);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_PG_WIDTHA(w << 2);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRCONTROL(8);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_GLINE_XSTARTF(1);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_IR_ALIAS(0x18);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_DMACTRL_0(8);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRCONTROL(9);
	impact_wait_dmaready(p);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_GLINE_XSTARTF(0);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_RE_TOGGLECNTX(0);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRCOUNTERS(0, 0);

#if 0
	/*
	 * <kumba@gentoo.org>
	 * Used in IP30 ImpactSR driver, replaced by IMPACT_CMD_HQ_DMACTRL_0?
	 */
	IMPACTSR_CFIFOW(MMIO) = 0x00080b04;
	IMPACTSR_CFIFO(MMIO) = 0x000000b900190204L;
	IMPACTSR_CFIFOW(MMIO) = 0x00000009;
#endif

	/* setup and perform DMA from HOST to RE */
	impact_wait_dma(p);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_CONFIG(0xca4);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_PP1FILLMODE(0x6200, IMPACT_LO_COPY);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_BLOCKXYSTARTI(dx, (dy + h - 1));
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_BLOCKXYENDI((dx + w - 1), dy);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_FILLMODE(0x01400000);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRMODE(0x00080);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_PIXELFORMAT(0x600);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_SCANWIDTH(w << 2);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_DMATYPE(0x0c);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_PIXCMD(3);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRSIZE(w, h);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRCOUNTERS(w, h);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_GLINE_XSTARTF(1);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_IR_ALIAS(0x18);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRCONTROL(1);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_PG_LIST_0(0x80000000);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_PG_OFFSET(0);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_PG_STARTADDR(0);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_PG_LINECNT(h);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_PG_WIDTHA(w << 2);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_DMACTRL_0(0);
	IMPACT_CFIFOW1(MMIO) = 0x000e0400;
	impact_wait_dma(p);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_GLINE_XSTARTF(0);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_RE_TOGGLECNTX(0);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRCOUNTERS(0, 0);

#if 0
	/*
	 * <kumba@gentoo.org>
	 * Used in IP30 ImpactSR driver, replaced by IMPACT_CMD_HQ_DMACTRL_0?
	 */
	IMPACTSR_CFIFOW(MMIO) = 0x0080b04;
	IMPACTSR_CFIFO(MMIO) = 0x000000b1000e0400L;
#endif
}

static inline unsigned impact_getpalreg(struct fb_info *p, unsigned i)
{
	return ((unsigned *)p->pseudo_palette)[i];
}


/* ------------ Accelerated Functions --------------------- */
static void impact_fillrect(struct fb_info *p,
                            const struct fb_fillrect *region)
{
	unsigned long flags;

	spin_lock_irqsave(&PAR.lock, flags);
	if (!PAR.open_flag) {
		switch(region->rop) {
		case ROP_XOR:
			impact_rect(p, region->dx, region->dy, region->width,
			            region->height,
			            impact_getpalreg(p, region->color),
			            IMPACT_LO_XOR);
			break;

		case ROP_COPY:
		default:
			impact_rect(p, region->dx, region->dy, region->width,
			            region->height,
			            impact_getpalreg(p, region->color),
			            IMPACT_LO_COPY);
			break;
		}
 	}
	spin_unlock_irqrestore(&PAR.lock, flags);
}

static void impact_copyarea(struct fb_info *p,
                            const struct fb_copyarea *area) 
{
	unsigned sx, sy, dx, dy, w, h;
	unsigned th, ah;
	unsigned long flags;

	w = area->width;
	h = area->height;

	if ((w < 1) || (h < 1))
		return;

	spin_lock_irqsave(&PAR.lock, flags);
	if (!PAR.open_flag) {
		sx = area->sx;
		sy = (0x3ff - (area->sy + h - 1));
		dx = area->dx;
		dy = (0x3ff - (area->dy + h - 1));
		th = (PAR.kpool_size[0] / (w * 4));
		IMPACT_CFIFO(MMIO) = IMPACT_CMD_XYWIN(0, 0);

		if (dy > sy) {
			dy += h;
			sy += h;
			while (h > 0) {
				ah = ((th > h) ? h : th);
				impact_smallcopy(p, sx, (sy - ah),
				                 dx, (dy - ah), w, ah);
				dy -= ah;
				sy -= ah;
				h -= ah;
			}
		} else {
			while (h > 0) {
				ah = ((th > h) ? h : th);
				impact_smallcopy(p, sx, sy, dx, dy, w, ah);
				dy += ah;
				sy += ah;
				h -= ah;
			}
		}

		IMPACT_CFIFO(MMIO) = IMPACT_CMD_PIXCMD(0);
		IMPACT_CFIFO(MMIO) = IMPACT_CMD_HQ_PIXELFORMAT(0xe00);
		IMPACT_CFIFO(MMIO) = IMPACT_CMD_CONFIG(0xcac);
		IMPACT_CFIFO(MMIO) = IMPACT_CMD_XYWIN(0,0x3ff);
	}
	spin_unlock_irqrestore(&PAR.lock, flags);
}

/*
 * 8-bpp blits are done as PIO draw operation; the pixels are
 * unpacked into 32-bpp values from the current palette in
 * software
 */
static void impact_imageblit_8bpp(struct fb_info *p,
                                  const struct fb_image *image)
{
	int i, u, v;
	const unsigned char *dp;
	unsigned pix;
	unsigned pal[256];

	/* setup PIO to RE */
	impact_wait_cfifo_empty(p);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_PP1FILLMODE(0x6300, IMPACT_LO_COPY);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_BLOCKXYSTARTI(image->dx, image->dy);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_BLOCKXYENDI((image->dx + image->width - 1),
	                                            (image->dy + image->height - 1));
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_FILLMODE(0x00c00000);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRMODE(0x00080);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRSIZE(image->width, image->height);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRCOUNTERS(image->width, image->height);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_GLINE_XSTARTF(1);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_IR_ALIAS(0x18);
 
	/* another workaround.. 33 writes to alpha... hmm... */
	for (i = 0; i < 33; i++)
		IMPACT_CFIFO(MMIO) = IMPACT_CMD_ALPHA(0);
	IMPACTR_CFIFO(MMIO) = IMPACT_CMD_XFRCONTROL(2);

	/* pairs of pixels are sent in two writes to the RE */
	i = 0;
	dp = image->data;
	for (v = 0; v < 256; v++)
		pal[v] = impact_getpalreg(p, v);

	for (v = 0; v < image->height; v++) {
		for (u = 0; u < image->width; u++) {
			pix = pal[*(dp++)];
			if (i)
				IMPACT_CFIFO(MMIO) = IMPACT_CMD_CHAR_L(pix);
			else
				IMPACT_CFIFO(MMIO) = IMPACT_CMD_CHAR_H(pix);
			i ^= 1;
		}
	}

	if (i)
		IMPACT_CFIFO(MMIO) = IMPACT_CMD_CHAR_L(0);

	IMPACT_CFIFO(MMIO) = IMPACT_CMD_GLINE_XSTARTF(0);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_RE_TOGGLECNTX(0);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_XFRCOUNTERS(0, 0);
}

/*
 * 1-bpp blits are done as character drawing; the bitmaps are
 * drawn as 8-bit wide strips; technically, Impact supports
 * 16-pixel wide characters, but Linux bitmap alignment is 8
 * bits and most draws are 8 pixels wide (font width), anyway
 */
static void impact_imageblit_1bpp(struct fb_info *p,
                                  const struct fb_image *image) 
{
	int x, y, w, h, b;
	int u, v, a;
	const unsigned char *d;

	impact_wait_cfifo_empty(p);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_PP1FILLMODE(0x6300, IMPACT_LO_COPY);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_FILLMODE(0x400018);

	a = impact_getpalreg(p, image->fg_color);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_PACKEDCOLOR(a);

	a = impact_getpalreg(p, image->bg_color);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_BKGRD_RG(a & 0xffff);
	IMPACT_CFIFO(MMIO) = IMPACT_CMD_BKGRD_BA((a & 0xff0000) >> 16);

	x = image->dx;
	y = image->dy;
	w = image->width;
	h = image->height;
	b = ((w + 7) / 8);

	for (u = 0; u < b; u++) {
		impact_wait_cfifo_empty(p);
		a = ((w < 8) ? w : 8);
		d = (image->data + u);
		IMPACT_CFIFO(MMIO) = IMPACT_CMD_BLOCKXYSTARTI(x, y);
		IMPACT_CFIFO(MMIO) = IMPACT_CMD_BLOCKXYENDI((x + a - 1),
		                                            (y + h - 1));
		IMPACT_CFIFO(MMIO) = IMPACT_CMD_IR_ALIAS(0x18);

		for (v = 0; v < h; v++) {
			IMPACT_CFIFO(MMIO) = IMPACT_CMD_CHAR((*d) << 24);
			d += b;
		}

		w -= a;
		x += a;
	}
}

static void impact_imageblit(struct fb_info *p, const struct fb_image *image) 
{
	unsigned long flags;

	spin_lock_irqsave(&PAR.lock, flags);
	if (!PAR.open_flag) {
		switch(image->depth) {
		case 1:
			impact_imageblit_1bpp(p, image);
			break;

		case 8:
			impact_imageblit_8bpp(p, image);
			break;
		}
 	}
	spin_unlock_irqrestore(&PAR.lock, flags);
}

static int impact_sync(struct fb_info *info)
{
	return 0;
}

static int impact_blank(int blank_mode, struct fb_info *info)
{
	/* TODO */
	return 0;
}

static int impact_setcolreg(unsigned regno, unsigned red, unsigned green,
                            unsigned blue, unsigned transp,
                            struct fb_info *info)
{
	if (regno > 255)
		return 1;

	((unsigned *)info->pseudo_palette)[regno]=
	    (red >> 8) |
	    (green & 0xff00) |
	    ((blue <<8 ) & 0xff0000);

	return 0;
}


/* ------------------- Framebuffer Access -------------------- */
ssize_t impact_read(struct fb_info *info, char __user *buf, size_t count,
                    loff_t *ppos)
{
	return -EINVAL;
}

ssize_t impact_write(struct fb_info *info, const char __user *buf,
                     size_t count, loff_t *ppos)
{
	return -EINVAL;
}


/* --------------------- Userland Access --------------------- */
int impact_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	return -EINVAL;
}

int impact_mmap(struct fb_info *p, struct vm_area_struct *vma)
{
	unsigned pool, i, n;
	unsigned long size = (vma->vm_end - vma->vm_start);
	unsigned long offset = (vma->vm_pgoff << PAGE_SHIFT);
	unsigned long start = vma->vm_start;

	switch (offset) {
	case 0x0000000:
	default:
#ifdef SGI_INDIGO2
		if ((offset + size) > 0x400000)
			return -EINVAL;
#else
		/* >0x400000, >0x1000000 ? */
		if ((offset + size) > 0x200000)
			return -EINVAL;
#endif

		if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
			return -EINVAL;

		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		vma->vm_flags |= VM_IO;

		offset += MMIO;
		if (remap_pfn_range(vma, vma->vm_start, (offset >> PAGE_SHIFT),
		                    size, vma->vm_page_prot))
			return -EAGAIN;

		PAR.mmap_flag = 1;
		break;

	case 0x1000000:
	case 0x2000000:
	case 0x3000000:
	case 0x8000000:
	case 0x9000000:
	case 0xa000000:
	case 0xb000000:
		if (size > 0x1000000)
			return EINVAL;

		pool = ((offset >> 24) & 3);
		n = ((size + PAGE_SIZE - 1) >> PAGE_SHIFT);
		if ((n * PAGE_SIZE) != PAR.kpool_size[pool])
			impact_resizekpool(&info, pool, size,
			                   offset & 0x8000000);

		for (i = 0; i < n; i++) {
			if (remap_pfn_range(vma, start,
			        (PAR.kpool_phys[pool][i] >> PAGE_SHIFT),
			        PAGE_SIZE, vma->vm_page_prot))
			{
				return -EAGAIN;
			}
			start += PAGE_SIZE;
		}
		PAR.mmap_flag = 1;
		break;
	}

	return 0;
}

static int impact_open(struct fb_info *p, int user)
{
	unsigned long flags;

	spin_lock_irqsave(&PAR.lock, flags);
	if (user)
		PAR.open_flag++;
	spin_unlock_irqrestore(&PAR.lock, flags);

	return 0;
}

static int impact_release(struct fb_info *p, int user)
{
	unsigned long flags;

	spin_lock_irqsave(&PAR.lock, flags);
	if (user && PAR.open_flag) {
		PAR.open_flag--;
		if (PAR.open_flag == 0)
                        PAR.mmap_flag = 0;
	}
	spin_unlock_irqrestore(&PAR.lock, flags);

	return 0;
}


/* ---------------- Framebuffer Operations ------------------- */
static struct fb_ops impact_ops = {
	.owner		= THIS_MODULE,
	.fb_read	= impact_read,
	.fb_write	= impact_write,
	.fb_blank	= impact_blank,
	.fb_fillrect	= impact_fillrect,
	.fb_copyarea	= impact_copyarea,
	.fb_imageblit	= impact_imageblit,
	.fb_sync	= impact_sync,
	.fb_ioctl	= impact_ioctl,
	.fb_setcolreg	= impact_setcolreg,
	.fb_mmap	= impact_mmap,
	.fb_open	= impact_open,
	.fb_release	= impact_release,
};


/* -------------------- Initialization ----------------------- */
static void __devinit impact_hwinit(void)
{
	/* initialize hardware */
	impact_inithq(&info);
	impact_initvc3(&info);
	impact_initrss(&info);
	impact_initxmap(&info);
	impact_initdma(&info);
}

static int __devinit impact_devinit(void)
{
	int i;
	unsigned long gfxaddr;

#ifdef SGI_INDIGO2
	/* Address provided by ARCS */
	extern unsigned long sgi_gfxaddr;
	if (!sgi_gfxaddr)
		return -ENODEV;
	gfxaddr = sgi_gfxaddr;
#else
	/* Find the first card in Octane */
	int xwid = ip30_xtalk_find(IMPACT_XTALK_MFGR, IMPACT_XTALK_PART,
	                           IP30_XTALK_NUM_WID);
	if (xwid == -1)
		return -ENODEV;
	gfxaddr = ip30_xtalk_get_id(xwid);
#endif

	current_par.open_flag = 0;
	current_par.mmap_flag = 0;
	current_par.lock = __SPIN_LOCK_UNLOCKED(current_par.lock);

	current_par.mmio_base = gfxaddr;
	current_par.mmio_virt = (unsigned long)ioremap(current_par.mmio_base,
	                        0x200000);

	impact_fix.mmio_start = current_par.mmio_base;
	impact_fix.mmio_len = 0x200000;

	info.flags = FBINFO_FLAG_DEFAULT;
	info.screen_base = NULL;
	info.fbops = &impact_ops;
	info.fix = impact_fix;
	info.var = impact_var;
	info.par = &current_par;
	info.pseudo_palette = pseudo_palette;

	/* get board config */
#ifndef SGI_INDIGO2
	current_par.num_ge = IMPACT_BDVERS1(current_par.mmio_virt) & 3;
	current_par.num_rss = current_par.num_ge;
	info.fix.id[7] = '0' + current_par.num_rss;
#else
	current_par.num_ge = current_par.num_rss = 1;
#endif
	impact_hwinit();

	/* initialize buffers */
	impact_resizekpool(&info, 0, IMPACT_KPOOL_SIZE, 0);
	for (i = 1; i < USEPOOLS; ++i)
		impact_resizekpool(&info, i, 8192, 0);

	/* Required. */
	fb_alloc_cmap(&info.cmap, 256, 0);

	if (register_framebuffer(&info) < 0)
		return -EINVAL;

	printk(KERN_INFO "fb%d: %s frame buffer device\n", info.node,
	       info.fix.id);

	return 0;
}

static int __devinit impact_probe(struct platform_device *dev)
{
	return impact_devinit();
}

static struct platform_driver impact_driver = {
	.probe = impact_probe,
	.driver = {
		.name = "impact",
		.bus = &platform_bus_type,
		.owner  = THIS_MODULE,
		.probe = impact_probe,
		/* Add .remove someday -- these cards aren't exactly PnP. */
	},
};

static struct platform_device impact_device = {
	.name = "impact",
};

int __init impact_init(void)
{
	int ret = platform_driver_register(&impact_driver);

	if (!ret) {
		ret = platform_device_register(&impact_device);
		if (ret)
			platform_driver_unregister(&impact_driver);
	}

	return ret;
}

void __exit impact_exit(void)
{
	 platform_driver_unregister(&impact_driver);
}

module_init(impact_init);
module_exit(impact_exit);

MODULE_AUTHOR("Stanislaw Skowronek <skylark@linux-mips.org>");
MODULE_AUTHOR("Peter Fuerst <post@pfrst.de>");
MODULE_AUTHOR("Joshua Kinard <kumba@gentoo.org>");
MODULE_DESCRIPTION("Impact Video Driver for SGI Octane (HQ4) and "
                   "SGI Indigo2 (HQ3)");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
MODULE_ALIAS("platform:impact");
