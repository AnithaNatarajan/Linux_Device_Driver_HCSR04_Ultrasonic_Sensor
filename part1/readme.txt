Part-1: A Linux kernel module to enable user-space device interface for HC-SR04

Steps to follow:
1. Compile Makefile; [This file compiles both driver & user program with sensor.ko & test1] 
2. Copy them to the galileo board & insmod using "insmod sensor.ko" 
3. Run the user program by "./test1"
4. Now the user will be prompted to provide values for trigger pin, trigger direction, trigger mux, echo pin, echo direction, echo mux separated by space
5. Once the value is got, PINs are set. Now sampling frequency is also obtained from command prompt.

NOTE:
====
There are 2 test files [Modify the Makefile to run the desired the user program]: 
 1. test1.c  - Opens 1st device & performs all the operation & gets closed; Then the 2nd device is opened to perform operation
 2. test2.c  - Opens 2 devices at the same time & performs operation in different sequence

The sample output, screenshots & dmesg are uploaded for reference.
