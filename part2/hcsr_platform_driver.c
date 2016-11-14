/*
 * Platfomr driver program to bind with device
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <asm/gpio.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/hashtable.h>
#include <linux/ioctl.h>
#include <linux/moduleparam.h>
#include "hcsr04_platform_device.h"
#include "ioctl_private.h"

#define ONE_SHOT 0
#define PERIODIC 1
#define CLASS_NAME "HCSR"
#define DRIVER_NAME "HCSR_of_driver"

static const struct platform_device_id P_id_table[] = {
         { "hcsr_plf_dev_1", 0 },
         { "hcsr_plf_dev_2", 0 },
   { },
};
static int count = 0;
static int device_count = 0;
static struct class *gko_class;


/////////////////////////////////////////////////////////
static LIST_HEAD(sensor_devices);

/*@get_sensor_device: Returns the sensor object from the linked list*/
struct hcsr_platform_device *get_sensor_device (int minor) {
  struct hcsr_platform_device  *hcsr_dev;
  list_for_each_entry(hcsr_dev, &sensor_devices, next) {
    if (hcsr_dev->misc_dev.minor == minor) {
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
  struct hcsr_platform_device *hcsr_dev = (struct hcsr_platform_device *)data;
  hcsr_dev->ongoing_sampling = 1;
  while(!kthread_should_stop())
  {
    printk("Triggering pulse...  pulse_width %llu \n ", hcsr_dev->distance );
    gpio_set_value_cansleep(hcsr_dev->trigger,1);
    udelay(10);
    gpio_set_value_cansleep(hcsr_dev->trigger,0);
    msleep(hcsr_dev->frequency);
  }
  return 0;
}

/*@echo_handler: Registered interrupt to handle the echo response.
 * It gets the timestamp & calculates the pulse width to store it
 * in circular FIFO buffer
 */
static irqreturn_t echo_handler(int irq, void *dev_id )
{
    struct hcsr_platform_device *hcsr_dev=dev_id;
    unsigned long long pulse_diff = 0;

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
    hcsr_dev->distance = do_div(pulse_diff, 400);
    pulse_diff=hcsr_dev->distance * 17 ;
    hcsr_dev->distance = do_div(pulse_diff, 1000);

    if((hcsr_dev->index)!=4){
      hcsr_dev->distance_buf[hcsr_dev->index] = hcsr_dev->distance;
      hcsr_dev->corner_index=0;
      hcsr_dev->index=(hcsr_dev->index)+ 1;
      hcsr_dev->write_index=(hcsr_dev->write_index)+ 1;
    } else {
      hcsr_dev->distance_buf[hcsr_dev->index] = hcsr_dev->distance;
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
  struct hcsr_platform_device *device = get_sensor_device(minor);
  file->private_data = device;
  if(device != NULL) {
    printk (KERN_ALERT "Opened the device file %s, minor number %d \n", device->misc_dev.name, minor);
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

	struct hcsr_platform_device *device = file->private_data;
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
 			temp_distance = device->distance_buf[device->read_index];
 			device->read_index=0;
    	} else {
     		temp_distance = device->distance_buf[device->read_index];
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
 				temp_distance = device->distance_buf[device->read_index];
 				device->read_index=0;
  			} else {
     			temp_distance = device->distance_buf[device->read_index];
     			device->read_index=((device->read_index)+1)%5;
  			}
	 		printk(KERN_DEBUG "READ: pulse width is %llu\n", device->distance);
    		ret=copy_to_user(buf, &temp_distance, sizeof(unsigned long long));

    		if( ret < 0) {
    			printk(KERN_ALERT "READ: Could not send data to user....\n");
    			return -1;
    		}
    	} else {
      		device->ongoing_sampling = 1;
      		printk(KERN_DEBUG "READ: ONE SHOT sending signal..\n");
      		gpio_set_value_cansleep(device->trigger, 1);
      		udelay(10);
      		gpio_set_value_cansleep(device->trigger, 0);
      		msleep(20);
      		if(device->corner_index==4){
 				temp_distance = device->distance_buf[device->read_index];
 				device->read_index=0;
  			} else {
     			temp_distance = device->distance_buf[device->read_index];
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
  	struct hcsr_platform_device *device = file->private_data;

    printk (KERN_ALERT "..........inside write............\n");
  	if(device == NULL) {
     	 printk (KERN_ALERT "Write: NO device structure available..\n");
     	 return -1;
  	}

  	ret = copy_from_user(&value, (int *)buf,  sizeof(int));
  	printk(KERN_ALERT "Write: input value %d \n",value);

  	switch(device->mode) {
    	case ONE_SHOT:
      		if (device->ongoing_sampling == 0){
        		printk(KERN_ALERT "Write: Triggering one shot\n");
        		gpio_set_value_cansleep(device->trigger,1);
        		udelay(10);
        		gpio_set_value_cansleep(device->trigger,0);
        		msleep(10);
        		device->ongoing_sampling = 1;
       		} else {
        		printk("Write: Device is busy\n");
       		}
       		if (value) {
        		for(i=0;i<5;i++) {
          			device->distance_buf[i]= 0;
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
	struct hcsr_platform_device *HCSR_dev = file->private_data;
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
			      HCSR_dev->trigger=ioctl_config.input3;
            HCSR_dev->trig_dir=ioctl_config.input4;
            HCSR_dev->trig_mux=ioctl_config.input5;
            HCSR_dev->echo=ioctl_config.input6;
            HCSR_dev->echo_dir=ioctl_config.input7;
            HCSR_dev->echo_mux=ioctl_config.input8;
            printk(KERN_INFO "IOCTL: echo_mux: %d, echo: %d, echo_dir: %d\n",
                    HCSR_dev->echo_mux, HCSR_dev->echo, HCSR_dev->echo_dir );
            printk(KERN_INFO "IOCTL: trigger: %d, trig_dir: %d, trig_mux: %d \n",
                    HCSR_dev->trigger,HCSR_dev->trig_dir,HCSR_dev->trig_mux);

          //  if (device_count == 0) {
          //    	device_count += 1;

            	if(HCSR_dev->trigger != -1){
              ret= gpio_is_valid(HCSR_dev->trigger);

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

              if(HCSR_dev->trigger!=-1){
            	ret= gpio_request(HCSR_dev->trigger, "trigger");
              }
              if(HCSR_dev->trig_dir!=-1){
            	ret= gpio_request(HCSR_dev->trig_dir, "trig_direction");
              }
              if(HCSR_dev->trig_mux!=-1){
            	ret= gpio_request(HCSR_dev->trig_mux, "trig_mux");
              }

              if(HCSR_dev->echo!=-1){
            	ret= gpio_is_valid(HCSR_dev->echo);
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

            	if(HCSR_dev->echo!=-1){
            	ret= gpio_request(HCSR_dev->echo, "echo");
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

              if(HCSR_dev->trigger!=-1){
            	ret =gpio_direction_output(HCSR_dev->trigger, 0);
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
              if(HCSR_dev->echo!=-1){
              ret =gpio_direction_input(HCSR_dev->echo);
              }
              if (device_count == 0){
                HCSR_dev->echo_irq = gpio_to_irq(HCSR_dev->echo);
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
            HCSR_dev->echo_irq = gpio_to_irq(HCSR_dev->echo);
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
                  HCSR_dev->mode = ioctl_config.input1;
                  HCSR_dev->frequency = ioctl_config.input2;
                  printk(KERN_ALERT "IOCTL: mode: %d\n", HCSR_dev->mode);
                  printk(KERN_ALERT "IOCTL: frequency: %d\n", HCSR_dev->frequency);
                  break;

              default:
                  return -EINVAL;
          }
          return 0;
      }


/*@hcsr_release: Called when the device file is closed */
static int hcsr_release(struct inode *inode, struct file *file)
{
  struct hcsr_platform_device *device = file->private_data;
  printk (KERN_ALERT "\n...........inside release..............\n");
  printk (KERN_ALERT "RELEASE: released the device file %s minor %d", device->misc_dev.name, device->misc_dev.minor);
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

////////////////////////////////////////////////
ssize_t trigger_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf,
                                  size_t count)
{
       struct hcsr_platform_device *hcsr_dev = dev_get_drvdata(dev);
       int hscr_tmp, ret = 0;
       sscanf(buf, "%d", &hscr_tmp);
       atomic_set((atomic_t *)(&hcsr_dev->trigger),hscr_tmp);
      ret= gpio_is_valid(hcsr_dev->trigger);
      if(ret<0)
      {
          printk(KERN_DEBUG "trigger pin is not valid:Sensor 1\n");
          return EINVAL;
      }
      ret= gpio_request(hcsr_dev->trigger, "gpio_trigger");
      ret =gpio_direction_output(hcsr_dev->trigger, 0);
      
      return count;
}


ssize_t trigger_show(struct device *dev,
                 struct device_attribute *attr, char *buf)
{
  const struct hcsr_platform_device *hcsr_dev = dev_get_drvdata(dev);
  return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read((atomic_t*)(&(hcsr_dev->trigger))));
}

static DEVICE_ATTR(trigger, S_IRUSR | S_IWUSR, trigger_show, trigger_store);

//////////////////////////////////////
ssize_t echo_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf,
                                  size_t count)
{
      struct hcsr_platform_device *hcsr_dev=dev_get_drvdata(dev);
      int hscr_tmp, ret = 0;
      sscanf(buf, "%d", &hscr_tmp);
      atomic_set((atomic_t *)(&hcsr_dev->echo),hscr_tmp);
      ret= gpio_is_valid(hcsr_dev->echo);
      if(ret<0)
      {
        printk(KERN_DEBUG "Echo pin is not valid\n");
        return EINVAL;
      }

      ret= gpio_request(hcsr_dev->echo, "gpio_echo");
      ret =gpio_direction_input(hcsr_dev->echo);
       
      hcsr_dev->echo_irq = gpio_to_irq(hcsr_dev->echo);
 printk(KERN_ALERT "IRQ: %d\n", hcsr_dev->echo_irq);

      ret = request_irq(hcsr_dev->echo_irq, echo_handler, IRQ_TYPE_EDGE_RISING, hcsr_dev->name, hcsr_dev);

      if(ret < 0)
          {
            printk(KERN_ALERT "Error in requesting irq: %d\n", hcsr_dev->echo_irq);
            return -EBADRQC;
          } else {
                  printk(KERN_ALERT "Handler allocated to irq: %d\n", hcsr_dev->echo_irq);
            }
       return count;
}


ssize_t echo_show(struct device *dev,
                 struct device_attribute *attr, char *buf)
{
  const struct hcsr_platform_device *hcsr_dev = dev_get_drvdata(dev);
  return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read((atomic_t*)(&(hcsr_dev->echo))));
}

static DEVICE_ATTR(echo, S_IRUSR | S_IWUSR, echo_show, echo_store);

//////////////////////////////////
ssize_t mode_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf,
                                  size_t count)
{
       const struct hcsr_platform_device *hcsr_dev=dev_get_drvdata(dev);
	     int hscr_tmp;
       sscanf(buf, "%d", &hscr_tmp);
       atomic_set((atomic_t *)(&hcsr_dev->mode),hscr_tmp);
       return count;
}

ssize_t mode_show(struct device *dev,
        		     struct device_attribute *attr, char *buf)
{
  const struct hcsr_platform_device *hcsr_dev = dev_get_drvdata(dev);
  return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read((atomic_t*)(&(hcsr_dev->mode))));
}

static DEVICE_ATTR(mode, S_IRUSR | S_IWUSR, mode_show, mode_store);

/////////////////////////////////////////////
ssize_t frequency_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf,
                                  size_t count)
{
      const struct hcsr_platform_device *hcsr_dev=dev_get_drvdata(dev);
      int hscr_tmp;
      sscanf(buf, "%d", &hscr_tmp);
      atomic_set((atomic_t *)(&hcsr_dev->frequency),hscr_tmp);
      return count;
}


ssize_t frequency_show(struct device *dev,
        		     struct device_attribute *attr, char *buf)
{
  const struct hcsr_platform_device *hcsr_dev = dev_get_drvdata(dev);
  return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read((atomic_t*)(&(hcsr_dev->frequency))));
}

static DEVICE_ATTR(frequency, S_IRUSR | S_IWUSR, frequency_show, frequency_store);

ssize_t enable_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf,
                                  size_t count)
{
      struct hcsr_platform_device *hcsr_dev=dev_get_drvdata(dev);
      int hcsr_tmp;
      sscanf(buf, "%d", &hcsr_tmp);
      atomic_set((atomic_t *)(&hcsr_dev->enable),hcsr_tmp);
       
      if(hcsr_tmp == 1) {
        if (hcsr_dev->mode == PERIODIC) {
          if( !hcsr_dev->SensorThread ) {
          printk("PERIODIC: starting kthread..\n");
          hcsr_dev->SensorThread = kthread_run(&trigger_signal,hcsr_dev, hcsr_dev->name);
        }
      } else {
        printk("ONE SHOT: Triggering the sample.. \n");
		    hcsr_dev->ongoing_sampling = 1;
	      gpio_set_value_cansleep(hcsr_dev->trigger, 1);
        udelay(10);
        gpio_set_value_cansleep(hcsr_dev->trigger, 0);
        msleep(20);
      }
      } else if (hcsr_tmp == 0) {
          if (hcsr_dev->mode == PERIODIC) {
          if( hcsr_dev->SensorThread != NULL ) {
          printk("PERIODIC: stopping kthread..\n");
          kthread_stop(hcsr_dev->SensorThread);
          hcsr_dev->ongoing_sampling = 0;
          hcsr_dev->SensorThread = NULL;
        }
      }
      }
       return count;
}

ssize_t enable_show(struct device *dev,
                 struct device_attribute *attr, char *buf)
{
  const struct hcsr_platform_device *hcsr_dev = dev_get_drvdata(dev);
  return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read((atomic_t*)(&(hcsr_dev->enable))));
}

static DEVICE_ATTR(enable, S_IRUSR | S_IWUSR, enable_show, enable_store);

////////////////////////////////

ssize_t distance_store(struct device *dev,
                 struct device_attribute *attr, char *buf)
{
       const struct hcsr_platform_device *hcsr_dev=dev_get_drvdata(dev);
       int hscr_tmp;
       sscanf(buf, "%d", &hscr_tmp);
       atomic_set((atomic_t *)(&hcsr_dev->distance),hscr_tmp);
       return count;
}


ssize_t distance_show(struct device *dev,
                 struct device_attribute *attr, char *buf)
{
  struct hcsr_platform_device *hcsr_dev = dev_get_drvdata(dev);
  printk("DISTANCE: %llu.. \n", hcsr_dev->distance);      
  return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read((atomic_t*)(&(hcsr_dev->distance))));
}
static DEVICE_ATTR(distance, S_IRUSR | S_IWUSR, distance_show, NULL);

////////////////////////

static int hcsr_driver_probe(struct platform_device *pdev)
{
	struct hcsr_platform_device *hcsr_dev;
	int ret = 0;

	hcsr_dev = container_of(pdev, struct hcsr_platform_device, plf_dev);
	memset(&hcsr_dev->misc_dev, '\0', sizeof(struct miscdevice));
	hcsr_dev->misc_dev.name = hcsr_dev->name;
	hcsr_dev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	hcsr_dev->misc_dev.fops = &hcsr_fops;
	ret = misc_register(&(hcsr_dev->misc_dev));
	if(ret) {
  	 printk(KERN_ERR " Misc registration failed.. \n");
	 goto misc_unregister;
	}

  list_add_tail(&hcsr_dev->next, &sensor_devices);


/* class */
if(count == 0) {
        gko_class = class_create(THIS_MODULE, CLASS_NAME);
        if (IS_ERR(gko_class)) {
                printk(KERN_ERR  " cant create class %s\n", CLASS_NAME);
		goto class_err;
            }
count += 1;
}

/* device */
        hcsr_dev->gko_device = device_create(gko_class, NULL, hcsr_dev->gko_dev, hcsr_dev, hcsr_dev->misc_dev.name);
        if (IS_ERR(hcsr_dev->gko_device)) {
                printk(KERN_ERR  " cant create device %s\n", hcsr_dev->misc_dev.name);
                goto device_err;
        }


/* device attribute on sysfs - trigger*/
        ret = device_create_file(hcsr_dev->gko_device, &dev_attr_trigger);
        if (ret < 0) {
                printk(KERN_ERR  " cant create device attribute %s %s\n",
                       hcsr_dev->misc_dev.name, dev_attr_trigger.attr.name);
        }

/* device attribute on sysfs -echo*/
        ret = device_create_file(hcsr_dev->gko_device, &dev_attr_echo);
        if (ret < 0) {
                printk(KERN_ERR  " cant create device attribute %s %s\n",
                       hcsr_dev->misc_dev.name, dev_attr_echo.attr.name);
        }

/* device attribute on sysfs -mode*/
        ret = device_create_file(hcsr_dev->gko_device, &dev_attr_mode);
        if (ret < 0) {
                printk(KERN_ERR  " cant create device mode %s %s\n",
                       hcsr_dev->misc_dev.name, dev_attr_mode.attr.name);
        }

/* device attribute on sysfs -frequency*/
        ret = device_create_file(hcsr_dev->gko_device, &dev_attr_frequency);
        if (ret < 0) {
                printk(KERN_ERR  " cant create device attribute %s %s\n",
                       hcsr_dev->misc_dev.name, dev_attr_frequency.attr.name);
        }


/* device attribute on sysfs -enable*/
        ret = device_create_file(hcsr_dev->gko_device, &dev_attr_enable);
        if (ret < 0) {
                printk(KERN_ERR  " cant create device attribute %s %s\n",
                       hcsr_dev->misc_dev.name, dev_attr_enable.attr.name);
        }

/* device attribute on sysfs -distance*/
        ret = device_create_file(hcsr_dev->gko_device, &dev_attr_distance);
        if (ret < 0) {
                printk(KERN_ERR  " cant create device attribute %s %s\n",
                       hcsr_dev->misc_dev.name, dev_attr_distance.attr.name);
        }

	printk(KERN_ERR "Probe: Registration complete %s - minor number: %d\n",hcsr_dev->misc_dev.name, hcsr_dev->misc_dev.minor);
	return 0;

device_err:
        device_destroy(gko_class, hcsr_dev->gko_dev);
class_err:
        class_unregister(gko_class);
        class_destroy(gko_class);

misc_unregister:
	misc_deregister(&(hcsr_dev->misc_dev));
return -EFAULT;
};





























///////////////////////////////////////////
/////////////////////////////////////////////////////




static int hcsr_driver_remove(struct platform_device *pdev)
{


      struct hcsr_platform_device *hcsr_dev;
      hcsr_dev = container_of(pdev, struct hcsr_platform_device, plf_dev);
//////////////////////
//////////////////////
if(hcsr_dev->SensorThread) {
  kthread_stop(hcsr_dev->SensorThread);
}
if(hcsr_dev->trigger!=-1){
gpio_free(hcsr_dev->trigger);
}
if(hcsr_dev->echo!=-1){
gpio_free(hcsr_dev->echo);
}

if(hcsr_dev->trig_dir!=-1){
gpio_free(hcsr_dev->trig_dir);
}
if(hcsr_dev->echo_dir!=-1){
gpio_free(hcsr_dev->echo_dir);
}

if(hcsr_dev->trig_mux!=-1){
gpio_free(hcsr_dev->trig_mux);
}
if(hcsr_dev->echo_mux!=-1){
gpio_free(hcsr_dev->echo_mux);
}

if(hcsr_dev->echo_irq){
free_irq(hcsr_dev->echo_irq, hcsr_dev);
}
        device_destroy(gko_class, hcsr_dev->gko_dev);
 if (count == 2) {
        class_unregister(gko_class);
        class_destroy(gko_class);
}
list_del(&hcsr_dev->next);
        misc_deregister(&(hcsr_dev->misc_dev));
count += 1;
      return 0;
};

static struct platform_driver HCSR_plf_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= hcsr_driver_probe,
	.remove		= hcsr_driver_remove,
	.id_table	= P_id_table,
};


module_platform_driver(HCSR_plf_driver);

MODULE_LICENSE("GPL");
