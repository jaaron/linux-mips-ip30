/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 *	Driver for the LEDs in SGI Octane.
 *
 *	Copyright (C) 2004-2007 Stanislaw Skowronek <skylark@linux-mips.org>
 */

#include <linux/bcd.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/rtc.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/notifier.h>

#include <linux/miscdevice.h>
#include <asm/mach-ip30/leds.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>

#include <linux/ioc3.h>

#define LEDS_STREAM_SIZE	4096


/* hardware dependent LEDs driver */
static struct ioc3_driver_data *ioc3 = NULL;
static unsigned int leds_buff;

static void ip30_leds_begin(void)
{
	leds_buff = ioc3->gpdr_shadow;
}

static void ip30_leds_set(int led, unsigned char state)
{
	state >>= 7;
	leds_buff &= ~(1 << led);
	leds_buff |= state << led;
}

static void ip30_leds_end(void)
{
	ioc3_gpio(ioc3, 3, leds_buff);
}


/* generic LEDs stream interpreter part */
static spinlock_t leds_lock =  __SPIN_LOCK_UNLOCKED(&leds_lock);;
static int leds_are_open = 0;
static struct timer_list leds_timer;
static unsigned char leds_stream[LEDS_STREAM_SIZE];
static int leds_pc = 0;

static void leds_timer_proc(unsigned long param)
{
	unsigned long timer_ms = 0;
	int end_flag = 0;
	unsigned char byte1, byte2;

	ip30_leds_begin();

	while (!end_flag) {
		byte1 = leds_stream[leds_pc++];
		byte2 = leds_stream[leds_pc++];

		switch (byte1 >> 6) {
		case LEDS_OP_SET:
			ip30_leds_set(byte1 & 0x3f, byte2);
			break;
		case LEDS_OP_LOOP:
			leds_pc = 0;
		case LEDS_OP_WAIT:
			timer_ms = ((unsigned long)byte2) << (byte1 & 0x3f);
			end_flag = 1;
			break;
		case LEDS_OP_RSVD:
			printk(KERN_INFO "ip30-leds: Stream to the future!\n");
			leds_pc = 0;
			timer_ms = 0;
			end_flag = 1;
			break;
		}

		if(leds_pc >= LEDS_STREAM_SIZE) {
			printk(KERN_INFO "ip30-leds: The Neverending Stream?\n");
			leds_pc = 0;
			timer_ms = 0;
			end_flag = 1;
		}
	}

	ip30_leds_end();

	if (timer_ms) {
		timer_ms = (timer_ms * HZ) / 1000;
		leds_timer.expires = jiffies + timer_ms;
		add_timer(&leds_timer);
	}
}

static int leds_open(struct inode *inode, struct file *file)
{
	spin_lock_irq(&leds_lock);
	if (leds_are_open) {
		spin_unlock_irq(&leds_lock);
		return -EBUSY;
	}
	leds_are_open = 1;
	del_timer(&leds_timer);
	memset(leds_stream, 0xFF, LEDS_STREAM_SIZE);
	spin_unlock_irq(&leds_lock);

	return 0;
}

static int leds_release(struct inode *inode, struct file *file)
{
	spin_lock_irq(&leds_lock);
	leds_are_open = 0;
	leds_pc = 0;
	leds_timer.expires = (jiffies + 1);
	leds_timer.function = leds_timer_proc;
	add_timer(&leds_timer);
	spin_unlock_irq(&leds_lock);

	return 0;
}

static ssize_t leds_write(struct file *file, const char *buf, size_t count, loff_t * ppos)
{
	if (count > LEDS_STREAM_SIZE)
		return -ENOSPC;
	copy_from_user(leds_stream, buf, count);
	return count;
}

static struct file_operations leds_fops = {
	.owner		= THIS_MODULE,
	.open		= leds_open,
	.write		= leds_write,
	.release	= leds_release,
};

static struct miscdevice leds_dev= {
	LEDS_MINOR,
	"leds",
	&leds_fops
};


/* special hacks */
static int panic_event(struct notifier_block *this, unsigned long event,
                      void *ptr)
{
	del_timer(&leds_timer);
	memset(leds_stream, 0xFF, LEDS_STREAM_SIZE);

	leds_stream[0] = 0x00;
	leds_stream[1] = 0x00;
	leds_stream[2] = 0x01;
	leds_stream[3] = 0xFF;

	leds_stream[4] = 0x49;
	leds_stream[5] = 0x01;

	leds_stream[6] = 0x01;
	leds_stream[7] = 0x00;
	leds_stream[8] = 0x00;
	leds_stream[9] = 0xFF;

	leds_stream[10] = 0x89;
	leds_stream[11] = 0x01;

	leds_pc = 0;
	leds_timer.expires = (jiffies + 1);
	leds_timer.function = leds_timer_proc;
	add_timer(&leds_timer);

	return NOTIFY_DONE;
}

static struct notifier_block panic_block = {
	.notifier_call	= panic_event,
};


/* IOC3 SuperIO probe */
static int ioc3led_probe(struct ioc3_submodule *is, struct ioc3_driver_data *idd)
{
	int i, p = 0;
	if (ioc3 || idd->class != IOC3_CLASS_BASE_IP30)
		return 1; /* no sense in setting LEDs on the MENETs */

	ioc3 = idd;

	if (misc_register(&leds_dev)) {
		printk(KERN_ERR "ip30-leds: There is no place for me here <sob, sniff>.\n");
		return 1;
	}

	for (i = 0; i < 3; i++) {
		leds_stream[p++] = 0x00;
		leds_stream[p++] = 0x00;
		leds_stream[p++] = 0x01;
		leds_stream[p++] = 0xff;

		leds_stream[p++] = 0x48;
		leds_stream[p++] = 0x01;

		leds_stream[p++] = 0x01;
		leds_stream[p++] = 0x00;
		leds_stream[p++] = 0x00;
		leds_stream[p++] = 0xff;

		leds_stream[p++] = 0x48;
		leds_stream[p++] = 0x01;
	}
	leds_stream[p++] = 0x80;
	leds_stream[p++] = 0x00;

	init_timer(&leds_timer);
	leds_timer.expires = (jiffies + 1);
	leds_timer.function = leds_timer_proc;
	add_timer(&leds_timer);

	atomic_notifier_chain_register(&panic_notifier_list, &panic_block);

	return 0;
}

static int ioc3led_remove(struct ioc3_submodule *is, struct ioc3_driver_data *idd)
{
	if (ioc3 != idd)
		return 1;

	misc_deregister(&leds_dev);
	ioc3 = NULL;
	return 0;
}


/* entry/exit functions */
static struct ioc3_submodule ioc3led_submodule = {
	.name = "leds",
	.probe = ioc3led_probe,
	.remove = ioc3led_remove,
	.owner = THIS_MODULE,
};

static int __init leds_init(void)
{
	ioc3_register_submodule(&ioc3led_submodule);
	return 0;
}

static void __exit leds_exit (void)
{
	ioc3_unregister_submodule(&ioc3led_submodule);
}

MODULE_AUTHOR("Stanislaw Skowronek <skylark@linux-mips.org>");
MODULE_DESCRIPTION("SGI Octane (IP30) LEDS Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("R28");

module_init(leds_init);
module_exit(leds_exit);
