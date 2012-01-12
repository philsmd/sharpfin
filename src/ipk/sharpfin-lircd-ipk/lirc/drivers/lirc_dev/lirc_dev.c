/*
 * LIRC base driver
 *
 * (L) by Artur Lipowski <alipowski@interia.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: lirc_dev.c,v 1.56 2008/01/13 10:45:02 lirc Exp $
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 2, 18)
#error "**********************************************************"
#error " Sorry, this driver needs kernel version 2.2.18 or higher "
#error "**********************************************************"
#endif

#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/smp_lock.h>
#include <linux/completion.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
#include <asm/uaccess.h>
#include <asm/errno.h>
#else
#include <linux/uaccess.h>
#include <linux/errno.h>
#endif
#include <asm/semaphore.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#include <linux/wrapper.h>
#endif
#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
#include <linux/kthread.h>
#endif

#include "drivers/kcompat.h"

/* SysFS header */
#if defined(LIRC_HAVE_SYSFS)
#include <linux/device.h>
#endif

#include "drivers/lirc.h"
#include "lirc_dev.h"

static int debug;
#define dprintk(fmt, args...)					\
	do {							\
		if (debug)					\
			printk(KERN_DEBUG fmt, ## args);	\
	} while (0)

#define IRCTL_DEV_NAME    "BaseRemoteCtl"
#define SUCCESS           0
#define NOPLUG            -1
#define LOGHEAD           "lirc_dev (%s[%d]): "

struct irctl {
	struct lirc_plugin p;
	int attached;
	int open;

	struct semaphore buffer_sem;
	struct lirc_buffer *buf;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	int tpid;
	struct completion *t_notify;
	struct completion *t_notify2;
	int shutdown;
#else
	struct task_struct *task;
#endif
	long jiffies_to_wait;

#ifdef LIRC_HAVE_DEVFS_24
	devfs_handle_t devfs_handle;
#endif
};

DECLARE_MUTEX(plugin_lock);

static struct irctl irctls[MAX_IRCTL_DEVICES];
static struct file_operations fops;

/* Only used for sysfs but defined to void otherwise */
static lirc_class_t *lirc_class;

/*  helper function
 *  initializes the irctl structure
 */
static inline void init_irctl(struct irctl *ir)
{
	memset(&ir->p, 0, sizeof(struct lirc_plugin));
	sema_init(&ir->buffer_sem, 1);
	ir->p.minor = NOPLUG;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	ir->tpid = -1;
	ir->t_notify = NULL;
	ir->t_notify2 = NULL;
	ir->shutdown = 0;
#else
	ir->task = NULL;
#endif
	ir->jiffies_to_wait = 0;

	ir->open = 0;
	ir->attached = 0;
}

static void cleanup(struct irctl *ir)
{
	dprintk(LOGHEAD "cleaning up\n", ir->p.name, ir->p.minor);

#ifdef LIRC_HAVE_DEVFS_24
	devfs_unregister(ir->devfs_handle);
#endif
#ifdef LIRC_HAVE_DEVFS_26
	devfs_remove(DEV_LIRC "/%u", ir->p.minor);
#endif
	class_device_destroy(lirc_class, MKDEV(IRCTL_DEV_MAJOR, ir->p.minor));

	if (ir->buf != ir->p.rbuf) {
		lirc_buffer_free(ir->buf);
		kfree(ir->buf);
	}
	ir->buf = NULL;

	init_irctl(ir);
}

/*  helper function
 *  reads key codes from plugin and puts them into buffer
 *  buffer free space is checked and locking performed
 *  returns 0 on success
 */
static inline int add_to_buf(struct irctl *ir)
{
	if (lirc_buffer_full(ir->buf)) {
		dprintk(LOGHEAD "buffer overflow\n",
			ir->p.name, ir->p.minor);
		return -EOVERFLOW;
	}

	if (ir->p.add_to_buf) {
		int res = -ENODATA;
		int got_data = 0;

		/* service the device as long as it is returning
		 * data and we have space
		 */
		while (!lirc_buffer_full(ir->buf)) {
			res = ir->p.add_to_buf(ir->p.data, ir->buf);
			if (res == SUCCESS)
				got_data++;
			else
				break;
		}

		if (res == -ENODEV)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
			ir->shutdown = 1;
#else
			kthread_stop(ir->task);
#endif

		return (got_data ? SUCCESS : res);
	}

	return SUCCESS;
}

/* main function of the polling thread
 */
static int lirc_thread(void *irctl)
{
	struct irctl *ir = irctl;

	/* This thread doesn't need any user-level access,
	 * so get rid of all our resources
	 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	daemonize("lirc_dev");

	if (ir->t_notify != NULL)
		complete(ir->t_notify);
#endif

	dprintk(LOGHEAD "poll thread started\n", ir->p.name, ir->p.minor);

	do {
		if (ir->open) {
			if (ir->jiffies_to_wait) {
				set_current_state(TASK_INTERRUPTIBLE);
				schedule_timeout(ir->jiffies_to_wait);
			} else {
				interruptible_sleep_on(
					ir->p.get_queue(ir->p.data));
			}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
			if (ir->shutdown)
#else
			if (kthread_should_stop())
#endif
				break;
			if (!add_to_buf(ir))
				wake_up_interruptible(&ir->buf->wait_poll);
		} else {
			/* if device not opened so we can sleep half a second */
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(HZ/2);
		}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	} while (!ir->shutdown);

	if (ir->t_notify2 != NULL)
		wait_for_completion(ir->t_notify2);

	ir->tpid = -1;
	if (ir->t_notify != NULL)
		complete(ir->t_notify);
