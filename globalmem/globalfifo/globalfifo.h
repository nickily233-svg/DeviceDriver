/*
 * @Author : LiuYang
 * @desc : globalfifo device driver
 * @date 2026-10-9
 * @contact : nickily233@gmail.com
 * @github : https://github.com/nickily233-svg
 */
#ifndef globalfifo_H
#define globalfifo_H

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

/*
 * 宏定义
 */
#define GLOBALFIFO_SIZE 0x1000
#define MEM_CLEAR 0x1
#define GLOBALFIFO_MAJOR 230
#define DEVNUM 10

/*
 * 设备结构体
 */
struct globalfifo_dev {
  struct cdev cdev;
  unsigned char mem[GLOBALFIFO_SIZE];
  struct mutex mutex;
  wait_queue_head_t r_wait;
  wait_queue_head_t w_wait;
  unsigned int current_len; // 有效数据长度
};

#endif // globalfifo_H