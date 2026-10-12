/*
 * @Author : LiuYang
 * @desc : globalfifo device driver
 * @date 2026-10-9
 * @contact : nickily233@gmail.com
 * @github : https://github.com/nickily233-svg
 */
#include "globalfifo.h"
#include <string.h>
/*
 * 局部变量
 */
static int globalfifo_major = GLOBALFIFO_MAJOR;
module_param(globalfifo_major, int, S_IRUGO);
/*
 * 实例化设备结构体
 */
struct globalfifo_dev *globalfifo_devp;
/*
 * 打开函数
 * @desc:当应用程序调用 open()
 * 打开设备文件时,该函数被内核调用,通常用于初始化设备的私有数据
 * @param1 inode:文件索引节点指针,包含设备号等关键信息
 * @param2 filp:文件结构体指针,用于保存设备的私有数据(将
 * globalfifo_devp赋值给filp->private_data)
 * @retval:成功时返回0,失败时返回负值(如-EBUSY等)
 */
static int global_open(struct inode *inode, struct file *filp) {
  // 设置私有数据
  struct globalfifo_dev *dev =
      container_of(inode->i_cdev, struct globalfifo_dev, cdev);
  filp->private_data = dev;
  return 0;
}
/*
 * 读函数
 * @desc  从设备中读取数据
 * @param1 struct file *filp : 文件结构体指针
 * @param2 char __user *buf : 用户空间内存地址
 * @param3 size_t count : 要读的字节数
 * @param4 loff_t *ppos : 读的位置相对于文件开头的偏移
 * @retval ssize_t : 成功时函数返回读取的字节数,出错时返回一个负数值
 */
static ssize_t globalfifo_read(struct file *filp, char __user *buf, size_t size,
                               loff_t *ppos) {
  loff_t p = *ppos;
  size_t count = size;
  int ret = 0;
  struct globalfifo_dev *dev = filp->private_data;

  // 在栈上静态声明并且初始化一个等待队列项(元素 wait_queueentry_t)
  // #define DECLARE_WAITQUEUE(name, tsk) \
  //   struct wait_queue_entry name = { \
  //       .private = tsk, \
  //       .func = default_wake_function, \
  //       .entry = LIST_HEAD_INIT((name).entry), \
  //   }
  //   name：你给这个等待队列项起的变量名
  //   tsk：通常传入 current（当前进程的 task_struct 指针）
  //   它将 private 设置为当前进程，并绑定默认的唤醒函数 default_wake_function
  DECLARE_WAITQUEUE(wait, current);
  mutex_lock(&globalfifo_devp->mutex);
  add_wait_queue(&dev->r_wait, &wait);

  while (dev->current_len == GLOBALFIFO_SIZE) {
    // 非阻塞 - 退出
    if (filp->f_flags & O_NONBLOCK) {
      ret = -EAGAIN;
      goto out;
    }
    // 阻塞 - 继续运行
    __set_current_state(TASK_INTERRUPTIBLE);

    mutex_unlock(&dev->mutex);
    schedule();
    if (signal_pending(current)) {
      ret = -ERESTARTSYS;
      goto out2;
    }
  }
  mutex_lock(&dev->mutex);

  if (count > GLOBALFIFO_SIZE - dev->current_len) {
    count = GLOBALFIFO_SIZE - dev->current_len;
  }

  // 读不全
  if (copy_to_user(buf, dev->mem, count)) {
    ret = -EFAULT;
    goto out;
  } else {
    memcpy(dev->mem, dev->mem + count, dev->current_len - count);
    dev->current_len -= count;
    printk(KERN_INFO "read %d bytes(s),current_len:%d\n", count,
           dev->current_len);
  }

  wake_up_interruptible(&dev->w_wait);
  ret = count;

out:
  mutex_unlock(&dev->mutex);
out2:
  remove_wait_queue(&dev->r_wait, &wait);
  set_current_state(TASK_RUNNING);
  return ret;
}
/*
 * 写函数
 * @desc  向设备写入数据
 * @param1 struct file *filp : 文件结构体指针
 * @param2 char __user *buf : 用户空间内存地址
 * @param3 size_t count : 要写的字节数
 * @param4 loff_t *ppos : 写的位置相对于文件开头的偏移
 * @retval ssize_t : 成功时函数返回写入的字节数
 */
