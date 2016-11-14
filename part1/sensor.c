/*
Part 1:
To develop a loadable kernel module which initiates two instances of HC-SR04 sensors
(named as HCSR_1 and HCSR_2) and allows the sensors being accessed as device files in
user space.
*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/math64.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <asm/gpio.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include "ioctl_private.h"

#define ONE_SHOT 1
#define PERIODIC 2

typedef struct sensor_device {
  struct miscdevice misc_device;
  struct list_head next;
  struct task_struct *SensorThread;

  int ongoing_sampling, edge_flag,echo_irq, op_mode, sampling_frequency;
  unsigned long long  distance[5], start_time, end_time, pulse_width;
  int read_index,write_index,index,corner_index;

  int echo_pin,echo_dir,echo_mux;
  int trig_pin,trig_dir,trig_mux;
  } HCSR04;

HCSR04 hc1,hc2;
unsigned long long pulse_diff=0;
static int device_count = 0;
static LIST_HEAD(sensor_devices);

/*@get_sensor_device: Returns the sensor object from the linked list*/
HCSR04 *get_sensor_device (int minor) {
  HCSR04  *hcsr_dev;
  list_for_each_entry(hcsr_dev, &sensor_devices, next) {
    if (hcsr_dev->misc_device.minor == minor) {
        return hcsr_dev;
    }
  }
return NULL;
}

/*@calculate_timestamp: Returns the current timestamp when requested*/
static __inline__ unsigned long long calculate_timestamp(void)
{
    uint32_t low, high;
    __asm__ __volatile__ ("rdtsc" : "=a" (low), "=d" (high));
    return ((uint64_t)(low)| (uint64_t)(high) << 32);
}


/*@trigger_signal: Returns the current timestamp when requested*/
static int trigger_signal(void *data)
{
  HCSR04 *hcsr_dev = (HCSR04 *)data;
  hcsr_dev->ongoing_sampling = 1;
  while(!kthread_should_stop())
  {
    printk("Triggering pulse... %llu \n ", hcsr_dev->pulse_width);
    gpio_set_value_cansleep(hcsr_dev->trig_pin,1);
    udelay(10);
    gpio_set_value_cansleep(hcsr_dev->trig_pin,0);
    msleep(hcsr_dev->sampling_frequency);
  }
  return 0;
}

/*@echo_handler: Registered interrupt to handle the echo response.
 * It gets the timestamp & calculates the pulse width to store it
 * in circular FIFO buffer
 */
static irqreturn_t echo_handler(int irq, void *dev_id )
{
    HCSR04 *hcsr_dev=dev_id;
    if(hcsr_dev->edge_flag == 0) {

    hcsr_dev->edge_flag = 1;
    irq_set_irq_type(irq, IRQ_TYPE_EDGE_FALLING);
    hcsr_dev->start_time = calculate_timestamp();

  } else {
    hcsr_dev->edge_flag = 0;
    irq_set_irq_type(irq, IRQ_TYPE_EDGE_RISING);
    hcsr_dev->end_time = calculate_timestamp();
    hcsr_dev->ongoing_sampling = 0;

    pulse_diff = (hcsr_dev->end_time - hcsr_dev->start_time);
    hcsr_dev->pulse_width = do_div(pulse_diff, 400);

    if((hcsr_dev->index)!=4){
      hcsr_dev->distance[hcsr_dev->index] = hcsr_dev->pulse_width;
      hcsr_dev->corner_index=0;
      hcsr_dev->index=(hcsr_dev->index )+ 1;
      hcsr_dev->write_index=(hcsr_dev->write_index)+ 1;
    } else {
      hcsr_dev->distance[hcsr_dev->index] = hcsr_dev->pulse_width;
      hcsr_dev->corner_index=4;
      hcsr_dev->index=0;
      hcsr_dev->write_index=0;
    }
    hcsr_dev->index = (hcsr_dev->index)%5;
  }
  return IRQ_HANDLED;
}


/*@hcsr_open: Opens the respective device based on the minor number match */
static int hcsr_open(struct inode *inode, struct file *file)
{
  int minor = iminor(inode);
  HCSR04 *device = get_sensor_device(minor);
  file->private_data = device;
  if(device != NULL) {
    printk (KERN_ALERT "Opened the device file %s, minor number %d \n", device->misc_device.name, minor);
  } else {
    printk (KERN_ALERT "Device is NOT present \n");
  }
  return 0;
}


