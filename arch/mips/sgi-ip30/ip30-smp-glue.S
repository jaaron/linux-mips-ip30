/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2005-2007 Stanislaw Skowronek
 */

#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>
#include <linux/init.h>

	__CPUINIT
	.text
	.set	noat
	.set	reorder
	.align	5
LEAF(ip30_smp_bootstrap)
	move	gp, a0
	j	smp_bootstrap
	END(ip30_smp_bootstrap)
