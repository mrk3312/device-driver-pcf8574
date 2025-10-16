#define DEVICE_COUNT 1
#define FIRST_MINOR 1
#define PCF8574_MAJOR 0
#define PCF8574_MINOR 0

struct pcf8574_private_data{
    char p_state; // pin state (ON/OFF)
    char p_dir; // pin direction (OUTPUT/INPUT)
};

struct pcf8574_dev {
    struct cdev cdev;
    struct i2c_driver driver;
    struct pcf8574_private_data data;
    struct i2c_client *slave;
};