/*
Read function
@params:   file, buffer, count, pointer position
@function: To retrieve a distance measure (an integer in centimeter) saved in the per-device FIFO
buffer. If the buffer is empty, the read call is blocked until a new measure arrives. Note that, if
there is no on-going sampling operation in one-shot mode, the device should be triggered to
collect a new measure.
*/
static ssize_t hcsr_read(struct file *file, char __user *buf, size_t count, loff_t *f_ps)
{

	HCSR04 *device = file->private_data;
  	unsigned long long temp_distance = 0;
  	int ret = 0;
  	printk (KERN_ALERT "..............inside read..........\n");
  	if(device == NULL) {
    	printk (KERN_ALERT "Read: NO device structure available..\n");
    	return -1;
  	}

    /*Check for buffer NOT empty */
  	if (device->read_index != device->write_index) {
		if(device->corner_index == 4){ // To update the index at the end of circular buffer
 			temp_distance = device->distance[device->read_index];
 			device->read_index=0;
    	} else {
     		temp_distance = device->distance[device->read_index];
     		device->read_index=((device->read_index)+1)%5;
 	 	}
    	ret=copy_to_user(buf, &temp_distance, sizeof(unsigned long long));

    	if( ret < 0) {
    		printk(KERN_ALERT "READ: Could not send data to user....\n");
    		return -1;
    	}
	} else { /* To handle buffer empty case */
  		printk(KERN_DEBUG "READ: Buffer is empty..\n");
    	if(device->ongoing_sampling == 1) {
       		printk(KERN_DEBUG "READ: Sampling is ongoing, wait until IRQ is received.. \n");
      		while(1)
      		{
        		udelay(1);
        		if(device->ongoing_sampling == 0)
        		{
          			break;
        		}
      		}
			if(device->corner_index==4){
 				temp_distance = device->distance[device->read_index];
 				device->read_index=0;
  			} else {
     			temp_distance = device->distance[device->read_index];
     			device->read_index=((device->read_index)+1)%5;
  			}
	 		printk(KERN_DEBUG "READ: pulse width is %llu\n", device->pulse_width);
    		ret=copy_to_user(buf, &temp_distance, sizeof(unsigned long long));

    		if( ret < 0) {
    			printk(KERN_ALERT "READ: Could not send data to user....\n");
    			return -1;
    		}
    	} else {
      		device->ongoing_sampling = 1;
      		printk(KERN_DEBUG "READ: ONE SHOT sending signal..\n");
      		gpio_set_value_cansleep(device->trig_pin, 1);
      		udelay(10);
      		gpio_set_value_cansleep(device->trig_pin, 0);
      		msleep(20);
      		if(device->corner_index==4){
 				temp_distance = device->distance[device->read_index];
 				device->read_index=0;
  			} else {
     			temp_distance = device->distance[device->read_index];
     			device->read_index=((device->read_index)+1)%5;
  			}
			printk(KERN_DEBUG "READ: ONE SHOT pulse width is %llu\n", temp_distance);
    		ret=copy_to_user(buf, &temp_distance, sizeof(unsigned long long));

    		if( ret < 0) {
    			printk(KERN_ALERT "READ: Could not send data to user....\n");
    			return -1;
    		}
    	}
	}
	return 0;
}

