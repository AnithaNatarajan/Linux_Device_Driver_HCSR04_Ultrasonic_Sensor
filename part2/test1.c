/*
 * Part 1: User space program to test HCSR-04 device driver
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "ioctl_private.h"

int main()
{
    int fd1, fd2;
    int ret;
    int buf,frequency, time;
    set_ioctl sensor_data;
    unsigned long long obj_dis;
    int trig_pin1, trig_dir1, trig_mux1, echo_pin1, echo_dir1, echo_mux1;

	/************DEVICE 1******************/
    fd1 = open("/dev/HCSRdevice1", O_RDWR);
	if(fd1<0){
		printf("Fail to open device 1\n");
	}

    printf("For sensor 1, Enter space seperated: Trig pin, Trig_direction, Trig_mux, Echo pin, Echo_direction, Echo_mux:\n");
    scanf("%d %d %d %d %d %d",&trig_pin1, &trig_dir1, &trig_mux1, &echo_pin1, &echo_dir1, &echo_mux1);

    printf("For One Shot Mode: \n");
    printf("Enter Frequency in KHz: \n");
    scanf("%d", &frequency);

    sensor_data.input3=trig_pin1;
    sensor_data.input4=trig_dir1;
    sensor_data.input5=trig_mux1;
    sensor_data.input6=echo_pin1;
    sensor_data.input7=echo_dir1;
    sensor_data.input8=echo_mux1;

    ret = ioctl(fd1,SETPIN,&sensor_data);
    if (ret < 0) {
        printf("ioctl failed\n");
        exit(-1);
    }

   /************ sampling time **********/
time = 1/(frequency*1000);
    printf("For One Shot Mode:  %d\n", time);



    /************ ONE SHOT MODE TESTING******************/
    sensor_data.input1=0;
    sensor_data.input2=time;

	printf("One shot: setting mode\n");

    ret =ioctl(fd1,SETMODE, &sensor_data);
    if (ret < 0) {
		printf("ioctl failed\n");
		exit(-1);
	}

    printf("One shot: write(non-zero)\n");
    buf=1;
    ret=write(fd1,&buf,sizeof(buf));
    if(ret < 0){
    	printf("write function failed\n");
    }
    sleep(2);

   	printf("One shot: read\n");
  	ret=read(fd1,&obj_dis,sizeof(obj_dis));
    if(ret < 0) {
    	printf(" Read function failed\n");
    }
    else{
    	obj_dis= obj_dis * (0.017);
		printf("calculated object distance is %llu centimeters\n",obj_dis);
    }

	printf("One shot: read again\n");
	ret=read(fd1,&obj_dis,sizeof(obj_dis));
    if(ret < 0){
    	printf(" Read function failed\n");
    }else{
    	obj_dis= obj_dis * (0.017);
		printf("calculated object distance is %llu centimeters\n",obj_dis);
    }

	printf("One shot: write\n");
	buf=0;
	ret=write(fd1,&buf,sizeof(buf));
    if(ret < 0){
    	printf("write function failed\n");
    }
    sleep(2);

    ret=read(fd1,&obj_dis,sizeof(obj_dis));
    if(ret < 0) {
    	printf(" Read function failed\n");
    } else{
    	obj_dis= obj_dis * (0.017);
		printf("calculated object distance is %llu centimeters\n",obj_dis);
    }

   
    ret=write(fd1,&buf,sizeof(buf));
    if(ret < 0){
    	printf("write function failed\n");
    }
    ret=read(fd1,&obj_dis,sizeof(obj_dis));
    if(ret < 0) {
    	printf(" Read function failed\n");
    } else{
    	obj_dis= obj_dis * (0.017);
		printf("calculated object distance is %llu centimeters\n",obj_dis);
    }



	/************ PERIODIC MODE TESTING******************/
  printf("For Periodic Mode: \n");
  printf("Enter Frequency in KHz: \n");
  scanf("%d", &frequency);
time = 1/(frequency*1000);
  sensor_data.input1=1;
  sensor_data.input2=time;

	printf("Periodic: setting mode\n");
	ioctl(fd1,SETMODE, &sensor_data);
	printf("Periodic: write(4)\n");
    buf=1;
    ret=write(fd1,&buf,sizeof(buf));
    if(ret < 0){
    	printf("write function failed\n");
    }
    sleep(2);

	printf("Periodic: read\n");
	ret=read(fd1,&obj_dis,sizeof(obj_dis));
   	if(ret < 0) {
    	printf(" Read function failed\n");
    }else{
    	obj_dis= obj_dis * (0.017);
		printf("calculated object distance is %llu centimeters\n",obj_dis);
    }

	printf("Periodic: write(0)\n");
	buf=0;
	ret=write(fd1,&buf,sizeof(buf));
   	if(ret < 0){
    	printf("write function failed\n");
    }
    sleep(2);
	printf("Periodic: read\n");
	ret=read(fd1,&obj_dis,sizeof(obj_dis));
    if(ret < 0){
    	printf(" Read function failed\n");
    }else{
    	obj_dis= obj_dis * (0.017);
		printf("calculated object distance is %llu centimeters\n",obj_dis);
    }
	printf("Periodic: write(0)\n");
	buf=0;
	ret=write(fd1,&buf,sizeof(buf));
    if(ret < 0){
    	printf("write function failed\n");
    }
    sleep(2);

	printf("Periodic: read\n");
	ret=read(fd1,&obj_dis,sizeof(obj_dis));
    if(ret < 0){
    	printf(" Read function failed\n");
    }else{
    	obj_dis= obj_dis * (0.017);
		printf("calculated object distance is %llu centimeters\n",obj_dis);
    }
	printf("Periodic: read\n");
	ret=read(fd1,&obj_dis,sizeof(obj_dis));
    if(ret < 0){
    	printf(" Read function failed\n");
    }else{
    	obj_dis= obj_dis * (0.017);
		printf("calculated object distance is %llu centimeters\n",obj_dis);
    }

    printf("...close fd...\n");
 	close(fd1);


	/************ DEVICE 2******************/
  printf("Now using device 2.....\n");
	fd2 = open("/dev/HCSRdevice2", O_RDWR);
	if( fd2<0 ){
		printf("Fail to open device file /dev/HCSRdevice2 \n");
	}

	/************ ONE SHOT MODE TESTING******************/

  printf("For sensor 2, Enter space seperated: Trig pin, Trig_direction, Trig_mux, Echo pin, Echo_direction, Echo_mux:\n");
  scanf("%d %d %d %d %d %d",&trig_pin1, &trig_dir1, &trig_mux1, &echo_pin1, &echo_dir1, &echo_mux1);

      printf("For One Shot Mode: \n");
      printf("Enter Frequency in KHz: \n");
      scanf("%d", &frequency);
