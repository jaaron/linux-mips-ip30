/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2005-2007 Stanislaw Skowronek
 *		 2009 Johannes Dickgreber <tanzy@gmx.de>
 */

#ifndef __ASM_MACH_IP30_RACERMP_H
#define __ASM_MACH_IP30_RACERMP_H

#include <linux/types.h>

extern void asmlinkage ip30_smp_bootstrap(void);
extern void ip30_cpu_irq_init(int cpu);

#define MPCONF_MAGIC	0xbaddeed2
#define	MPCONF_ADDR	0xa800000000000600L
#define MPCONF_SIZE  	0x80
#define MPCONF(x)	(MPCONF_ADDR + (x) * MPCONF_SIZE)

/* Octane can theoretically do 4 CPUs, but only 2 are physically possible */
#define MP_NCPU		4

#define MP_MAGIC(x)     ((void *)(MPCONF(x) + 0x00))
#define MP_PRID(x)      ((void *)(MPCONF(x) + 0x04))
#define MP_PHYSID(x)    ((void *)(MPCONF(x) + 0x08))
#define MP_VIRTID(x)    ((void *)(MPCONF(x) + 0x0c))
#define MP_SCACHESZ(x)  ((void *)(MPCONF(x) + 0x10))
#define MP_FANLOADS(x)  ((void *)(MPCONF(x) + 0x14))
#define MP_LAUNCH(x)    ((void *)(MPCONF(x) + 0x18))
#define MP_RNDVZ(x)     ((void *)(MPCONF(x) + 0x20))
#define MP_STACKADDR(x) ((void *)(MPCONF(x) + 0x40))
#define MP_LPARM(x)     ((void *)(MPCONF(x) + 0x48))
#define MP_RPARM(x)     ((void *)(MPCONF(x) + 0x50))
#define MP_IDLEFLAG(x)  ((void *)(MPCONF(x) + 0x58))

#endif /* __ASM_MACH_IP30_RACERMP_H */
