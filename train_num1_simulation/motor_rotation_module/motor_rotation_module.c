/* This module is used to control the
 * rotation direction of motor
 * we use PIN 20 and PIN 21 to control
 * */


#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/uaccess.h> //copy_to/from_user()
#include <linux/gpio.h>

#define GPIO_20 20
#define GPIO_21 21

#define INPUT_BUF_SIZE 8

dev_t dev = 0;
static struct class *dev_class;
static struct cdev etx_cdev;
static int __init etx_driver_init(void);
static void __exit etx_driver_exit(void);
/*************** Driver functions **********************/
static int      etx_open(struct inode *inode, struct file *file);
static int      etx_release(struct inode *inode, struct file *file);
static ssize_t  etx_read(struct file *filp,
                        char __user *buf, size_t len,loff_t * off);
static ssize_t  etx_write(struct file *filp,
                        const char *buf, size_t len, loff_t * off);
/******************************************************/
//File operation structure
static struct file_operations fops =
{
.owner      = THIS_MODULE,
.read       = etx_read,
.write      = etx_write,
.open       = etx_open,
.release    = etx_release,
};


static struct gpio motor_direction_GPIOs[] = {
    {
        .gpio  = GPIO_20,
        .flags = GPIOF_DIR_OUT | GPIOF_INIT_LOW,
        .label = "GPIO_20"
    },
    {
        .gpio  = GPIO_21,
        .flags = GPIOF_DIR_OUT | GPIOF_INIT_LOW,
        .label = "GPIO_21" 
    }
};

/*
** This function will be called when we open the Device file
*/
static int etx_open(struct inode *inode, struct file *file)
{
    pr_info("Device File Opened...!!!\n");
    return 0;
}
/*
** This function will be called when we close the Device file
*/
static int etx_release(struct inode *inode, struct file *file)
{
    pr_info("Device File Closed...!!!\n");
    return 0;
}

/*
** This function will be called when we read the Device file
*/
static ssize_t etx_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    printk("Read() called\n");


    return 0;
}

static ssize_t etx_write(struct file *filp,
                            const char __user *buf, size_t len, loff_t *off){
    uint8_t rec_buf[INPUT_BUF_SIZE] = {0};
    printk("etx_write() called\n");

    if(copy_from_user(rec_buf, buf, len) > 0){
        pr_err("ERROR: Not all the bytes have been copied from user\n");
    }

    printk("writer write rec_buf[0] = %d\n",rec_buf[0]-'0');

    
    
    
    if((rec_buf[0]-'0') & 0b01){
        printk("test");
        gpio_set_value(GPIO_20, 1);   
    }else{
        gpio_set_value(GPIO_20, 0);
    }
    if((rec_buf[0]-'0') & 0b10){
        gpio_set_value(GPIO_21, 1);
    }else{
        gpio_set_value(GPIO_21, 0);
    }
    
    return len;
}

static int __init etx_driver_init(void){
    printk("LED driver initialize\n");
    
    /*Allocating Major number*/
    if((alloc_chrdev_region(&dev, 0, 1, "etx_Dev")) <0){
        pr_err("Cannot allocate major number\n");
        goto r_unreg;
    }

    pr_info("Major = %d Minor = %d \n",MAJOR(dev), MINOR(dev));

    /*Creating cdev structure*/
    cdev_init(&etx_cdev,&fops);

    /*Adding character device to the system*/
    if((cdev_add(&etx_cdev,dev,1)) < 0){
        pr_err("Cannot add the device to the system\n");
        goto r_del;
    }
    /*Creating struct class*/
    if((dev_class = class_create(THIS_MODULE,"etx_class")) == NULL){
        pr_err("Cannot create the struct class\n");
        goto r_class;
    }

    /*Creating device*/
    if((device_create(dev_class,NULL,dev,NULL,"etx_device")) == NULL){
        pr_err( "Cannot create the Device \n");
        goto r_device;
    }

    /* Check GPIO is valid or not*/
    if(gpio_is_valid(GPIO_20) == false){
        pr_err("GPIO %d is not valid\n",GPIO_20);
        goto r_device;
    }

    if(gpio_is_valid(GPIO_21) == false){
        pr_err("GPIO %d is not valid\n",GPIO_21);
        goto r_device;
    }

    /* Requesting the GPIO*/
    int err = gpio_request_array(motor_direction_GPIOs, ARRAY_SIZE(motor_direction_GPIOs));
    if(err){
        pr_err("GPIO request denied\n");
        goto r_gpio;
    }

    //configure the GPIO as output
    gpio_direction_output(GPIO_20, 0);
    gpio_direction_output(GPIO_21, 0);

    gpio_export(GPIO_20, false);
    gpio_export(GPIO_21, false);

    pr_info("Device Driver Insert...Done!!!\n");
    return 0;

    r_gpio:
    gpio_free_array(motor_direction_GPIOs, ARRAY_SIZE(motor_direction_GPIOs));

    r_device:
    device_destroy(dev_class,dev);

    r_class:
    class_destroy(dev_class);

    r_del:
    cdev_del(&etx_cdev);

    r_unreg:
    unregister_chrdev_region(dev,1);
    return -1;
}

static void free_PIN(void){
    gpio_free_array(motor_direction_GPIOs, ARRAY_SIZE(motor_direction_GPIOs));

    printk("PIN free\n");
}

static void unexport_PIN(void){
    gpio_unexport(GPIO_20);
    gpio_unexport(GPIO_21);

    printk("PIN unexport\n");
}

static void __exit etx_driver_exit(void)
{
    printk("motor direction driver exit\n");
    unexport_PIN();
    free_PIN();
    device_destroy(dev_class,dev);
    class_destroy(dev_class);
    cdev_del(&etx_cdev);
    unregister_chrdev_region(dev, 1);
    pr_info("Device Driver Remove...Done!!\n");
}

module_init(etx_driver_init); //when module is called module_init() will automatically call etx_driver_init()
module_exit(etx_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("EmbeTronicX <embetronicx@gmail.com>");
MODULE_DESCRIPTION("A simple device driver - GPIO Driver");
MODULE_VERSION("1.32");