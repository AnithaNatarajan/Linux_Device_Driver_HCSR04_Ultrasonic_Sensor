#ifndef HCSR04_IOCTL_H
#define HCSR04_IOCTL_H
#include <linux/ioctl.h>

#define MAGIC_NO 1238975
typedef struct {
  int input1;
  int input2;
  int input3;
  int input4;
  int input5;
  int input6;
  int input7;
  int input8;
} set_ioctl;

#define SETPIN _IOWR(MAGIC_NO, 1, set_ioctl*)
#define SETMODE _IOWR(MAGIC_NO, 2, set_ioctl*)
#endif
