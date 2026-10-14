#include <asm-generic/errno-base.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include "pcf8574.h"

/* COMUNICATION PROTOCOL BETWEEN USERLAND PROCESS AND THE KERNEL MODULE */
/* Userland processes send 1 byte, which describes what the requested operation is. */

/* NOTES */
/*
*   Bit 4 -> Operation bit
*   Bit 3 -> Level bit
*   Bit 2, 1, 0 -> Pin number
*/

/* A kernel module provides users with an option to extend the functionality of the kernel dynamically */
/* A kernel module is event-driven, it provides functionalities, which are called by the kernel wherever needed, therefore its structure differs significantly from standard software */

/* A structure that describes the PCF8574 Hardware */

static struct pcf8574_dev dev;

/* Major and minor number, used to give the module an identification, which is used by the kernel to call it's functionalities wherever needed. */

static int pcf8574_major = PCF8574_MAJOR;
static int pcf8574_minor = PCF8574_MINOR;

/* Table, which is used by the kernel to match the module with the correct Device Tree node */
static const struct i2c_device_id pcf8574_idtable[] = {
    {"pcf8574", 1},
    { }
};

/* Macro used to connect the table to a particular subsystem */
MODULE_DEVICE_TABLE(i2c, pcf8574_idtable);


/* Executed when the PCF8574 device is removed; Currently unused */
static void driver_pcf8574_remove(struct i2c_client *client){

}

/* Executed when a node in the Device Tree matches an entry from the pcf8574_idtable */
static int driver_pcf8574_probe(struct i2c_client *client){
    int retval = 0;

    dev.slave = client;
    dev.data.level = 0b11111111;
    dev.data.dir = 0b00000000;
    i2c_set_clientdata(client, &dev.data);
    printk(KERN_WARNING "PCF8574 is found in the device tree and probe() is executed by the i2c core!\n");

    return retval;
}

/* Executed when a userland process has called the open() system call */
/* Sets up fields the kernel file object, which will be used in other system calls */
static int char_pcf8574_open(struct inode *inode, struct file *filp){
    filp->private_data = dev.slave;
    return 0;
}

/* Executed when a userland process has called the write() system call */
/* If message is valid, it sends data to the PCF8574 device */
static ssize_t char_pcf8574_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos){
    
    u8 byte = 0;
    int retval = -ENOMSG;

    printk(KERN_WARNING "Entered write successfully!\n");

    struct i2c_client *client = filp->private_data;
    struct pcf8574_private_data *data = i2c_get_clientdata(client);
    
    if (count != 1){
        goto err;
    }
    
    if(copy_from_user(&byte, buf, sizeof(u8))){
        goto err;
    }

    u8 *ptr = (byte & (1 << 4) ? &data->level : &data->dir);


    u8 pin_n = byte & 0x7;
    
    if (byte & (1 << 3)){
        *ptr |= (1 << pin_n);
    }
    else{
        *ptr &= ~(1 << pin_n);
    }

    if (byte & (1 << 4)){
        retval = i2c_master_send(client, ptr, sizeof(u8));
        printk(KERN_WARNING "Sent %d", *ptr);
        if (retval < 0){
            goto err;
        }
    }

    retval = count;
err:
    return retval;
}

/* Executed when a userland process has called the read() system call */
/* If the request is valid, data from the kernel is retrieved to the userland process that requested it */
static ssize_t char_pcf8574_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos){
    int retval = -ENOMSG;

    printk(KERN_WARNING "Entered read successfully!\n");
    
    struct i2c_client *client = filp->private_data;
    struct pcf8574_private_data *data = i2c_get_clientdata(client);
    u8 kern_buf[2] = {data->level, data->dir};

    if (count != 2){
        goto err;
    }

    if(copy_to_user(buf, kern_buf, sizeof(u8) * count)){
        goto err;
    }
    
    retval = count;
err:
    return retval;
}

/* Executed when a userland process has called the ioctl() system call */
/* If the request is valid, special operations are executed */

/*  OPERATIONS  
*
*   0 -> OP_INIT 
*   1 -> OP_SET_ALL_PINS_LOW
*   2 -> OP_SET_ALL_PINS_HIGH
*
*/
static long int char_pcf8574_ioctl(struct file *filp, unsigned int operation, unsigned long arg){
    int retval = -EFAULT;

    struct i2c_client *client = filp->private_data;
    struct pcf8574_private_data *data = i2c_get_clientdata(client);
    
    switch(operation){
        case 0:{
            data->dir = 0b00000000;
            data->level = 0b00000000;
            break;
        }
        case 1:{
            data->level = 0b00000000;
            break;
        }
        case 2:{
            data->level = 0b11111111;
            break;
        }

        default:{
            goto err;
        }

    }

    if(i2c_master_send(client, &data->level, sizeof(u8)) < 0){
        goto err;
    }

    retval = 0;

err:
    return retval;
}

/* Executed when a userland process has called the close() system call, but only if there is only 1 file instance left */
static int char_pcf8574_release(struct inode *inode, struct file *filp){
    return 0;
}

/* Tagged structure initialization, used to connect the system calls to the correct function inside the kernel module */
static struct file_operations pcf8574_fops = {
    .owner = THIS_MODULE,
    .open = char_pcf8574_open,
    .write = char_pcf8574_write,
    .read = char_pcf8574_read,
    .unlocked_ioctl = char_pcf8574_ioctl,
    .release = char_pcf8574_release
};

/* Tagged structure initialization, used by the kernel to identify to which device can the module communicate */
static struct i2c_driver i2c_driver = {
    .driver = {
        .name = "pcf8574",
        .owner = THIS_MODULE,
    },
    .id_table = pcf8574_idtable,
    .probe = driver_pcf8574_probe,
    .remove = driver_pcf8574_remove
};

/* The first function called by the kernel when the module is inserted; destroyed after initialization is successful */
static int __init pcf8574_init(void){
    
    dev_t dev_n;
    
    int retval = 0;
    if (!pcf8574_major){
        retval = alloc_chrdev_region(&dev_n, pcf8574_minor, DEVICE_COUNT, "pcf8574");
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
    
    dev.driver = &i2c_driver;

    retval = i2c_add_driver(dev.driver);

    if (retval < 0){
        printk(KERN_WARNING "PCF8574 couldn't be added to the Linux I2C Core\n");
        goto err;
    }

    printk(KERN_WARNING "Major is %d\nDriver is registered as Class Character\nPCF8574 was added successfully to the i2c core subsystem\n", pcf8574_major);
    
err:
    return retval;
}

/* Executed by the kernel right after rmmod request; used to free up used resources */
static void pcf8574_exit(void){

    dev_t dev_n = MKDEV(pcf8574_major, pcf8574_minor);
    unregister_chrdev_region(dev_n, DEVICE_COUNT);
    cdev_del(&dev.cdev);

    i2c_del_driver(dev.driver);
    printk(KERN_WARNING "PCF8574 I2C Driver has been removed!");
}

/* Macros used to tell the kernel which functions have to be called during insmod and rmmod */
module_init(pcf8574_init);
module_exit(pcf8574_exit);

/* Macros used to tell the kernel the license of the module */
MODULE_LICENSE("GPL");