/*
Write function
@params:   file, buffer, count, pointer position
@function: In one-shot mode, the write call trigger a new measurement request if there is no on-going
measurement operation. The argument of the write call is an integer. The per-device buffer
should be cleared if the input integer has a non-zero value, and has no effect if the value is 0. For
periodical sampling mode, the sampling operation gets started when the input integer is non-
zero or stopped otherwise.
*/
static ssize_t hcsr_write(struct file *file, const char *buf, size_t count, loff_t *ppos) {
	  int value=0, ret=0, i;
  	HCSR04 *device = file->private_data;

    printk (KERN_ALERT "..........inside write............\n");
  	if(device == NULL) {
     	 printk (KERN_ALERT "Write: NO device structure available..\n");
     	 return -1;
  	}

  	ret = copy_from_user(&value, (int *)buf,  sizeof(int));
  	printk(KERN_ALERT "Write: input value %d \n",value);

  	switch(device->op_mode) {
    	case ONE_SHOT:
      		if (device->ongoing_sampling == 0){
        		printk(KERN_ALERT "Write: Triggering one shot\n");
        		gpio_set_value_cansleep(device->trig_pin,1);
        		udelay(10);
        		gpio_set_value_cansleep(device->trig_pin,0);
        		msleep(10);
        		device->ongoing_sampling = 1;
       		} else {
        		printk("Write: Device is busy\n");
       		}
       		if (value) {
        		for(i=0;i<5;i++) {
          			device->distance[i]= 0;
          		}
        		printk(KERN_INFO "Write: Cleared the buffer.. \n");
        		device->index =0;
        		device->write_index=0;
            device->read_index=0;
       		}
      		break;

    	case PERIODIC:
  			if (value) {
    			if (device->ongoing_sampling == 0  && device->SensorThread != NULL) {
      				printk("starting kthread\n");
      				device->SensorThread = kthread_run(&trigger_signal,device, "Trigger thread");
    			}
  			} else if (device->SensorThread){
    			kthread_stop(device->SensorThread);
    			device->SensorThread = NULL;
          device->ongoing_sampling =0;

  			}
  			break;
  	}
  	device->ongoing_sampling = 0;
  	return 0;
}

