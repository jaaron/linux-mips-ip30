/*
 * SGI IOC3 PS/2 controller driver for linux
 *
 * Copyright (C) 2005 Stanislaw Skowronek <skylark@linux-mips.org>
 *               2009 Johannes Dickgreber <tanzy@gmx.de>
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/pci.h>
#include <linux/serio.h>

#include <linux/ioc3.h>

struct ioc3kbd_data {
	struct ioc3_driver_data *idd;
	struct serio *kbd, *aux;
};

static int ioc3kbd_write(struct serio *dev, unsigned char val)
{
	struct ioc3kbd_data *d = (struct ioc3kbd_data *)(dev->port_data);
	struct ioc3_driver_data *idd = d->idd;
	unsigned mask;
	unsigned long timeout = 0;

	mask = (dev == d->aux) ? KM_CSR_M_WRT_PEND : KM_CSR_K_WRT_PEND;
	while ((readl(&idd->vma->km_csr) & mask) && (timeout < 1000)) {
		udelay(100);
		timeout++;
	}

	if (dev == d->aux)
		writel(((unsigned)val) & 0x000000ff, &idd->vma->m_wd);
	else
		writel(((unsigned)val) & 0x000000ff, &idd->vma->k_wd);

	if (timeout >= 1000)
		return -1;
	return 0;
}

static int ioc3kbd_intr(struct ioc3_submodule *is,
			struct ioc3_driver_data *idd, unsigned int irq)
{
	struct ioc3kbd_data *d = (struct ioc3kbd_data *)(idd->data[is->id]);
	unsigned int data_k, data_m;

	ioc3_ack(is, idd, irq);
	data_k = readl(&idd->vma->k_rd);
	data_m = readl(&idd->vma->m_rd);

	if (data_k & KM_RD_VALID_0)
		serio_interrupt(d->kbd,
		(data_k >> KM_RD_DATA_0_SHIFT) & 0xff, 0);
	if (data_k & KM_RD_VALID_1)
		serio_interrupt(d->kbd,
		(data_k >> KM_RD_DATA_1_SHIFT) & 0xff, 0);
	if (data_k & KM_RD_VALID_2)
		serio_interrupt(d->kbd,
		(data_k >> KM_RD_DATA_2_SHIFT) & 0xff, 0);
	if (data_m & KM_RD_VALID_0)
		serio_interrupt(d->aux,
		(data_m >> KM_RD_DATA_0_SHIFT) & 0xff, 0);
	if (data_m & KM_RD_VALID_1)
		serio_interrupt(d->aux,
		(data_m >> KM_RD_DATA_1_SHIFT) & 0xff, 0);
	if (data_m & KM_RD_VALID_2)
		serio_interrupt(d->aux,
		(data_m >> KM_RD_DATA_2_SHIFT) & 0xff, 0);

	return 0;
}

static int ioc3kbd_open(struct serio *dev)
{
	return 0;
}

static void ioc3kbd_close(struct serio *dev)
{
	/* Empty */
}

static struct ioc3kbd_data __devinit *ioc3kbd_allocate_port(int idx,
						struct ioc3_driver_data *idd)
{
	struct serio *sk, *sa;
	struct ioc3kbd_data *d;

	sk = kzalloc(sizeof(struct serio), GFP_KERNEL);
	sa = kzalloc(sizeof(struct serio), GFP_KERNEL);
	d = kzalloc(sizeof(struct ioc3kbd_data), GFP_KERNEL);

	if (sk && sa && d) {
		sk->id.type = SERIO_8042;
		sk->write = ioc3kbd_write;
		sk->open = ioc3kbd_open;
		sk->close = ioc3kbd_close;
		snprintf(sk->name, sizeof(sk->name), "IOC3 keyboard %d", idx);
		snprintf(sk->phys, sizeof(sk->phys), "ioc3/serio%dkbd", idx);
		sk->port_data = d;
		sk->dev.parent = &(idd->pdev->dev);

		sa->id.type = SERIO_8042;
		sa->write = ioc3kbd_write;
		sa->open = ioc3kbd_open;
		sa->close = ioc3kbd_close;
		snprintf(sa->name, sizeof(sa->name), "IOC3 auxiliary %d", idx);
		snprintf(sa->phys, sizeof(sa->phys), "ioc3/serio%daux", idx);
		sa->port_data = d;
		sa->dev.parent = &(idd->pdev->dev);

		d->idd = idd;
		d->kbd = sk;
		d->aux = sa;
		return d;
	}
	kfree(sk);
	kfree(sa);
	kfree(d);

	return NULL;
}

static int __devinit ioc3kbd_probe(struct ioc3_submodule *is,
				struct ioc3_driver_data *idd)
{
	struct ioc3kbd_data *d;

	if ((idd->class != IOC3_CLASS_BASE_IP30) &&
	    (idd->class != IOC3_CLASS_CADDUO))
		return 1;

	d = ioc3kbd_allocate_port(idd->id, idd);
	if (!d)
		return 1;

	idd->data[is->id] = d;

	serio_register_port(d->kbd);
	serio_register_port(d->aux);

	ioc3_enable(is, idd, is->irq_mask);

	return 0;
}

static int __devexit ioc3kbd_remove(struct ioc3_submodule *is,
				struct ioc3_driver_data *idd)
{
	struct ioc3kbd_data *d = (struct ioc3kbd_data *)(idd->data[is->id]);

	ioc3_disable(is, idd, is->irq_mask);
	serio_unregister_port(d->kbd);
	serio_unregister_port(d->aux);
	kfree(d->kbd);
	kfree(d->aux);
	kfree(d);
	idd->data[is->id] = NULL;

	return 0;
}

static struct ioc3_submodule ioc3kbd_driver = {
	.name = "serio",
	.probe = ioc3kbd_probe,
	.remove = ioc3kbd_remove,
	.irq_mask = SIO_IR_KBD_INT,
	.reset_mask = 1,
	.intr = ioc3kbd_intr,
	.owner = THIS_MODULE,
};

static int __init ioc3kbd_init(void)
{
	return ioc3_register_submodule(&ioc3kbd_driver);
}

static void __exit ioc3kbd_exit(void)
{
	ioc3_unregister_submodule(&ioc3kbd_driver);
}

module_init(ioc3kbd_init);
module_exit(ioc3kbd_exit);

MODULE_AUTHOR("Stanislaw Skowronek <skylark@linux-mips.org>");
MODULE_DESCRIPTION("SGI IOC3 serio driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("R27");