#else
	} while (!kthread_should_stop());
#endif

	dprintk(LOGHEAD "poll thread ended\n", ir->p.name, ir->p.minor);

	return 0;
}

int lirc_register_plugin(struct lirc_plugin *p)
{
	struct irctl *ir;
	int minor;
	int bytes_in_key;
	int err;
#ifdef LIRC_HAVE_DEVFS_24
	char name[16];
#endif
	DECLARE_COMPLETION(tn);

	if (!p) {
		printk(KERN_ERR "lirc_dev: lirc_register_plugin: "
		       "plugin pointer must be not NULL!\n");
		err = -EBADRQC;
		goto out;
	}

	if (MAX_IRCTL_DEVICES <= p->minor) {
		printk(KERN_ERR "lirc_dev: lirc_register_plugin: "
		       "\"minor\" must be between 0 and %d (%d)!\n",
		       MAX_IRCTL_DEVICES-1, p->minor);
		err = -EBADRQC;
		goto out;
	}

	if (1 > p->code_length || (BUFLEN * 8) < p->code_length) {
		printk(KERN_ERR "lirc_dev: lirc_register_plugin: "
		       "code length in bits for minor (%d) "
		       "must be less than %d!\n",
		       p->minor, BUFLEN * 8);
		err = -EBADRQC;
		goto out;
	}

	printk(KERN_INFO "lirc_dev: lirc_register_plugin: sample_rate: %d\n",
		p->sample_rate);
	if (p->sample_rate) {
		if (2 > p->sample_rate || HZ < p->sample_rate) {
			printk(KERN_ERR "lirc_dev: lirc_register_plugin: "
			       "sample_rate must be between 2 and %d!\n", HZ);
			err = -EBADRQC;
			goto out;
		}
		if (!p->add_to_buf) {
			printk(KERN_ERR "lirc_dev: lirc_register_plugin: "
			       "add_to_buf cannot be NULL when "
			       "sample_rate is set\n");
			err = -EBADRQC;
			goto out;
		}
	} else if (!(p->fops && p->fops->read)
		   && !p->get_queue && !p->rbuf) {
		printk(KERN_ERR "lirc_dev: lirc_register_plugin: "
		       "fops->read, get_queue and rbuf "
		       "cannot all be NULL!\n");
		err = -EBADRQC;
		goto out;
	} else if (!p->get_queue && !p->rbuf) {
		if (!(p->fops && p->fops->read && p->fops->poll)
		    || (!p->fops->ioctl && !p->ioctl)) {
			printk(KERN_ERR "lirc_dev: lirc_register_plugin: "
			       "neither read, poll nor ioctl can be NULL!\n");
			err = -EBADRQC;
			goto out;
		}
	}

	if (p->owner == NULL) {
		printk(KERN_ERR "lirc_dev: lirc_register_plugin: "
				    "no module owner registered\n");
		err = -EBADRQC;
		goto out;
	}

	down(&plugin_lock);

	minor = p->minor;

	if (0 > minor) {
		/* find first free slot for plugin */
		for (minor = 0; minor < MAX_IRCTL_DEVICES; minor++)
			if (irctls[minor].p.minor == NOPLUG)
				break;
		if (MAX_IRCTL_DEVICES == minor) {
			printk(KERN_ERR "lirc_dev: lirc_register_plugin: "
			       "no free slots for plugins!\n");
			err = -ENOMEM;
			goto out_lock;
		}
	} else if (irctls[minor].p.minor != NOPLUG) {
		printk(KERN_ERR "lirc_dev: lirc_register_plugin: "
		       "minor (%d) just registered!\n", minor);
		err = -EBUSY;
		goto out_lock;
	}

	ir = &irctls[minor];

	if (p->sample_rate) {
		ir->jiffies_to_wait = HZ / p->sample_rate;
	} else {
		/* it means - wait for external event in task queue */
		ir->jiffies_to_wait = 0;
	}

	/* some safety check 8-) */
	p->name[sizeof(p->name)-1] = '\0';

	bytes_in_key = p->code_length/8 + (p->code_length%8 ? 1 : 0);

	if (p->rbuf) {
		ir->buf = p->rbuf;
	} else {
		ir->buf = kmalloc(sizeof(struct lirc_buffer), GFP_KERNEL);
		if (!ir->buf) {
			err = -ENOMEM;
			goto out_lock;
		}
		if (lirc_buffer_init(ir->buf, bytes_in_key,
				     BUFLEN/bytes_in_key) != 0) {
			kfree(ir->buf);
			err = -ENOMEM;
			goto out_lock;
		}
	}

	if (p->features == 0)
		p->features = (p->code_length > 8) ?
			LIRC_CAN_REC_LIRCCODE : LIRC_CAN_REC_CODE;

	ir->p = *p;
	ir->p.minor = minor;

#if defined(LIRC_HAVE_DEVFS_24)
	sprintf(name, DEV_LIRC "/%d", ir->p.minor);
	ir->devfs_handle = devfs_register(NULL, name, DEVFS_FL_DEFAULT,
					  IRCTL_DEV_MAJOR, ir->p.minor,
					  S_IFCHR | S_IRUSR | S_IWUSR,
					  &fops, NULL);
#elif defined(LIRC_HAVE_DEVFS_26)
	devfs_mk_cdev(MKDEV(IRCTL_DEV_MAJOR, ir->p.minor),
			S_IFCHR|S_IRUSR|S_IWUSR,
			DEV_LIRC "/%u", ir->p.minor);
#endif
	(void) lirc_class_device_create(lirc_class, NULL,
					MKDEV(IRCTL_DEV_MAJOR, ir->p.minor),
					ir->p.dev, "lirc%u", ir->p.minor);

	if (p->sample_rate || p->get_queue) {
		/* try to fire up polling thread */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
		ir->t_notify = &tn;
		ir->tpid = kernel_thread(lirc_thread, (void *)ir, 0);
		if (ir->tpid < 0) {
#else
		ir->task = kthread_run(lirc_thread, (void *)ir, "lirc_dev");
		if (IS_ERR(ir->task)) {
#endif
			printk(KERN_ERR "lirc_dev: lirc_register_plugin: "
			       "cannot run poll thread for minor = %d\n",
			       p->minor);
			err = -ECHILD;
			goto out_sysfs;
		}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
		wait_for_completion(&tn);
		ir->t_notify = NULL;
#endif
	}
	ir->attached = 1;
	up(&plugin_lock);

/*
 * Recent kernels should handle this autmatically by increasing/decreasing
 * use count when a dependant module is loaded/unloaded.
 */
#ifndef KERNEL_2_5
	MOD_INC_USE_COUNT;
#endif
	dprintk("lirc_dev: plugin %s registered at minor number = %d\n",
		ir->p.name, ir->p.minor);
	p->minor = minor;
	return minor;

out_sysfs:
	class_device_destroy(lirc_class, MKDEV(IRCTL_DEV_MAJOR, ir->p.minor));
#ifdef LIRC_HAVE_DEVFS_24
	devfs_unregister(ir->devfs_handle);
#endif
#ifdef LIRC_HAVE_DEVFS_26
	devfs_remove(DEV_LIRC "/%i", ir->p.minor);
#endif
out_lock:
	up(&plugin_lock);
out:
	return err;
}
EXPORT_SYMBOL(lirc_register_plugin);

int lirc_unregister_plugin(int minor)
{
	struct irctl *ir;
	DECLARE_COMPLETION(tn);
	DECLARE_COMPLETION(tn2);

	if (minor < 0 || minor >= MAX_IRCTL_DEVICES) {
		printk(KERN_ERR "lirc_dev: lirc_unregister_plugin: "
		       "\"minor\" must be between 0 and %d!\n",
		       MAX_IRCTL_DEVICES-1);
		return -EBADRQC;
	}

	ir = &irctls[minor];

	down(&plugin_lock);

	if (ir->p.minor != minor) {
		printk(KERN_ERR "lirc_dev: lirc_unregister_plugin: "
		       "minor (%d) device not registered!", minor);
		up(&plugin_lock);
		return -ENOENT;
	}

	/* end up polling thread */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	if (ir->tpid >= 0) {
		ir->t_notify = &tn;
		ir->t_notify2 = &tn2;
		ir->shutdown = 1;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
		{
			struct task_struct *p;

			p = find_task_by_pid(ir->tpid);
			wake_up_process(p);
		}
#else
		/* 2.2.x does not export wake_up_process() */
		wake_up_interruptible(ir->p.get_queue(ir->p.data));
#endif
		complete(&tn2);
		wait_for_completion(&tn);
		ir->t_notify = NULL;
		ir->t_notify2 = NULL;
	}
#else /* kernel >= 2.6.23 */
	if (ir->task) {
		wake_up_process(ir->task);
		kthread_stop(ir->task);
	}
#endif

	dprintk("lirc_dev: plugin %s unregistered from minor number = %d\n",
		ir->p.name, ir->p.minor);

	ir->attached = 0;
	if (ir->open) {
		dprintk(LOGHEAD "releasing opened plugin\n",
			ir->p.name, ir->p.minor);
		wake_up_interruptible(&ir->buf->wait_poll);
		down(&ir->buffer_sem);
		ir->p.set_use_dec(ir->p.data);
		module_put(ir->p.owner);
		up(&ir->buffer_sem);
	} else
		cleanup(ir);
	up(&plugin_lock);

/*
 * Recent kernels should handle this autmatically by increasing/decreasing
 * use count when a dependant module is loaded/unloaded.
 */
#ifndef KERNEL_2_5
	MOD_DEC_USE_COUNT;
#endif

	return SUCCESS;
}
EXPORT_SYMBOL(lirc_unregister_plugin);

/*
 *
 */
static int irctl_open(struct inode *inode, struct file *file)
{
	struct irctl *ir;
	int retval;

	if (MINOR(inode->i_rdev) >= MAX_IRCTL_DEVICES) {
		dprintk("lirc_dev [%d]: open result = -ENODEV\n",
			MINOR(inode->i_rdev));
		return -ENODEV;
	}

	ir = &irctls[MINOR(inode->i_rdev)];

	dprintk(LOGHEAD "open called\n", ir->p.name, ir->p.minor);

	/* if the plugin has an open function use it instead */
	if (ir->p.fops && ir->p.fops->open)
		return ir->p.fops->open(inode, file);

	if (down_interruptible(&plugin_lock))
		return -ERESTARTSYS;

	if (ir->p.minor == NOPLUG) {
		up(&plugin_lock);
		dprintk(LOGHEAD "open result = -ENODEV\n",
			ir->p.name, ir->p.minor);
		return -ENODEV;
	}

	if (ir->open) {
		up(&plugin_lock);
		dprintk(LOGHEAD "open result = -EBUSY\n",
			ir->p.name, ir->p.minor);
		return -EBUSY;
	}

	/* there is no need for locking here because ir->open is 0
	 * and lirc_thread isn't using buffer
	 * plugins which use irq's should allocate them on set_use_inc,
	 * so there should be no problem with those either.
	 */
	ir->buf->head = ir->buf->tail;
	ir->buf->fill = 0;

	if (ir->p.owner != NULL && try_module_get(ir->p.owner)) {
		++ir->open;
		retval = ir->p.set_use_inc(ir->p.data);

		if (retval != SUCCESS) {
			module_put(ir->p.owner);
			--ir->open;
		}
	} else {
		if (ir->p.owner == NULL)
			dprintk(LOGHEAD "no module owner!!!\n",
				ir->p.name, ir->p.minor);

		retval = -ENODEV;
	}

	dprintk(LOGHEAD "open result = %d\n", ir->p.name, ir->p.minor, retval);
	up(&plugin_lock);

	return retval;
}

/*
 *
 */
static int irctl_close(struct inode *inode, struct file *file)
{
	struct irctl *ir = &irctls[MINOR(inode->i_rdev)];

	dprintk(LOGHEAD "close called\n", ir->p.name, ir->p.minor);

	/* if the plugin has a close function use it instead */
	if (ir->p.fops && ir->p.fops->release)
		return ir->p.fops->release(inode, file);

	if (down_interruptible(&plugin_lock))
		return -ERESTARTSYS;

	--ir->open;
	if (ir->attached) {
		ir->p.set_use_dec(ir->p.data);
		module_put(ir->p.owner);
	} else {
		cleanup(ir);
	}

	up(&plugin_lock);

	return SUCCESS;
}

/*
 *
 */
static unsigned int irctl_poll(struct file *file, poll_table *wait)
{
	struct irctl *ir = &irctls[MINOR(file->f_dentry->d_inode->i_rdev)];
	unsigned int ret;

	dprintk(LOGHEAD "poll called\n", ir->p.name, ir->p.minor);

	/* if the plugin has a poll function use it instead */
	if (ir->p.fops && ir->p.fops->poll)
		return ir->p.fops->poll(file, wait);

	down(&ir->buffer_sem);
	if (!ir->attached) {
		up(&ir->buffer_sem);
		return POLLERR;
	}

	poll_wait(file, &ir->buf->wait_poll, wait);

	dprintk(LOGHEAD "poll result = %s\n",
		ir->p.name, ir->p.minor,
		lirc_buffer_empty(ir->buf) ? "0" : "POLLIN|POLLRDNORM");

	ret = lirc_buffer_empty(ir->buf) ? 0 : (POLLIN|POLLRDNORM);

	up(&ir->buffer_sem);
	return ret;
}

/*
 *
 */
static int irctl_ioctl(struct inode *inode, struct file *file,
		       unsigned int cmd, unsigned long arg)
{
	unsigned long mode;
	int result;
	struct irctl *ir = &irctls[MINOR(inode->i_rdev)];

	dprintk(LOGHEAD "ioctl called (0x%x)\n",
		ir->p.name, ir->p.minor, cmd);

	/* if the plugin has a ioctl function use it instead */
	if (ir->p.fops && ir->p.fops->ioctl)
		return ir->p.fops->ioctl(inode, file, cmd, arg);

	if (ir->p.minor == NOPLUG || !ir->attached) {
		dprintk(LOGHEAD "ioctl result = -ENODEV\n",
			ir->p.name, ir->p.minor);
		return -ENODEV;
	}

	/* Give the plugin a chance to handle the ioctl */
	if (ir->p.ioctl) {
		result = ir->p.ioctl(inode, file, cmd, arg);
		if (result != -ENOIOCTLCMD)
			return result;
	}
	/* The plugin can't handle cmd */
	result = SUCCESS;

	switch (cmd) {
	case LIRC_GET_FEATURES:
		result = put_user(ir->p.features, (unsigned long *)arg);
		break;
	case LIRC_GET_REC_MODE:
		if (!(ir->p.features&LIRC_CAN_REC_MASK))
			return -ENOSYS;

		result = put_user(LIRC_REC2MODE
				  (ir->p.features&LIRC_CAN_REC_MASK),
				  (unsigned long *)arg);
		break;
	case LIRC_SET_REC_MODE:
		if (!(ir->p.features&LIRC_CAN_REC_MASK))
			return -ENOSYS;

		result = get_user(mode, (unsigned long *)arg);
		if (!result && !(LIRC_MODE2REC(mode) & ir->p.features))
			result = -EINVAL;
		/*
		 * FIXME: We should actually set the mode somehow but
		 * for now, lirc_serial doesn't support mode changing either
		 */
		break;
	case LIRC_GET_LENGTH:
		result = put_user((unsigned long)ir->p.code_length,
				  (unsigned long *)arg);
		break;
	default:
		result = -ENOIOCTLCMD;
	}

	dprintk(LOGHEAD "ioctl result = %d\n",
		ir->p.name, ir->p.minor, result);

	return result;
}

/*
 *
 */
static ssize_t irctl_read(struct file *file,
			  char *buffer,
			  size_t length,
			  loff_t *ppos)
{
	struct irctl *ir = &irctls[MINOR(file->f_dentry->d_inode->i_rdev)];
	unsigned char buf[ir->buf->chunk_size];
	int ret = 0, written = 0;
	DECLARE_WAITQUEUE(wait, current);

	dprintk(LOGHEAD "read called\n", ir->p.name, ir->p.minor);

	/* if the plugin has a specific read function use it instead */
	if (ir->p.fops && ir->p.fops->read)
		return ir->p.fops->read(file, buffer, length, ppos);

	if (down_interruptible(&ir->buffer_sem))
		return -ERESTARTSYS;
	if (!ir->attached) {
		up(&ir->buffer_sem);
		return -ENODEV;
	}

	if (length % ir->buf->chunk_size) {
		dprintk(LOGHEAD "read result = -EINVAL\n",
			ir->p.name, ir->p.minor);
		up(&ir->buffer_sem);
		return -EINVAL;
	}

	/*
	 * we add ourselves to the task queue before buffer check
	 * to avoid losing scan code (in case when queue is awaken somewhere
	 * beetwen while condition checking and scheduling)
	 */
	add_wait_queue(&ir->buf->wait_poll, &wait);
	set_current_state(TASK_INTERRUPTIBLE);

	/*
	 * while we did't provide 'length' bytes, device is opened in blocking
	 * mode and 'copy_to_user' is happy, wait for data.
	 */
	while (written < length && ret == 0) {
		if (lirc_buffer_empty(ir->buf)) {
			/* According to the read(2) man page, 'written' can be
			 * returned as less than 'length', instead of blocking
			 * again, returning -EWOULDBLOCK, or returning
			 * -ERESTARTSYS */
			if (written)
				break;
			if (file->f_flags & O_NONBLOCK) {
				ret = -EWOULDBLOCK;
				break;
			}
			if (signal_pending(current)) {
				ret = -ERESTARTSYS;
				break;
			}
			schedule();
			set_current_state(TASK_INTERRUPTIBLE);
			if (!ir->attached) {
				ret = -ENODEV;
				break;
			}
		} else {
			lirc_buffer_read_1(ir->buf, buf);
			ret = copy_to_user((void *)buffer+written, buf,
					   ir->buf->chunk_size);
			written += ir->buf->chunk_size;
		}
	}

	remove_wait_queue(&ir->buf->wait_poll, &wait);
	set_current_state(TASK_RUNNING);
	up(&ir->buffer_sem);

	dprintk(LOGHEAD "read result = %s (%d)\n",
		ir->p.name, ir->p.minor, ret ? "-EFAULT" : "OK", ret);

	return ret ? ret : written;
}


void *lirc_get_pdata(struct file *file)
{
	void *data = NULL;

	if (file && file->f_dentry && file->f_dentry->d_inode &&
	    file->f_dentry->d_inode->i_rdev) {
		struct irctl *ir;
		ir = &irctls[MINOR(file->f_dentry->d_inode->i_rdev)];
		data = ir->p.data;
	}

	return data;
}
EXPORT_SYMBOL(lirc_get_pdata);


static ssize_t irctl_write(struct file *file, const char *buffer,
			   size_t length, loff_t *ppos)
{
	struct irctl *ir = &irctls[MINOR(file->f_dentry->d_inode->i_rdev)];

	dprintk(LOGHEAD "write called\n", ir->p.name, ir->p.minor);

	/* if the plugin has a specific read function use it instead */
	if (ir->p.fops && ir->p.fops->write)
		return ir->p.fops->write(file, buffer, length, ppos);

	if (!ir->attached)
		return -ENODEV;

	return -EINVAL;
}


static struct file_operations fops = {
	.read		= irctl_read,
	.write		= irctl_write,
	.poll		= irctl_poll,
	.ioctl		= irctl_ioctl,
	.open		= irctl_open,
	.release	= irctl_close
};


static int lirc_dev_init(void)
{
	int i;

	for (i = 0; i < MAX_IRCTL_DEVICES; ++i)
		init_irctl(&irctls[i]);

	if (register_chrdev(IRCTL_DEV_MAJOR, IRCTL_DEV_NAME, &fops)) {
		printk(KERN_ERR "lirc_dev: register_chrdev failed\n");
		goto out;
	}

	lirc_class = class_create(THIS_MODULE, "lirc");
	if (IS_ERR(lirc_class)) {
		printk(KERN_ERR "lirc_dev: class_create failed\n");
		goto out_unregister;
	}

	printk(KERN_INFO "lirc_dev: IR Remote Control driver registered, "
	       "major %d \n", IRCTL_DEV_MAJOR);

	return SUCCESS;

out_unregister:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	if (unregister_chrdev(IRCTL_DEV_MAJOR, IRCTL_DEV_NAME))
		printk(KERN_ERR "lirc_dev: unregister_chrdev failed!\n");
#else
	/* unregister_chrdev returns void now */
	unregister_chrdev(IRCTL_DEV_MAJOR, IRCTL_DEV_NAME);
#endif
out:
	return -1;
}

/* ---------------------------------------------------------------------- */

/* For now dont try to use it as a static version !  */

#ifdef MODULE

/*
 *
 */
int init_module(void)
{
	return lirc_dev_init();
}

/*
 *
 */
void cleanup_module(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	int ret;

	ret = unregister_chrdev(IRCTL_DEV_MAJOR, IRCTL_DEV_NAME);
	class_destroy(lirc_class);

	if (ret)
		printk(KERN_ERR "lirc_dev: error in "
		       "module_unregister_chrdev: %d\n", ret);
	else
		dprintk("lirc_dev: module successfully unloaded\n");
#else
	/* unregister_chrdev returns void now */
	unregister_chrdev(IRCTL_DEV_MAJOR, IRCTL_DEV_NAME);
	class_destroy(lirc_class);
	dprintk("lirc_dev: module unloaded\n");
#endif
}

MODULE_DESCRIPTION("LIRC base driver module");
MODULE_AUTHOR("Artur Lipowski");
MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(IRCTL_DEV_MAJOR);

module_param(debug, bool, 0644);
MODULE_PARM_DESC(debug, "Enable debugging messages");

#endif /* MODULE */

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
