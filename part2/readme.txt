Part-2: Platform device driver and sysfs interface for HC-SR04

Steps to follow:
1. Compile Makefile; [This file compiles platform device, platform driver and user program to hcsr04_platform_device.ko, hcsr_platform_driver.ko and test1] 
2. Copy them to the galileo board and insmod the device module and driver module:
#insmod hcsr04_platform_device.ko
#insmod hcsr_platform_driver.ko

Method 1 : Via User Interface:
===============================
3.a) Run the user program by "./test1"
3.b) Now the user will be prompted to provide values for trigger pin, trigger direction, trigger mux, echo pin, echo direction,      echo mux separated by space
3.c) Once the value is got, PINs are set. Now sampling frequency is also obtained from command prompt.

Method 2: Via Sysfs Interface:
===============================
3.a) Run the script by "./part2_script.sh"
3.b) Pins are set and the required operations are performed.
3.c) Sample output-screenshot(part2_screenshot.odt) and dmesg(part2_dmesg.txt) are uploaded.


