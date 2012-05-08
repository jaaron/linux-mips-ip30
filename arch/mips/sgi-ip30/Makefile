#
# Makefile for the IP30 specific kernel interface routines under Linux.
#

obj-y	:= ip30-setup.o ip30-irq.o ip30-timer.o ip30-err.o	\
           ip30-xtalk.o ip30-power.o

obj-$(CONFIG_EARLY_PRINTK)	+= ip30-console.o
obj-$(CONFIG_SMP)		+= ip30-smp.o ip30-smp-glue.o

EXTRA_AFLAGS := $(CFLAGS)