static ssize_t globalfifo_write(struct file *filp, const char __user *buf,
                                size_t size, loff_t *ppos) {
  loff_t p = *ppos;
  size_t count = size;
  int ret = 0;
  struct globalfifo_dev *dev = filp->private_data;

  // 偏移位置超过定义内存的大小,表示已达到文件末尾,返回End-Of-File(EOF)
  if (p > GLOBALFIFO_SIZE) {
    printk("写入的数据超过globalfifo定义的内存大小");
    return 0;
  }
  // 如果要写的数据超过当前内存剩余数据的大小,只写剩余的部分
  if (count > GLOBALFIFO_SIZE - p) {
    count = GLOBALFIFO_SIZE - p;
  }

  mutex_lock(&globalfifo_devp->mutex);
  if (copy_from_user(dev->mem + p, buf, count)) {
    return -EFAULT;
  } else {
    *ppos += count;
    ret = count;

    printk(KERN_INFO "written %lu bytes(s) from %llu\n", count, p);
  }

  mutex_unlock(&globalfifo_devp->mutex);

  return ret;
}
/*
 * 查找函数(修改文件的当前读写位置)
 * @desc: 用于修改文件的当前读写位置,并将新位置返回
 * @param1 filp:文件结构体指针,包含文件的私有数据
 * @param2 offset:相对于基准点的偏移值
 * @param3 orig:基准点(SEEK_SET:文件头,SEEK_CUR:当前位置,SEEK_END:文件尾)
 * @retval:成功时返回新的文件偏移量,失败时返回负值(如-EINVAL)
 */
static loff_t globalfifo_llseek(struct file *filp, loff_t offset, int orig) {
  loff_t ret = 0;
  switch (orig) {
  case 0: /* 从文件头位置开始 */
    if (offset < 0 || offset > GLOBALFIFO_SIZE) {
      ret = -EINVAL;
      break;
    }
    filp->f_pos = offset;
    ret = filp->f_pos;
    break;
  case 1: /* 从文件当前位置开始查找 */
    if ((filp->f_pos + offset) > GLOBALFIFO_SIZE) {
      ret = -EINVAL;
      break;
    }
    if ((filp->f_pos + offset) < 0) {
      ret = -EINVAL;
      break;
    }
    filp->f_pos += offset;
    ret = filp->f_pos;
    break;
  case 2: /* 从文件尾开始查找 */
    if ((GLOBALFIFO_SIZE + offset) < 0) {
      ret = -EINVAL;
      break;
    }
    if ((GLOBALFIFO_SIZE + offset) > GLOBALFIFO_SIZE) {
      ret = -EINVAL;
      break;
    }
    filp->f_pos = offset + GLOBALFIFO_SIZE;
    ret = filp->f_pos;
    break;
  default:
    ret = -EINVAL;
    break;
  }
  return ret;
}
/*
 * 释放函数
 * @desc: 当应用程序调用 close() 关闭设备文件时, 该函数被内核调用,
 * 通常用于释放资源或清理私有数据
 * @param1 inode: 文件索引节点指针, 包含设备号等关键信息
 * @param2 filp: 文件结构体指针, 包含设备的私有数据
 * @retval: 成功时返回 0, 失败时返回负值
 */
