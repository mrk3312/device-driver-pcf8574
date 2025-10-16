#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include "pcf8574.h"

static const struct i2c_device_id pcf_idtable[] = {
    {"pcf8574", 1},
    { }
};

MODULE_DEVICE_TABLE(i2c, pcf_idtable);

static struct pcf8574_dev dev;

static int pcf_probe(struct i2c_client *client){
    dev.slave = client;
    i2c_set_clientdata(client, &dev.data);
    return 0;
}

static void pcf_remove(struct i2c_client *client){
    
}

static int pcf8574_open(struct inode *inode, struct file *filp){
    filp->private_data = &dev.data;
    
    return 0;
}

static struct file_operations pcf8574_fops = {
    .owner = THIS_MODULE,
    .open = pcf8574_open,
    //.write = pcf8574_write,
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
