/*
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Stefan Kalkowski
 * \date   2026-09-28
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <lx_emul.h>

pteval_t __default_kernel_pte_mask __read_mostly = ~0;


#include <linux/random.h>

void add_device_randomness(const void * buf,size_t len)
{
	lx_emul_trace(__func__);
}


#include <linux/sched/topology.h>

int arch_asym_cpu_priority(int cpu)
{
	lx_emul_trace_and_stop(__func__);
	return 0;
}


DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_sibling_map);
EXPORT_PER_CPU_SYMBOL(cpu_sibling_map);


#include <linux/percpu.h>

DEFINE_PER_CPU(unsigned long, cpu_scale);


#include <asm/pgtable.h>

pgprot_t pgprot_writecombine(pgprot_t prot)
{
    lx_emul_trace_and_stop(__func__);
}


/* kernel/sched/cpudeadline.h */
struct cpudl;
int cpudl_init(struct cpudl *cp)
{
	lx_emul_trace_and_stop(__func__);
	return -1;
}


void cpudl_cleanup(struct cpudl *cp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/interrupt.h>

DEFINE_STATIC_KEY_FALSE(force_irqthreads_key);


struct dl_bw;
void init_dl_bw(struct dl_bw *dl_b)
{
	lx_emul_trace_and_stop(__func__);
}


struct input_dev_poller;
extern void input_dev_poller_start(struct input_dev_poller * poller);
void input_dev_poller_start(struct input_dev_poller * poller)
{
	lx_emul_trace_and_stop(__func__);
}


extern void input_dev_poller_stop(struct input_dev_poller * poller);
void input_dev_poller_stop(struct input_dev_poller * poller)
{
	lx_emul_trace_and_stop(__func__);
}


const struct attribute_group input_poller_attribute_group;


#include <linux/kobject.h>

int kobject_uevent(struct kobject * kobj,enum kobject_action action)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <net/net_namespace.h>

void __init net_ns_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/fs.h>

int nonseekable_open(struct inode * inode,struct file * filp)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/string.h>

bool parse_option_str(const char * str,const char * option)
{
	lx_emul_trace(__func__);
	return false;
}


#include <linux/fs.h>

int register_chrdev_region(dev_t from,unsigned count,const char * name)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/syscore_ops.h>

void register_syscore_ops(struct syscore_ops * ops)
{
	lx_emul_trace(__func__);
}


struct irq_work;
extern void rto_push_irq_work_func(struct irq_work *work);
void rto_push_irq_work_func(struct irq_work *work)
{
	lx_emul_trace_and_stop(__func__);
}


/* kernel/sched/sched.h */
bool sched_smp_initialized = true;


#include <linux/skbuff.h>

void __init skb_init(void)
{
	lx_emul_trace(__func__);
}


extern void software_node_notify(struct device * dev);
void software_node_notify(struct device * dev)
{
	lx_emul_trace(__func__);
}


#include <linux/fs.h>

int stream_open(struct inode * inode,struct file * filp)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/swiotlb.h>

void swiotlb_dev_init(struct device * dev)
{
	lx_emul_trace(__func__);
}


#include <linux/timekeeper_internal.h>

void update_vsyscall(struct timekeeper * tk)
{
	lx_emul_trace(__func__);
}


const struct attribute_group *usb_device_groups[] = { };


extern int __init usb_devio_init(void);
int __init usb_devio_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


struct usb_driver usbfs_driver = {
	.name = "usbfs"
};


extern int usb_create_ep_devs(struct device * parent,struct usb_host_endpoint * endpoint,struct usb_device * udev);
int usb_create_ep_devs(struct device * parent,struct usb_host_endpoint * endpoint,struct usb_device * udev)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int usb_create_sysfs_dev_files(struct usb_device * udev);
int usb_create_sysfs_dev_files(struct usb_device * udev)
{
	lx_emul_trace(__func__);
	return 0;
}


extern void usb_create_sysfs_intf_files(struct usb_interface * intf);
void usb_create_sysfs_intf_files(struct usb_interface * intf)
{
	lx_emul_trace(__func__);
}


#include <linux/usb/hcd.h>

struct usb_hcd * usb_get_hcd(struct usb_hcd * hcd)
{
	lx_emul_trace(__func__);
	return hcd;
}


#include <linux/usb/hcd.h>

int usb_hcd_alloc_bandwidth(struct usb_device * udev,struct usb_host_config * new_config,struct usb_host_interface * cur_alt,struct usb_host_interface * new_alt)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/usb/hcd.h>

void usb_hcd_disable_endpoint(struct usb_device * udev,struct usb_host_endpoint * ep)
{
	lx_emul_trace(__func__);
}


#include <linux/usb/hcd.h>

void usb_hcd_flush_endpoint(struct usb_device * udev,struct usb_host_endpoint * ep)
{
	lx_emul_trace(__func__);
}


#include <linux/usb/hcd.h>

void usb_hcd_reset_endpoint(struct usb_device * udev,struct usb_host_endpoint * ep)
{
	lx_emul_trace(__func__);
}


struct usb_hub;
int usb_hub_create_port_device(struct usb_hub * hub,int port1)
{
	lx_emul_trace_and_stop(__func__);
}


void usb_hub_remove_port_device(struct usb_hub * hub,int port1)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb/hcd.h>

void __init usb_init_pool_max(void)
{
	lx_emul_trace(__func__);
}


const struct attribute_group *usb_interface_groups[] = { NULL };


extern int usb_major_init(void);
int usb_major_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


/* driver/usb/core/usb.h */

const struct class usbmisc_class = {
	.name = "usbmisc",
};


extern void usb_remove_sysfs_intf_files(struct usb_interface * intf);
void usb_remove_sysfs_intf_files(struct usb_interface * intf)
{
	lx_emul_trace(__func__);
}


#include <linux/ratelimit_types.h>

int ___ratelimit(struct ratelimit_state * rs,const char * func)
{
	lx_emul_trace(__func__);
	return 0;
}


extern void usb_remove_ep_devs(struct usb_host_endpoint * endpoint);
void usb_remove_ep_devs(struct usb_host_endpoint * endpoint)
{
	lx_emul_trace(__func__);
}


#include <linux/pid.h>

void put_pid(struct pid * pid)
{
	lx_emul_trace(__func__);
}


#if defined(CONFIG_OF)
#include <linux/usb/of.h>

bool usb_of_has_combined_node(struct usb_device * udev)
{
	lx_emul_trace(__func__);
	return true;
}
#endif


#ifdef CONFIG_SWIOTLB
#include <linux/swiotlb.h>

bool is_swiotlb_allocated(void)
{
	lx_emul_trace(__func__);
	return false;
}
#endif


extern void software_node_notify_remove(struct device * dev);
void software_node_notify_remove(struct device * dev)
{
	lx_emul_trace(__func__);
}


#include <linux/usb/hcd.h>

void usb_hcd_synchronize_unlinks(struct usb_device * udev)
{
	lx_emul_trace(__func__);
}


extern void usb_remove_sysfs_dev_files(struct usb_device * udev);
void usb_remove_sysfs_dev_files(struct usb_device * udev)
{
	lx_emul_trace(__func__);
}


#include <linux/usb/hcd.h>

void usb_put_hcd(struct usb_hcd * hcd)
{
	lx_emul_trace(__func__);
}