/*
Ioctl
@params: file, cmd, pointer to ioctl struct
@function: Two commands “SETPINS” and “SETMODE” to configure the pins and the operation
mode of the sensor. For “SETPINS”, the argument includes two integers to specify Linux gpio
numbers for the sensor’s trigger and echo pins. If any of the input Linux gpio numbers are not
valid, -1 is returned and errno is set to EINVAL. For “SETMODE”, the argument consists of two
integers. A zero value in the first integer sets the operation mode to “one-shot”, while an one
configures the mode to “periodic sampling”. The 2nd integer gives the frequency of the sampling.
*/
static long hcsr_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{
	HCSR04 *HCSR_dev = file->private_data;
	int ret;
	set_ioctl ioctl_config;
	printk (KERN_ALERT ".................inside ioctl............\n");
  switch (cmd) {
		case SETPIN:
            printk(KERN_ALERT "IOCTL: SETPIN\n");
      		ret=copy_from_user(&ioctl_config, (set_ioctl *)arg, sizeof(set_ioctl));
            if(ret < 0){
            	printk(KERN_DEBUG"IOCTL: Failed to do copy from user\n");
            }
			      HCSR_dev->trig_pin=ioctl_config.input3;
            HCSR_dev->trig_dir=ioctl_config.input4;
            HCSR_dev->trig_mux=ioctl_config.input5;
            HCSR_dev->echo_pin=ioctl_config.input6;
            HCSR_dev->echo_dir=ioctl_config.input7;
            HCSR_dev->echo_mux=ioctl_config.input8;
            printk(KERN_INFO "IOCTL: echo_mux: %d, echo_pin: %d, echo_dir: %d\n",
                    HCSR_dev->echo_mux, HCSR_dev->echo_pin, HCSR_dev->echo_dir );
            printk(KERN_INFO "IOCTL: trig_pin: %d, trig_dir: %d, trig_mux: %d \n",
                    HCSR_dev->trig_pin,HCSR_dev->trig_dir,HCSR_dev->trig_mux);

            	if(HCSR_dev->trig_pin != -1){
              ret= gpio_is_valid(HCSR_dev->trig_pin);

              if(ret<0)
            	{
                	printk(KERN_DEBUG "IOCTL: Sensor 1: Invalid trigger pin\n");
                	return EINVAL;
            	}
              }

              if(HCSR_dev->trig_dir != -1){
            	ret= gpio_is_valid(HCSR_dev->trig_dir);
            	if(ret<0)
            	{
                	printk(KERN_DEBUG "IOCTL: Sensor 1: Invalid trigger Dir pin\n");
                	return EINVAL;
            	}
              }

              if(HCSR_dev->trig_mux != -1){
            	ret= gpio_is_valid(HCSR_dev->trig_mux);
            	if(ret<0)
            	{
                	printk(KERN_DEBUG "IOCTL: Sensor 1: Invalid trigger Mux pin\n");
                	return EINVAL;
            	}
              }

              if(HCSR_dev->trig_pin!=-1){
            	ret= gpio_request(HCSR_dev->trig_pin, "TRIG_PIN");
              }
              if(HCSR_dev->trig_dir!=-1){
            	ret= gpio_request(HCSR_dev->trig_dir, "trig_direction");
              }
              if(HCSR_dev->trig_mux!=-1){
            	ret= gpio_request(HCSR_dev->trig_mux, "trig_mux");
              }

              if(HCSR_dev->echo_pin!=-1){
            	ret= gpio_is_valid(HCSR_dev->echo_pin);
            	if(ret<0)
            	{
              		printk(KERN_DEBUG "IOCTL: Sensor 1: Invalid Echo pin\n");
              		return EINVAL;
            	}
              }

              if(HCSR_dev->echo_dir!=-1){
            	ret= gpio_is_valid(HCSR_dev->echo_dir);
            	if(ret<0)
            	{
              		printk(KERN_DEBUG "IOCTL: Sensor 1: Invalid Echo Dir pin\n");
              		return EINVAL;
            	}
              }

              if(HCSR_dev->echo_mux!=-1){
            	ret= gpio_is_valid(HCSR_dev->echo_mux);
            	if(ret<0)
            	{
              		printk(KERN_DEBUG "IOCTL: Sensor 1: Invalid Echo Mux pin\n");
              		return EINVAL;
            	}
              }

            	if(HCSR_dev->echo_pin!=-1){
            	ret= gpio_request(HCSR_dev->echo_pin, "echo_pin");
              }
              if(HCSR_dev->echo_dir!=-1){
            	ret= gpio_request(HCSR_dev->echo_dir, "echo_dir");
              }
              if(HCSR_dev->echo_mux!=-1){
            	ret= gpio_request(HCSR_dev->echo_mux, "echo_mux");
              }

              if(HCSR_dev->trig_mux!=-1){
              gpio_set_value(HCSR_dev->trig_mux, 0);
              }

              if(HCSR_dev->trig_pin!=-1){
            	ret =gpio_direction_output(HCSR_dev->trig_pin, 0);
              }
              if(HCSR_dev->trig_dir!=-1){
              ret =gpio_direction_output(HCSR_dev->trig_dir, 0);
              }

              if(HCSR_dev->echo_mux!=-1){
              gpio_set_value(HCSR_dev->echo_mux, 0);
              }

              if(HCSR_dev->echo_dir!=-1){
              ret=gpio_direction_input(HCSR_dev->echo_dir);
              }
              if(HCSR_dev->echo_pin!=-1){
              ret =gpio_direction_input(HCSR_dev->echo_pin);
              }
              if (device_count == 0){
                HCSR_dev->echo_irq = gpio_to_irq(HCSR_dev->echo_pin);
               	ret = request_irq(HCSR_dev->echo_irq, echo_handler, IRQ_TYPE_EDGE_RISING, "HCSR_1", HCSR_dev);

                  if(ret < 0)
            		{
              		printk(KERN_ALERT "IOCTL: Error requesting irq: %d\n", HCSR_dev->echo_irq);
              		return -EBADRQC;
            		}else {
                    	printk(KERN_ALERT "IOCTL: Handler allocated to irq: %d\n", HCSR_dev->echo_irq);
      			}
            device_count += 1;
          } else if (device_count == 1){
            HCSR_dev->echo_irq = gpio_to_irq(HCSR_dev->echo_pin);
            ret = request_irq(HCSR_dev->echo_irq, echo_handler, IRQ_TYPE_EDGE_RISING, "HCSR_2", HCSR_dev);
              if(ret < 0)
            {
              printk(KERN_ALERT "IOCTL: Error requesting irq: %d\n", HCSR_dev->echo_irq);
              return -EBADRQC;
            }else {
                  printk(KERN_ALERT "IOCTL: Handler allocated to irq: %d\n", HCSR_dev->echo_irq);
        }
        device_count += 1;
          }
		break;

        case SETMODE:
			      printk(KERN_ALERT "IOCTL: SETMODE:\n");
          	ret=copy_from_user(&ioctl_config, (set_ioctl *)arg, sizeof(set_ioctl));
          	if(ret<0){
            	printk(KERN_DEBUG"IOCTL: Copy from user failed\n");
           	}
            HCSR_dev->op_mode = ioctl_config.input1;
            HCSR_dev->sampling_frequency = ioctl_config.input2;
            printk(KERN_ALERT "IOCTL: OP_MODE: %d\n", HCSR_dev->op_mode);
            printk(KERN_ALERT "IOCTL: sampling_frequency: %d\n", HCSR_dev->sampling_frequency);
            break;

        default:
            return -EINVAL;
    }
    return 0;
}


