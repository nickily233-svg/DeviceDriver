/*
 * @Author : LiuYang
 * @desc : globalmem device driver
 * @date 2026-10-9
 * @contact : nickily233@gmail.com
 * @github : https://github.com/nickily233-svg
 */
#include "globalmem.h"
/*
 * 局部变量
 */
static int globalmem_major = GLOBALMEM_MAJOR;
module_param(globalmem_major, int, S_IRUGO);
/*
 * 实例化设备结构体
 */
struct globalmem_dev *globalmem_devp;
/*
 * 读函数
 * @desc  从设备中读取数据
 * @param1 struct file *filp : 文件结构体指针
 * @param2 char __user *buf : 用户空间内存地址
 * @param3 size_t count : 要读的字节数
 * @param4 loff_t *ppos : 读的位置相对于文件开头的偏移
 * @retval ssize_t : 成功时函数返回读取的字节数,出错时返回一个负数值
 */
static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t size,
                              loff_t *ppos) {
  loff_t p = *ppos;
  size_t count = size;
  int ret = 0;
  struct globalmem_dev *dev = filp->private_data;

  // 偏移位置超过定义内存的大小,表示已达到文件末尾,返回End-Of-File(EOF)
  if (p >= GLOBALMEM_SIZE) {
    printk("读取的数据超过globalmem定义的内存大小");
    return 0;
  }
  // 读取0字节,直接返回
  if (count == 0) {
    return 0;
  }
  // 如果要读的数据超过当前内存剩余数据的大小,只读剩余的部分
  if (count > GLOBALMEM_SIZE - p) {
    count = GLOBALMEM_SIZE - p;
  }

  if (copy_to_user(buf, dev->mem + p, count)) {
    ret = -EFAULT;
  } else {
    *ppos += count;
    ret = count;

    printk(KERN_INFO "read %lu bytes(s) from %llu\n", count, p);
  }

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
static ssize_t globalmem_write(struct file *filp, const char __user *buf,
                               size_t size, loff_t *ppos) {
  loff_t p = *ppos;
  size_t count = size;
  int ret = 0;
  struct globalmem_dev *dev = filp->private_data;

  // 偏移位置超过定义内存的大小,表示已达到文件末尾,返回End-Of-File(EOF)
  if (p > GLOBALMEM_SIZE) {
    printk("写入的数据超过globalmem定义的内存大小");
    return 0;
  }
  // 如果要写的数据超过当前内存剩余数据的大小,只写剩余的部分
  if (count > GLOBALMEM_SIZE - p) {
    count = GLOBALMEM_SIZE - p;
  }

  if (copy_from_user(dev->mem + p, buf, count)) {
    return -EFAULT;
  } else {
    *ppos += count;
    ret = count;

    printk(KERN_INFO "written %lu bytes(s) from %llu\n", count, p);
  }

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
static loff_t globalmem_llseek(struct file *filp, loff_t offset, int orig) {
  loff_t ret = 0;
  switch (orig) {
  case 0: /* 从文件头位置开始 */
    if (offset < 0 || offset > GLOBALMEM_SIZE) {
      ret = -EINVAL;
      break;
    }
    filp->f_pos = offset;
    ret = filp->f_pos;
    break;
  case 1: /* 从文件当前位置开始查找 */
    if ((filp->f_pos + offset) > GLOBALMEM_SIZE) {
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
    if ((GLOBALMEM_SIZE + offset) < 0) {
      ret = -EINVAL;
      break;
    }
    if ((GLOBALMEM_SIZE + offset) > GLOBALMEM_SIZE) {
      ret = -EINVAL;
      break;
    }
    filp->f_pos = offset + GLOBALMEM_SIZE;
    ret = filp->f_pos;
    break;
  default:
    ret = -EINVAL;
    break;
  }
  return ret;
}
/*
 * 打开函数
 * @desc:当应用程序调用 open()
 * 打开设备文件时,该函数被内核调用,通常用于初始化设备的私有数据
 * @param1 inode:文件索引节点指针,包含设备号等关键信息
 * @param2 filp:文件结构体指针,用于保存设备的私有数据(将
 * globalmem_devp赋值给filp->private_data)
 * @retval:成功时返回0,失败时返回负值(如-EBUSY等)
 */
static int global_open(struct inode *inode, struct file *filp) {
  filp->private_data = globalmem_devp;
  return 0;
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
  struct globalmem_dev *dev = filp->private_data;

  switch (cmd) {
  case MEM_CLEAR:
    memset(dev->mem, 0, GLOBALMEM_SIZE);
    printk("globalmem被全部设置为 0");
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
struct file_operations globalmem_fops = {
    .owner = THIS_MODULE,
    .read = globalmem_read,
    .write = globalmem_write,
    .llseek = globalmem_llseek,
    .unlocked_ioctl = global_ioctl,
    .open = global_open,
    .release = global_release,
};
/*
 * 初始化/添加 cdev
 * @param1 struct globalmem_dev *dev : 设备结构体
 * @param2 int index : 索引
 * @retval void : 空
 */
static void golbalmem_setup_dev(struct globalmem_dev *dev, int index) {
  int err;
  dev_t devno = MKDEV(globalmem_major, index);

  // 初始化 cdev 并绑定操作函数集
  cdev_init(&dev->cdev, &globalmem_fops);
  dev->cdev.owner = THIS_MODULE;
  // 添加cdev
  err = cdev_add(&dev->cdev, devno, 1);
  if (err) {
    printk(KERN_ERR "Error %d adding globalmem%d", err, index);
    return;
  }
}
/*
 * 入口函数
 * @param1 void
 * @retval int : 0-加载成功 负数-加载失败
 */
static int __init globalmem_init(void) {
  int ret;
  dev_t devno = MKDEV(GLOBALMEM_MAJOR, 0);

  if (globalmem_major) {
    // 静态分配设备号
    ret = register_chrdev_region(devno, 1, "globalmem");
  } else {
    // 动态分配设备号
    ret = alloc_chrdev_region(&devno, 0, 1, "globalmem");
    globalmem_major = MAJOR(devno);
  }
  // 分配设备号错误
  if (ret < 0) {
    return ret;
  }
  // 分配内存
  globalmem_devp = kzalloc(sizeof(struct globalmem_dev), GFP_KERNEL);
  if (!globalmem_devp) {
    ret = -ENOMEM;
    goto fail_malloc;
  }

  golbalmem_setup_dev(globalmem_devp, 0);
  return 0;

fail_malloc:
  unregister_chrdev_region(devno, 1);
  return ret;
}
module_init(globalmem_init);
/*
 * 出口函数
 * @param1 void
 * @retval void : 空
 */
static void __exit globalmem_exit(void) {
  cdev_del(&globalmem_devp->cdev);
  kfree(globalmem_devp);
  unregister_chrdev_region(MKDEV(globalmem_major, 0), 1);
}
module_exit(globalmem_exit);

MODULE_AUTHOR("LiuYang@nickily233@gmail.com");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("v1.0");
MODULE_DESCRIPTION("globalmem virtual device driver");