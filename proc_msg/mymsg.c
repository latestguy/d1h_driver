#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/export.h>
#include <linux/stat.h>
#include <linux/device.h>

#define MYLOG_BUF_LEN 1024

static struct proc_dir_entry *myentry;

static char mylog_buf[MYLOG_BUF_LEN];
static char tmp_buf[MYLOG_BUF_LEN];
static int mylog_w = 0;
static int mylog_r = 0;

static DECLARE_WAIT_QUEUE_HEAD(mymsg_waitq);

extern struct proc_dir_entry *proc_create(const char *name, umode_t mode,
				   struct proc_dir_entry *parent,
				   const struct file_operations *proc_fops);
extern void proc_remove(struct proc_dir_entry *de);

static int is_mylog_empty(void)
{
    return (mylog_r == mylog_w);
}

static int is_mylog_full(void)
{
    return ((mylog_w + 1) % MYLOG_BUF_LEN == mylog_r);
}

static void mylog_putc(char c)
{
    if (is_mylog_full())
    {
        //discard a byte
        mylog_r = (mylog_r + 1) % MYLOG_BUF_LEN;
    }
    
    mylog_buf[mylog_w] = c;
    mylog_w = (mylog_w + 1) % MYLOG_BUF_LEN;
}

static int mylog_getc(char *p)
{
    if (is_mylog_empty())
    {
        return 0;
    }

    *p = mylog_buf[mylog_r];
    mylog_r = (mylog_r + 1) % MYLOG_BUF_LEN;

    return 1;
}

int mylog_print(const char *fmt, ...)
{
	va_list args;
	int i;
    int j;

	va_start(args, fmt);
	i = vsnprintf(tmp_buf, MYLOG_BUF_LEN, fmt, args);
	va_end(args);

    for (j = 0; j < i; j++)
    {
        mylog_putc(tmp_buf[j]);
    }

    wake_up_interruptible(&mymsg_waitq);

	return i;
}
EXPORT_SYMBOL(mylog_print);

static ssize_t mymsg_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    int error = 0;
    int i = 0;
    char c;
    // printk("%s enter!\n", __func__);
    // if (copy_to_user(buf, mylog_buf, 10))
    //     return -EFAULT;

    if ((file->f_flags & O_NONBLOCK) && is_mylog_empty())
    {
        return -EAGAIN;
    }

    error = wait_event_interruptible(mymsg_waitq, !is_mylog_empty());

    /* copy to user */
    while (!error && (mylog_getc(&c)) && i < count)
    {
        error = copy_to_user(buf, &c, 1);
        buf++;
        i++;
    }

    if (!error)
        error = i;

    return error;
}

static struct file_operations proc_mymsg_operations = {
    .read = mymsg_read,
};


static ssize_t write_some(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    if (buf == NULL)
    {
        return size;
    }

    mylog_print("%s", buf);

    return size;
}

static DEVICE_ATTR(write_some, S_IRUSR | S_IWUSR | S_IRUSR, NULL, write_some);

static struct attribute *power_mcu_sysfs_attrs[] = {
    &dev_attr_write_some.attr,
	NULL
};

static struct attribute_group mymsg_attribute_group = {
	.attrs = power_mcu_sysfs_attrs,
};

static struct kobject *kobj;
static int mymsg_init(void)
{
    int ret;

    printk("%s enter!\n", __func__);

    kobj = kobject_create_and_add("mymsg_kobj", NULL);
    if (!kobj)
    {
        pr_err("Failed to create kobject!\n");
        return -ENOMEM;
    }

    ret = sysfs_create_group(kobj, &mymsg_attribute_group);
    if (ret)
    {
        pr_err("Failed to creata sysfs attribute!\n");
        kobject_put(kobj);
        return ret;
    }

	myentry = proc_create("mymsg", S_IRUSR, NULL, &proc_mymsg_operations);

    return 0;
}

static void mymsg_exit(void)
{
    printk("%s enter!\n", __func__);

    sysfs_remove_group(kobj, &mymsg_attribute_group);
    kobject_del(kobj);

    proc_remove(myentry);
}

module_init(mymsg_init);
module_exit(mymsg_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("latestguy");
MODULE_INFO(intree, "Y");