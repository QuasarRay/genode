/*
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Automatically generated file - do no edit
 * \date   2026-09-30
 */

#include <lx_emul.h>


#include <linux/sched.h>

void ___migrate_enable(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/cpumask.h>

struct cpumask __cpu_active_mask;


#include <linux/dma-mapping.h>

bool __dma_need_sync(struct device * dev,dma_addr_t dma_addr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

void __dma_sync_sg_for_cpu(struct device * dev,struct scatterlist * sg,int nelems,enum dma_data_direction dir)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

void __dma_sync_sg_for_device(struct device * dev,struct scatterlist * sg,int nelems,enum dma_data_direction dir)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

void __dma_sync_single_for_cpu(struct device * dev,dma_addr_t addr,size_t size,enum dma_data_direction dir)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

void __dma_sync_single_for_device(struct device * dev,dma_addr_t addr,size_t size,enum dma_data_direction dir)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

void __printk_deferred_enter(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

void __printk_deferred_exit(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

int __printk_ratelimit(const char * func)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/scatterlist.h>

bool __sg_page_iter_dma_next(struct sg_dma_page_iter * dma_iter)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/scatterlist.h>

bool __sg_page_iter_next(struct sg_page_iter * piter)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/scatterlist.h>

void __sg_page_iter_start(struct sg_page_iter * piter,struct scatterlist * sglist,unsigned int nents,unsigned long pgoffset)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mmap_lock.h>

void __vma_start_write(struct vm_area_struct * vma,unsigned int mm_lock_seq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

int _printk_deferred(const char * fmt,...)
{
	lx_emul_trace_and_stop(__func__);
}


extern void ack_bad_irq(unsigned int irq);
void ack_bad_irq(unsigned int irq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kobject.h>

int add_uevent_var(struct kobj_uevent_env * env,const char * format,...)
{
	lx_emul_trace_and_stop(__func__);
}


extern unsigned long arch_scale_cpu_capacity(int cpu);
unsigned long arch_scale_cpu_capacity(int cpu)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/async.h>

async_cookie_t async_schedule_node(async_func_t func,void * data,int node)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/async.h>

void async_synchronize_full(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/bitmap-str.h>

int bitmap_parselist(const char * buf,unsigned long * maskp,int nmaskbits)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/bitrev.h>

u8 const byte_rev_table[256] = {};


#include <linux/arch_topology.h>

const struct cpumask * cpu_clustergroup_mask(int cpu)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/cpumask.h>

unsigned int cpumask_any_and_distribute(const struct cpumask * src1p,const struct cpumask * src2p)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/topology.h>

bool cpus_share_cache(int this_cpu,int that_cpu)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/context_tracking_irq.h>

noinstr void ct_irq_enter(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/context_tracking_irq.h>

noinstr void ct_irq_exit(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

void * dma_alloc_attrs(struct device * dev,size_t size,dma_addr_t * dma_handle,gfp_t flag,unsigned long attrs)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

struct sg_table * dma_alloc_noncontiguous(struct device * dev,size_t size,enum dma_data_direction dir,gfp_t gfp,unsigned long attrs)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

struct page * dma_alloc_pages(struct device * dev,size_t size,dma_addr_t * dma_handle,enum dma_data_direction dir,gfp_t gfp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

bool dma_can_mmap(struct device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

void dma_free_attrs(struct device * dev,size_t size,void * cpu_addr,dma_addr_t dma_handle,unsigned long attrs)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

void dma_free_noncontiguous(struct device * dev,size_t size,struct sg_table * sgt,enum dma_data_direction dir)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

void dma_free_pages(struct device * dev,size_t size,struct page * page,dma_addr_t dma_handle,enum dma_data_direction dir)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

dma_addr_t dma_map_page_attrs(struct device * dev,struct page * page,size_t offset,size_t size,enum dma_data_direction dir,unsigned long attrs)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

int dma_map_sgtable(struct device * dev,struct sg_table * sgt,enum dma_data_direction dir,unsigned long attrs)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

int dma_mmap_attrs(struct device * dev,struct vm_area_struct * vma,void * cpu_addr,dma_addr_t dma_addr,size_t size,unsigned long attrs)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

int dma_mmap_noncontiguous(struct device * dev,struct vm_area_struct * vma,size_t size,struct sg_table * sgt)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

int dma_mmap_pages(struct device * dev,struct vm_area_struct * vma,size_t size,struct page * page)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

void dma_unmap_page_attrs(struct device * dev,dma_addr_t addr,size_t size,enum dma_data_direction dir,unsigned long attrs)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

void dma_unmap_sg_attrs(struct device * dev,struct scatterlist * sg,int nents,enum dma_data_direction dir,unsigned long attrs)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

void * dma_vmap_noncontiguous(struct device * dev,size_t size,struct sg_table * sgt)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

void dma_vunmap_noncontiguous(struct device * dev,void * vaddr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mmdebug.h>

void dump_page(const struct page * page,const char * reason)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

asmlinkage __visible void dump_stack(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

asmlinkage __visible void dump_stack_lvl(const char * log_lvl)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/file.h>

struct fd fdget(unsigned int fd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rcuwait.h>

void finish_rcuwait(struct rcuwait * w)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/file.h>

void fput(struct file * file)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/genalloc.h>

void * gen_pool_dma_alloc_align(struct gen_pool * pool,size_t size,dma_addr_t * dma,int align)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/genalloc.h>

void gen_pool_free_owner(struct gen_pool * pool,unsigned long addr,size_t size,void ** owner)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/string.h>

int get_option(char ** str,int * pint)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/gfp.h>

bool gfp_pfmemalloc_allowed(gfp_t gfp_mask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uuid.h>

const u8 guid_index[16] = {};


#include <linux/init.h>

bool initcall_debug;


extern int input_bits_to_string(char * buf,int buf_size,unsigned long bits,bool skip_empty);
int input_bits_to_string(char * buf,int buf_size,unsigned long bits,bool skip_empty)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/input.h>

void input_ff_destroy(struct input_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/input/mt.h>

void input_mt_destroy_slots(struct input_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


extern void input_mt_release_slots(struct input_dev * dev);
void input_mt_release_slots(struct input_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

void __sched io_schedule(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

void io_schedule_finish(int token)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

int io_schedule_prepare(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

long __sched io_schedule_timeout(long timeout)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/interrupt.h>

int irq_can_set_affinity(unsigned int irq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/interrupt.h>

int irq_set_affinity(unsigned int irq,const struct cpumask * cpumask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irq_work.h>

bool irq_work_queue(struct irq_work * work)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irq_work.h>

bool irq_work_queue_on(struct irq_work * work,int cpu)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irq_work.h>

void irq_work_tick(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kobject.h>

int kobject_synth_uevent(struct kobject * kobj,const char * buf,size_t count)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kobject.h>

int kobject_uevent_env(struct kobject * kobj,enum kobject_action action,char * envp_ext[])
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/delay.h>

unsigned long loops_per_jiffy;


#include <linux/fs.h>

loff_t noop_llseek(struct file * file,loff_t offset,int whence)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/panic.h>

void panic(const char * fmt,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/moduleparam.h>

void param_free_charp(void * arg)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/moduleparam.h>

int param_get_charp(char * buffer,const struct kernel_param * kp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/moduleparam.h>

const struct kernel_param_ops param_ops_bool;


#include <linux/moduleparam.h>

const struct kernel_param_ops param_ops_charp;


#include <linux/moduleparam.h>

const struct kernel_param_ops param_ops_int;


#include <linux/moduleparam.h>

int param_set_charp(const char * val,const struct kernel_param * kp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/moduleparam.h>

int param_set_copystring(const char * val,const struct kernel_param * kp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pid.h>

pid_t pid_vnr(struct pid * pid)
{
	lx_emul_trace_and_stop(__func__);
}


extern void raw_spin_rq_lock_nested(struct rq * rq,int subclass);
void raw_spin_rq_lock_nested(struct rq * rq,int subclass)
{
	lx_emul_trace_and_stop(__func__);
}


extern void raw_spin_rq_unlock(struct rq * rq);
void raw_spin_rq_unlock(struct rq * rq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/refcount.h>

void refcount_warn_saturate(refcount_t * r,enum refcount_saturation_type t)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

int remap_pfn_range(struct vm_area_struct * vma,unsigned long addr,unsigned long pfn,unsigned long size,pgprot_t prot)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/vmalloc.h>

int remap_vmalloc_range(struct vm_area_struct * vma,void * addr,unsigned long pgoff)
{
	lx_emul_trace_and_stop(__func__);
}


extern int set_memory_wb(unsigned long addr,int numpages);
int set_memory_wb(unsigned long addr,int numpages)
{
	lx_emul_trace_and_stop(__func__);
}


extern int set_memory_wc(unsigned long addr,int numpages);
int set_memory_wc(unsigned long addr,int numpages)
{
	lx_emul_trace_and_stop(__func__);
}


extern void set_rq_offline(struct rq * rq);
void set_rq_offline(struct rq * rq)
{
	lx_emul_trace_and_stop(__func__);
}


extern void set_rq_online(struct rq * rq);
void set_rq_online(struct rq * rq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/smp.h>

unsigned int setup_max_cpus;


#include <linux/scatterlist.h>

int sg_alloc_table_from_pages_segment(struct sg_table * sgt,struct page ** pages,unsigned int n_pages,unsigned int offset,unsigned long size,unsigned int max_segment,gfp_t gfp_mask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/scatterlist.h>

void sg_free_table(struct sg_table * table)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/smp.h>

void smp_call_function_many(const struct cpumask * mask,smp_call_func_t func,void * info,bool wait)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/smp.h>

int smp_call_function_single(int cpu,smp_call_func_t func,void * info,int wait)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/smp.h>

int smp_call_function_single_async(int cpu,call_single_data_t * csd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/jump_label.h>

bool static_key_initialized;


#include <linux/hardirq.h>

void synchronize_irq(unsigned int irq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rcupdate.h>

void synchronize_rcu(void)
{
	lx_emul_trace_and_stop(__func__);
}


extern void update_group_capacity(struct sched_domain * sd,int cpu);
void update_group_capacity(struct sched_domain * sd,int cpu)
{
	lx_emul_trace_and_stop(__func__);
}


extern void usb_devio_cleanup(void);
void usb_devio_cleanup(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb.h>

struct mutex usb_dynids_lock;


#include <linux/usb.h>

int usb_free_streams(struct usb_interface * interface,struct usb_host_endpoint ** eps,unsigned int num_eps,gfp_t mem_flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb/hcd.h>

int usb_hcd_find_raw_port_number(struct usb_hcd * hcd,int port1)
{
	lx_emul_trace_and_stop(__func__);
}


extern void usb_major_cleanup(void);
void usb_major_cleanup(void)
{
	lx_emul_trace_and_stop(__func__);
}


extern int usb_phy_roothub_notify_connect(struct usb_phy_roothub * phy_roothub,int port);
int usb_phy_roothub_notify_connect(struct usb_phy_roothub * phy_roothub,int port)
{
	lx_emul_trace_and_stop(__func__);
}


extern int usb_phy_roothub_notify_disconnect(struct usb_phy_roothub * phy_roothub,int port);
int usb_phy_roothub_notify_disconnect(struct usb_phy_roothub * phy_roothub,int port)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb/ch9.h>

const char * usb_speed_string(enum usb_device_speed speed)
{
	lx_emul_trace_and_stop(__func__);
}


extern int usb_update_wireless_status_attr(struct usb_interface * intf);
int usb_update_wireless_status_attr(struct usb_interface * intf)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uuid.h>

const u8 uuid_index[16] = {};


#include <linux/mm.h>

pgprot_t vm_get_page_prot(vm_flags_t vm_flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

int vm_map_pages(struct vm_area_struct * vma,struct page ** pages,unsigned long num)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

struct page * vmalloc_to_page(const void * vmalloc_addr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/vmalloc.h>

void * vmap(struct page ** pages,unsigned int count,unsigned long flags,pgprot_t prot)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/vmalloc.h>

void vunmap(const void * addr)
{
	lx_emul_trace_and_stop(__func__);
}

