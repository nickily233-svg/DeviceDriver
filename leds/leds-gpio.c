/**
 *
 * @file led-gpio.c
 * @brief dirver for led-gpio
 * @date 2026-10-2
 * @version 1.0
 *
 **/
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <stdio.h>

#ifndef LIGHT_MAJOR
#define LIGHT_MAJOR = 240
#endif

/**
 * @brief express for a led-gpio struct
 */
struct light_dev {

    struct cdev cdev;
    unsigned char value;

};

struct light_dev *light_devp;
int light_major = LIGHT_MAJOR;

MODULE_AUTHOR("LiuYang nickily@gmial.com");
MODULE_LICENSE("GPL v2");
