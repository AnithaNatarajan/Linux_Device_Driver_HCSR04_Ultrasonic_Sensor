#include <linux/platform_device.h>
#include<linux/miscdevice.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/slab.h>

#include<linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/unistd.h>
#include <linux/types.h>

#ifndef __HCSR04_PLATFORM_H__

#define __HCSR04_PLATFORM_H__

struct hcsr_platform_device {
		char 			*name;
		int			dev_no;
		struct platform_device 	plf_dev;
		struct miscdevice misc_dev;
		dev_t gko_dev;
		struct device *gko_device;
		int trigger, echo, mode, frequency, enable;
    int trig_dir, trig_mux, echo_dir, echo_mux;
		unsigned long long distance;
   int ongoing_sampling,edge_flag, thread_flag;
   unsigned long long start_time, end_time, pulse_width, pulse_diff, distance_buf[5];
		int read_index,write_index,index,corner_index, echo_irq;
		struct list_head next;
		struct task_struct *SensorThread;
};

#endif /* __GPIO_FUNC_H__ */