time = 1/(frequency*1000);
      sensor_data.input3=trig_pin1;
      sensor_data.input4=trig_dir1;
      sensor_data.input5=trig_mux1;
      sensor_data.input6=echo_pin1;
      sensor_data.input7=echo_dir1;
      sensor_data.input8=echo_mux1;

      ret = ioctl(fd2,SETPIN,&sensor_data);
      if (ret < 0) {
          printf("ioctl failed\n");
          exit(-1);
      }

      sensor_data.input1=0;
      sensor_data.input2=time;

    printf("second device: One shot: setting pin\n");
    printf("second device... Setting pins\n");

    ret = ioctl(fd2,SETPIN,&sensor_data);
	if (ret < 0) {
		printf("ioctl failed\n");
		exit(-1);
	}
  printf("One shot: setting mode\n");
    ret =ioctl(fd2,SETMODE, &sensor_data);
    if (ret < 0) {
    printf("ioctl failed\n");
    exit(-1);
  }


    printf("second device: One shot: write(4)\n");
    buf=1;
    ret=write(fd2,&buf,sizeof(buf));
    if(ret < 0){
    	printf("write function failed\n");
    }
    sleep(2);

   	printf("second device: One shot: read\n");
  	ret=read(fd2,&obj_dis,sizeof(obj_dis));
    if(ret < 0) {
    	printf(" Read function failed\n");
    }
    else{
    	obj_dis= obj_dis * (0.017);
		printf("calculated object distance is %llu centimeters\n",obj_dis);
    }

	printf("second device: One shot: read again\n");
	ret=read(fd2,&obj_dis,sizeof(obj_dis));
    if(ret < 0){
    	printf(" Read function failed\n");
    }else{
    	obj_dis= obj_dis * (0.017);
		printf("calculated object distance is %llu centimeters\n",obj_dis);
    }

	printf("second device: One shot: write\n");
	buf=0;
	ret=write(fd2,&buf,sizeof(buf));
    if(ret < 0){
    	printf("write function failed\n");
    }
    sleep(2);

    ret=read(fd2,&obj_dis,sizeof(obj_dis));
    if(ret < 0) {
    	printf(" Read function failed\n");
    } else{
    	obj_dis= obj_dis * (0.017);
		printf("calculated object distance is %llu centimeters\n",obj_dis);
    }

	/************ PERIODIC TESTING******************/
  printf("For Periodic Mode: \n");
  printf("Enter Frequency in KHz: \n");
  scanf("%d", &frequency);
time = 1/(frequency*1000);
  sensor_data.input1=1;
  sensor_data.input2=time;

	printf("Periodic: setting mode\n");
	ioctl(fd2,SETMODE, &sensor_data);
	printf("Periodic: write(4)\n");
    buf=1;
    ret=write(fd2,&buf,sizeof(buf));
    if(ret < 0){
    	printf("write function failed\n");
    }
    sleep(2);

	printf("Periodic: read\n");
	ret=read(fd2,&obj_dis,sizeof(obj_dis));
   	if(ret < 0) {
    	printf(" Read function failed\n");
    }else{
    	obj_dis= obj_dis * (0.017);
		printf("calculated object distance is %llu centimeters\n",obj_dis);
    }

	printf("Periodic: write(0)\n");
	buf=0;
	ret=write(fd2,&buf,sizeof(buf));
   	if(ret < 0){
    	printf("write function failed\n");
    }
    sleep(2);
	printf("Periodic: read\n");
	ret=read(fd2,&obj_dis,sizeof(obj_dis));
    if(ret < 0){
    	printf(" Read function failed\n");
    }else{
    	obj_dis= obj_dis * (0.017);
		printf("calculated object distance is %llu centimeters\n",obj_dis);
    }
	  printf("Periodic: write(0)\n");
	  buf=0;
	  ret=write(fd2,&buf,sizeof(buf));
    if(ret < 0){
    	printf("write function failed\n");
    }
    sleep(2);

	  printf("Periodic: read\n");
	  ret=read(fd2,&obj_dis,sizeof(obj_dis));
    if(ret < 0){
    	printf(" Read function failed\n");
    }else{
    	obj_dis= obj_dis * (0.017);
		printf("calculated object distance is %llu centimeters\n",obj_dis);
    }
	  printf("Periodic: read\n");
	  ret=read(fd2,&obj_dis,sizeof(obj_dis));
    if(ret < 0){
    	printf(" Read function failed\n");
    }else{
    	obj_dis= obj_dis * (0.017);
		printf("calculated object distance is %llu centimeters\n",obj_dis);
    }
    printf("...close fd...\n");
 	  close(fd2);

	return 0;
}