static int global_release(struct inode *inode, struct file *filp) { return 0; }
/*
 * IOCTL 控制函数
 * @desc: 当应用程序调用 ioctl() 时,
 * 该函数被内核调用,用于处理用户空间发出的控制命令
 * @param1 filp: 文件结构体指针, 包含设备的私有数据
 * @param2 cmd: 用户空间传递下来的控制命令
 * @param3 arg: 与命令对应的参数
 * @retval: 成功时返回 0, 失败时返回负值(如 -EINVAL)
 */
static long global_ioctl(struct file *filp, unsigned int cmd,
                         unsigned long arg) {
  struct globalfifo_dev *dev = filp->private_data;

  switch (cmd) {
  case MEM_CLEAR:
    mutex_lock(&globalfifo_devp->mutex);
    memset(dev->mem, 0, GLOBALFIFO_SIZE);
    mutex_unlock(&globalfifo_devp->mutex);
    printk("globalfifo被全部设置为 0");
    break;
  default:
    return -EINVAL;
  }

  return 0;
}
/*
 * 操作函数集合结构体
 * VFS 与设备驱动的桥梁
 */
struct file_operations globalfifo_fops = {
    .owner = THIS_MODULE,
    .read = globalfifo_read,
    .write = globalfifo_write,
    .llseek = globalfifo_llseek,
    .unlocked_ioctl = global_ioctl,
    .open = global_open,
    .release = global_release,
};
/*
 * 初始化/添加 cdev
 * @param1 struct globalfifo_dev *dev : 设备结构体
 * @param2 int index : 索引
 * @retval void : 空
 */
static void golbalmem_setup_dev(struct globalfifo_dev *dev, int index) {
  int err;
  dev_t devno = MKDEV(globalfifo_major, index);

  // 初始化 cdev 并绑定操作函数集
  cdev_init(&dev->cdev, &globalfifo_fops);
  dev->cdev.owner = THIS_MODULE;
  // 添加cdev
  err = cdev_add(&dev->cdev, devno, 1);
  if (err) {
    printk(KERN_ERR "Error %d adding globalfifo%d", err, index);
    return;
  }
}
/*
 * 入口函数
 * @param1 void
 * @retval int : 0-加载成功 负数-加载失败
 */
static int __init globalfifo_init(void) {
  int ret;
  int i;
  dev_t devno = MKDEV(GLOBALFIFO_MAJOR, 0);

  if (globalfifo_major) {
    // 静态分配设备号
    ret = register_chrdev_region(devno, DEVNUM, "globalfifo");
  } else {
    // 动态分配设备号
    ret = alloc_chrdev_region(&devno, 0, DEVNUM, "globalfifo");
    globalfifo_major = MAJOR(devno);
  }
  // 分配设备号错误
  if (ret < 0) {
    return ret;
  }
  // 分配内存
  globalfifo_devp = kzalloc(sizeof(struct globalfifo_dev) * DEVNUM, GFP_KERNEL);
  if (!globalfifo_devp) {
    ret = -ENOMEM;
    goto fail_malloc;
  }

  for (i = 0; i < DEVNUM; i++) {
    golbalmem_setup_dev(globalfifo_devp + i, i);
  }

  mutex_init(&globalfifo_devp->mutex);
  init_waitqueue_head(&globalfifo_devp->r_wait);
  init_waitqueue_head(&globalfifo_devp->w_wait);

  return 0;

fail_malloc:
  unregister_chrdev_region(devno, DEVNUM);
  return ret;
}
module_init(globalfifo_init);
/*
 * 出口函数
 * @param1 void
 * @retval void : 空
 */
static void __exit globalfifo_exit(void) {
  int i;
  for (i = 0; i < DEVNUM; i++) {
    cdev_del(&(globalfifo_devp + i)->cdev);
  }
  kfree(globalfifo_devp);
  unregister_chrdev_region(MKDEV(globalfifo_major, 0), 1);
}
module_exit(globalfifo_exit);

MODULE_AUTHOR("LiuYang@nickily233@gmail.com");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("v1.0");
MODULE_DESCRIPTION("globalfifo virtual device driver");