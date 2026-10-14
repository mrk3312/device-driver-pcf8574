#ifndef PCF8574_H
#define PCF8574_H

#define DEVICE_COUNT 1
#define PCF8574_MAJOR 0
#define PCF8574_MINOR 0

/* Describes the current state of each pin in the PCF8574 Hardware */
/* Each bit is a pin */
struct pcf8574_private_data{
    char level; /* Level (1 - ON; 0 - OFF) */
    char dir;   /* Direction (1 - INPUT; 0 - OUTPUT)*/
};

struct pcf8574_dev {
    struct cdev cdev;                   /* the class of the device driver; provides a way for userland processes to communicate with the kernel module */
    struct i2c_driver *driver;          /* used by the kernel to match the device tree node to the correct kernel module */
    struct pcf8574_private_data data;   /* used by the kernel to match the device tree node to the correct kernel module */
    struct i2c_client *slave;           /* pointer to the i2c_client structure, which provides the required data for i2c master-slave communication */
};

#endif