/*@hcsr_release: Called when the device file is closed */
static int hcsr_release(struct inode *inode, struct file *file)
{
  HCSR04 *device = file->private_data;
  printk (KERN_ALERT "\n...........inside release..............\n");
  printk (KERN_ALERT "RELEASE: released the device file %s minor %d", device->misc_device.name, device->misc_device.minor);
  return 0;
}

/*
 * The fops on HCSR04.
 */
static const struct file_operations hcsr_fops = {
  .owner    = THIS_MODULE,
  .open   = hcsr_open,
  .read   = hcsr_read,
  .write    = hcsr_write,
  .unlocked_ioctl = hcsr_ioctl,
  .release  = hcsr_release,
};

/*
 * @hcsr_init: Device is initialized & Registered as misc device with the required fops & params.
 */
static int __init hcsr_init(void)
{

  int ret;
	printk (KERN_ALERT "\n................inside init...................\n");
  printk("\nINIT: Driver is inserted..\n");

  memset (&hc1, 0, sizeof(HCSR04));
  hc1.misc_device.minor = MISC_DYNAMIC_MINOR;
  hc1.misc_device.name = "hc1";
  hc1.misc_device.fops = &hcsr_fops;

  ret = misc_register(&hc1.misc_device);
  if (ret){
    printk(KERN_ALERT "INIT: Unable to register \"dev\"hc1 \n");
      return ret;
  }

  list_add_tail(&hc1.next, &sensor_devices);

  memset (&hc2, 0, sizeof(HCSR04));
  hc2.misc_device.minor = MISC_DYNAMIC_MINOR;
  hc2.misc_device.name = "hc2";
  hc2.misc_device.fops = &hcsr_fops;

  ret = misc_register(&hc2.misc_device);
  if (ret){
    printk(KERN_ALERT "INIT: Unable to register \"dev\"hc2\n");
      return ret;
  }

  list_add_tail(&hc2.next, &sensor_devices);

  printk (KERN_ALERT "INIT: I'm in..\n");
    return ret;
}


/* Deletes and deallocates all the hash entries before unregistering the hash
 * table device.
 */
static void __exit hcsr_exit(void)
{
  printk (KERN_ALERT ".................inside exit.................\n");

  if(hc1.SensorThread) {
    kthread_stop(hc1.SensorThread);
  }
  if(hc2.SensorThread) {
    kthread_stop(hc2.SensorThread);
  }

if(hc1.trig_pin!=-1){
gpio_free(hc1.trig_pin);
}
if(hc1.echo_pin!=-1){
gpio_free(hc1.echo_pin);
}

if(hc1.trig_dir!=-1){
gpio_free(hc1.trig_dir);
}
if(hc1.echo_dir!=-1){
gpio_free(hc1.echo_dir);
}

if(hc1.trig_mux!=-1){
gpio_free(hc1.trig_mux);
}
if(hc1.echo_mux!=-1){
gpio_free(hc1.echo_mux);
}

if(hc2.trig_pin!=-1){
gpio_free(hc2.trig_pin);
}
if(hc2.echo_pin!=-1){
gpio_free(hc2.echo_pin);
}

if(hc2.trig_dir!=-1){
gpio_free(hc2.trig_dir);
}
if(hc2.echo_dir!=-1){
gpio_free(hc2.echo_dir);
}

if(hc2.trig_mux!=-1){
gpio_free(hc2.trig_mux);
}
if(hc2.echo_mux!=-1){
gpio_free(hc2.echo_mux);
}

  if(hc1.echo_irq){
  free_irq(hc1.echo_irq, &hc1);
  }
  if(hc2.echo_irq){
  free_irq(hc2.echo_irq, &hc2);
  }
list_del (&hc1.next);
list_del (&hc2.next);

misc_deregister(&(hc1.misc_device));
misc_deregister(&(hc2.misc_device));
printk(KERN_ALERT "EXIT: Driver file succesfully removed\n");
}

module_init(hcsr_init);
module_exit(hcsr_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("hcsr_04");
MODULE_VERSION("dev");
