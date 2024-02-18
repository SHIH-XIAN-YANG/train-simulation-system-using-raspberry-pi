/***************************************************************************
 **
 * \file led_driver.c
 * \details Simple GPIO driver explanation
 * \author EmbeTronicX
 * \Tested with Linux raspberrypi 5.4.51-v7l+
 ******************************************************************************
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/uaccess.h> //copy_to/from_user()
#include <linux/gpio.h> //GPIO
#include <linux/delay.h>
//LED is connected to this GPIO
//LED = {14, 25, 16}

dev_t dev = 0;
static struct class *dev_class;
static struct cdev etx_cdev;
static int __init etx_driver_init(void);
static void __exit etx_driver_exit(void);
/*************** Driver functions **********************/
static int etx_open(struct inode *inode, struct file *file);
static int etx_release(struct inode *inode, struct file *file);
//static ssize_t etx_read(struct file *filp,
		//char __user *buf, size_t len,loff_t * off);
static ssize_t etx_write(struct file *filp,
		const char *buf, size_t len, loff_t * off);
/******************************************************/
//File operation structure
static struct file_operations fops =
{
	.owner = THIS_MODULE,
	//.read = etx_read,
	.write = etx_write,
	.open = etx_open,
	.release = etx_release,
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
 /*
static ssize_t etx_read(struct file *filp,
		char __user *buf, size_t len, loff_t *off)
{
	uint8_t gpio_state = 0;

	//reading GPIO value
	gpio_state = gpio_get_value(GPIO_21);

	//write to user
	len = 1;
	if( copy_to_user(buf, &gpio_state, len) > 0) {
		pr_err("ERROR: Not all the bytes have been copied to user\n");
	}

	pr_info("Read function : GPIO_21 = %d \n", gpio_state);

	return 0;
}
*/
/*
 ** This function will be called when we write the Device file
 */
static ssize_t etx_write(struct file *filp,
		const char __user *buf, size_t len, loff_t *off)
{
	char rec_buf[10] = {0};
	uint8_t pin, i;
	uint8_t LED[10] = {12, 8, 6, 5, 13, 2, 17, 23, 24, 18};

	if( copy_from_user( rec_buf, buf, len ) > 0) {
		pr_err("ERROR: Not all the bytes have been copied from user\n");
	}

	pr_info("write: %s\n", rec_buf);

	if (!rec_buf[0]) {
		for (i = 0; i < 10; i++)
			gpio_set_value(LED[i], 0);
	}
	
	pin = LED[rec_buf[0]-'0'];
	pr_info("pin = %d\n", pin);
	for (i = 0; i < 3; i++) {
		gpio_set_value(pin, 1);
		msleep_interruptible(200);
		//mdelay(200);
		gpio_set_value(pin, 0);
		//mdelay(200);
		msleep_interruptible(200);
	}

	return len;
}
/*
 ** Module Init function
 */
static int __init etx_driver_init(void)
{
	/*Allocating Major number*/
	int8_t i;
	uint8_t LED[10] = {12, 8, 6, 5, 13, 2, 17, 23, 24, 18};
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
	
	for (i = 0; i < 10; i++) {
		//Checking the GPIO is valid or not
		if (gpio_is_valid(LED[i]) == false) {
			pr_err("GPIO %d is not valid\n", LED[i]);
			goto r_device;
		}

		//Requesting the GPIO
		if(gpio_request(LED[i], "GPIO") < 0){
			pr_err("ERROR: GPIO %d request\n", 14);
			goto r_gpio;
		}

		//configure the GPIO as output
		gpio_direction_output(LED[i], 0);

		gpio_export(LED[i], false);
	}

	/* Using this call the GPIO 21 will be visible in /sys/class/gpio/
	 ** Now you can change the gpio values by using below commands also.
	 ** echo 1 > /sys/class/gpio/gpio21/value (turn ON the LED)
	 ** echo 0 > /sys/class/gpio/gpio21/value (turn OFF the LED)
	 ** cat /sys/class/gpio/gpio21/value (read the value LED)
	 **
	 ** the second argument prevents the direction from being changed.
	 */
	pr_info("Device Driver Insert...Done!!!\n");
	return 0;
r_gpio:
	for (i = 0; i < 10; i++)
		gpio_free(LED[i]);
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
/*
 ** Module exit function
 */
static void __exit etx_driver_exit(void)
{
	int i;
	uint8_t LED[10] = {12, 8, 6, 5, 13, 2, 17, 23, 24, 18};
	for (i = 0; i < 10; i++) {
		gpio_unexport(LED[i]);
		gpio_free(LED[i]);
	}
	device_destroy(dev_class,dev);
	class_destroy(dev_class);
	cdev_del(&etx_cdev);
	unregister_chrdev_region(dev, 1);
	pr_info("Device Driver Remove...Done!!\n");
}
module_init(etx_driver_init);
module_exit(etx_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("EmbeTronicX <embetronicx@gmail.com>");
MODULE_DESCRIPTION("A simple device driver - GPIO Driver");
MODULE_VERSION("1.32");
