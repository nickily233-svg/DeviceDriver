/*
 * @Author : LiuYang
 * @desc : globalmem device driver
 * @date 2026-10-9
 * @contact : nickily233@gmail.com
 * @github : https://github.com/nickily233-svg
 */
#ifndef GLOBALMEM_H
#define GLOBALMEM_H

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/uaccess.h>

/*
 * 宏定义
 */
#define GLOBALMEM_SIZE 0x1000
#define MEM_CLEAR 0x1
#define GLOBALMEM_MAJOR 230
#define DEVNUM 10

/*
 * 设备结构体
 */
struct globalmem_dev {
  struct cdev cdev;
  unsigned char mem[GLOBALMEM_SIZE];
};

#endif // GLOBALMEM_H