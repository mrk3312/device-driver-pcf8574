#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include "pcf8574.h"

static const struct i2c_device_id pcf_idtable[] = {
    {"pcf8574", 1},
    { }
};

MODULE_DEVICE_TABLE(i2c, pcf_idtable);

static struct pcf8574_dev dev;

static int pcf8574_probe(struct i2c_client *client){
    dev.slave = client;
    i2c_set_clientdata(client, &dev.data);
    return 0;
}

static void pcf8574_remove(struct i2c_client *client){
    
}

static int pcf8574_open(struct inode *inode, struct file *filp){
    filp->private_data = &dev.data;
    
    return 0;
}

static ssize_t pcf8574_write(struct file *filp, const char __user *buf, ssize_t count, loff_t *f_pos){
    struct i2c_client *client = filp->private_data;
    struct pcf8574_private_data *data = i2c_get_clientdata(client);
    u16 word = 0;
    int retval = 0;
    
    if (count != 2){
        retval = -ENOMSG;
        goto err;
    }
    
    if (copy_from_user(&word, buf, sizeof(u16))){
        retval = -EFAULT;
        goto err;
    }

    u8 mask = word & 0xFF;

    if ((!(word & (1 << 15)) || (mask == 0 || (mask & (mask - 1)) != 0))){ // reject read request and make sure only 1 pin is requested for update
        retval = -EINVAL;
        goto err;
    }

    if (word & (1 << 14)){ // whether it should update p_state or p_dir
        
        if (word & (1 << 13)){ //whether pin should be turned on or off
            data->p_state |= (mask);
        }

        else{
            data->p_state &= ~(mask);
        }

        retval = i2c_master_send(client, &data->p_state, sizeof(data->p_state));
    }
    else{
        if (word & (1 << 13)){
            data->p_dir |= mask;
        }
        else{
            data->p_dir &= ~(mask);
        }
    }
    
    if (retval < 0){
        goto err;
    }
    retval = count;

err:
    return retval;
}

static struct file_operations pcf8574_fops = {
    .owner = THIS_MODULE,
    .open = pcf8574_open,
    .write = pcf8574_write,
    //.read = pcf8574_read,
    //.ioctl = pcf8574_ioctl,
    //.release = pcf8574_release
};

static int pcf8574_major = PCF8574_MAJOR;
static int pcf8574_minor = PCF8574_MINOR;

static int __init pcf8574_init(void){
    
    dev_t dev_n;
    
    int retval = 0;
    if (!pcf8574_major){
        retval = alloc_chrdev_region(&dev_n, FIRST_MINOR, DEVICE_COUNT, "pcf8574");
        pcf8574_major = MAJOR(dev_n);
    }
    else{
        dev_n = MKDEV(pcf8574_major, pcf8574_minor);
        retval = register_chrdev_region(dev_n, DEVICE_COUNT, "pcf8574");        
    }
    if (retval < 0){
        printk(KERN_WARNING "PCF8574 Device Driver coudln't get a major number!\n");
        goto err;
    }

    cdev_init(&dev.cdev, &pcf8574_fops);
    dev.cdev.owner = THIS_MODULE;

    retval = cdev_add(&dev.cdev, dev_n, DEVICE_COUNT);
    
    if (retval < 0){
        printk(KERN_WARNING "The kernel couldn't add the PCF8574 device driver as a char one\n");
        goto err;
    }
    dev.driver.driver.name = "pcf8574";
    dev.driver.driver.owner = THIS_MODULE;
    dev.driver.id_table = pcf_idtable;
    dev.driver.probe = pcf_probe;
    dev.driver.remove = pcf_remove;

    retval = i2c_add_driver(&dev.driver);

    if (retval < 0){
        printk(KERN_WARNING "PCF8574 couldn't be added to the Linux I2C Core\n");
        goto err;
    }
    printk(KERN_WARNING "Major is %d\nDriver is registered as Class Character\nPCF was added successfully to the i2c core subsystem\n", pcf8574_major);
err:
    return retval;
}

static void pcf8574_exit(void){
    dev_t dev_n = MKDEV(pcf8574_major, pcf8574_minor);
    unregister_chrdev_region(dev_n, DEVICE_COUNT);
    cdev_del(&dev.cdev);
}


module_init(pcf8574_init);
module_exit(pcf8574_exit);

MODULE_LICENSE("GPL");
