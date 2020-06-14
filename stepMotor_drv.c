#include <linux/module.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/kmod.h>
#include <linux/gfp.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/param.h>
#include <linux/kthread.h>
#include <linux/err.h>

#include "stepMotor.h"
#include "stepMotor_drv.h"

struct stepMotor_dev
{
    dev_t devid;              /* 设备号 */
    struct cdev cdev;         /* cdev */
    struct class *class;      /* 类 */
    struct device *device;    /* 设备 */
    struct device_node *nd;   /* 设备节点 */
    int major;                /* 主设备号 */

    unsigned int step;
    unsigned long timer_expires_offset;
    struct gpio_desc *ch1_a;
    struct gpio_desc *ch1_b;
    struct gpio_desc *ch2_a;
    struct gpio_desc *ch2_b;

};

static struct task_struct *stepMotor_task;
static struct stepMotor_dev stepMotorDev;
static stepMotor_workmode *mode;
struct timer_list stepMotor_timer;

static void stepMotor_ctrl(char i)
{
//  printk( KERN_DEBUG "stepMotor_step %d!\n",i);
    gpiod_set_value( stepMotorDev.ch1_a, (i >> 0) & 0x1 );
    gpiod_set_value( stepMotorDev.ch1_b, (i >> 1) & 0x1 );
    gpiod_set_value( stepMotorDev.ch2_a, (i >> 2) & 0x1 );
    gpiod_set_value( stepMotorDev.ch2_b, (i >> 3) & 0x1 );
//  printk(KERN_DEBUG"val = %d %d %d %d \n",((i >> 0)&0x1), ((i >> 1)&0x1),((i >> 2)&0x1),((i >> 3)&0x1) );
}
static void stepMotor_stop(void)
{
//  printk( KERN_DEBUG "stepMotor stop!\n");
    stepMotor_ctrl(VAL_STOP);
}
static void stepMotor_break(void)
{
//  printk( KERN_DEBUG "stepMotor break!\n");
    stepMotor_ctrl(VAL_BREAK);
}
static void stepMotor_timer_handler(unsigned long data)
{
    kthread_unpark(stepMotor_task);     /* unpark线程 */
}

static int stepMotor_Run(void)
{
    if(stepMotorDev.step > 8 )                            return -1;            /* 判断节拍是否有效 */
    if(mode == NULL)                                      return -1;            /* 判断mode指针是否有效 */
    if(mode->stepInterval < 2)                            return -1;            /* 判断节拍间隔设置是否有效 */
    if(stepMotorDev.timer_expires_offset <= 0)            return -1;            /* 判断定时器偏移的值是否有效 */
    if(mode->direction != 0 && mode->direction != 1)      return -1;            /* 判断方向设置是否有效 */
    else if(mode->direction == 0) stepMotor_ctrl(val_step[stepMotorDev.step]);      /* 正转 */
    else                          stepMotor_ctrl(val_step[8 - stepMotorDev.step]);  /* 反转 */
    stepMotorDev.step++;
    if(stepMotorDev.step > 8) stepMotorDev.step = 0;
    /* 修改定时器时间并运行 */
    init_timer(&stepMotor_timer);
    stepMotor_timer.function = stepMotor_timer_handler;
    stepMotor_timer.data = 0;
    stepMotor_timer.expires = jiffies + stepMotorDev.timer_expires_offset  ;
    add_timer(&stepMotor_timer);
    /* 线程park标记 */
    kthread_park(stepMotor_task);
    return 0;
}

static int stepMotor_thread(void *data)
{
    while(1)
    {
        stepMotor_Run();
        if(kthread_should_park()) kthread_parkme(); 
        if(kthread_should_stop())  
        {
          stepMotor_stop();
          break;
        }
    }
    return 0;
}

/*
  * @description    : 从设备读取数据
  * @param - filp   : 要打开的设备文件(文件描述符)
  * @param - buf    : 返回给用户空间的数据缓冲区
  * @param - cnt    : 要读取的数据长度
  * @param - offt   : 相对于文件首地址的偏移
  * @return         : 读取的字节数，如果为负值，表示读取失败
  */
static ssize_t stepMotor_read(struct file *filp,char __user *buf,size_t cnt, loff_t *off)
{ 
    int err = 0;
    return err;
}

static ssize_t stepMotor_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
    int err;
    if(mode == NULL) 
    {
      printk(KERN_DEBUG"mode is NULL!\n"); 
      return -1; 
    }
    else 
    {
      err = copy_from_user(mode,buf,sizeof(stepMotor_workmode));
      if(!err) printk(KERN_ERR"%d Byte wait to copy!\n",err);
      stepMotorDev.timer_expires_offset = (((HZ*mode->stepInterval)*timer_deviation)/(1000*timer_deviation));
      return err;
    }
}


/*
 * @description     : 打开设备
 * @param – inode  : 传递给驱动的inode
 * @param - filp   : 设备文件，file结构体有个叫做pr似有ate_data的成员变量
 *                     一般在open的时候将private_data指向设备结构体。
 * @return           :0 成功;其他失败
 */
static int stepMotor_open(struct inode *inode,struct file *filp)
{
    printk(KERN_DEBUG"stepMotor_open func! \n");
    if(mode == NULL)
    {
        mode = kzalloc(sizeof(stepMotor_workmode),GFP_KERNEL);
        mode->stepInterval = 0;
        mode->direction    = 0xf;
    }
    else printk(KERN_DEBUG"pointer of mode addr = 0x%d",(unsigned int)mode);
    stepMotorDev.step = 0;
    /* 创建并运行内核线程 */
    stepMotor_task = kthread_create(stepMotor_thread,NULL,"stepMotor");
    wake_up_process(stepMotor_task);
    return 0;
}

/*
  * @description    : 关闭/释放设备
  * @param - filp   : 要关闭的设备文件(文件描述符)
  * @return         : 0 成功;其他失败
  */
static int stepMotor_release(struct inode *inode,struct file *filp)
{
    if(mode != NULL) kfree(mode);
    mode = NULL;
    stepMotorDev.timer_expires_offset = 0;
    del_timer_sync(&stepMotor_timer); /* 确保定时器删除 */
    stepMotor_break();                /* 刹车模式 */
    kthread_stop(stepMotor_task);     /* 内核线程退出 */
    printk(KERN_DEBUG"stepMotor release!\n");
    return 0;
}


static const struct file_operations stepMotor_ops ={
    .owner   = THIS_MODULE,
    .open    = stepMotor_open,
    .read    = stepMotor_read,
    .write   = stepMotor_write,
    .release = stepMotor_release,
};

static int stepMotor_ProbErr(unsigned int cnt)
{   
    if(cnt >6) return -1;
    if(cnt >= 0)                                                       /* 删除设备 */
    {
        cdev_del(&stepMotorDev.cdev);
        unregister_chrdev_region(stepMotorDev.devid, drvCNT);
    }
    if(cnt >= 1)device_destroy(stepMotorDev.class, stepMotorDev.devid);/* 注销掉类和设备 */
    if(cnt >= 2)class_destroy(stepMotorDev.class);
    if(cnt >= 3)gpiod_put(stepMotorDev.ch1_a);                         /* 释放gpio */
    if(cnt >= 4)gpiod_put(stepMotorDev.ch1_b);
    if(cnt >= 5)gpiod_put(stepMotorDev.ch2_a);
    if(cnt >= 6)gpiod_put(stepMotorDev.ch2_b);
    return 0;
}

static int stepMotor_probe(struct platform_device *pdev)
{
    
    int err;
    /* 1、构建设备号 */
    if(stepMotorDev.major)
    {
        stepMotorDev.devid = MKDEV(stepMotorDev.major,0);
        register_chrdev_region(stepMotorDev.devid, drvCNT,drvName);
    }
    else
    {
        alloc_chrdev_region(&stepMotorDev.devid,0, drvCNT,drvName);
        stepMotorDev.major = MAJOR(stepMotorDev.devid);
    }
    /* 2、注册设备 */
    cdev_init(&stepMotorDev.cdev,&stepMotor_ops);
    err = cdev_add(&stepMotorDev.cdev, stepMotorDev.devid, drvCNT);
    if(!err)
    {
        stepMotor_ProbErr(0);
        return PTR_ERR(stepMotorDev.class);
    }  
    /* 3、创建类 */
    stepMotorDev.class = class_create(THIS_MODULE, drvName);
    if(IS_ERR(stepMotorDev.class))
    {
        stepMotor_ProbErr(1);
        return PTR_ERR(stepMotorDev.class);
    }    
    /* 4、创建设备 */
    stepMotorDev.device = device_create(stepMotorDev.class,NULL,stepMotorDev.devid,NULL,"myMotor");
    if(IS_ERR(stepMotorDev.device))
    {
        stepMotor_ProbErr(2);
        return PTR_ERR(stepMotorDev.device);
    }
#if 0  //使用gpios子系统 不需要获取节点
     /* 获取设备树节点 */
    stepMotorDev.nd = of_find_compatible_node(NULL,NULL, "fsl,my_stepMotor_drv");
    if(stepMotorDev.nd == NULL)
    {
        printk( KERN_DEBUG "mySTEPmotor node not found!\n");
        return -EINVAL;
    }   
#endif
    /* 获取设备树中的gpio 需要在设备树节点中定义xxx-gpios*/
    stepMotorDev.ch1_a = gpiod_get(&pdev->dev, "ch1_a", 0);
    if (IS_ERR(stepMotorDev.ch1_a)) {		dev_err(&pdev->dev, "Failed to get GPIO for ch1_a\n");	stepMotor_ProbErr(3);	return PTR_ERR(stepMotorDev.ch1_a);	}
    stepMotorDev.ch1_b = gpiod_get(&pdev->dev, "ch1_b", 0);        
    if (IS_ERR(stepMotorDev.ch1_b)) {		dev_err(&pdev->dev, "Failed to get GPIO for ch1_b\n");	stepMotor_ProbErr(4);	return PTR_ERR(stepMotorDev.ch1_b);	}
    stepMotorDev.ch2_a = gpiod_get(&pdev->dev, "ch2_a", 0);
    if (IS_ERR(stepMotorDev.ch2_a)) {		dev_err(&pdev->dev, "Failed to get GPIO for ch2_a\n");	stepMotor_ProbErr(5);	return PTR_ERR(stepMotorDev.ch2_a);	}
    stepMotorDev.ch2_b = gpiod_get(&pdev->dev, "ch2_b", 0);
    if (IS_ERR(stepMotorDev.ch2_b)) {	    dev_err(&pdev->dev, "Failed to get GPIO for ch2_b\n");	stepMotor_ProbErr(6);   return PTR_ERR(stepMotorDev.ch2_b); }
    
    /* 设置GPIO模式 */
    err = gpiod_direction_output(stepMotorDev.ch1_a,0);
    if(err <0)  printk( KERN_ERR "can't set mode ch1_a!\n");
    err = gpiod_direction_output(stepMotorDev.ch1_b,0);
    if(err <0)  printk( KERN_ERR "can't set mode ch1_b!\n");
    err = gpiod_direction_output(stepMotorDev.ch2_a,0);
    if(err <0)  printk( KERN_ERR "can't set mode ch2_a!\n");
    err = gpiod_direction_output(stepMotorDev.ch2_b,0);
    if(err <0)  printk( KERN_ERR "can't set mode ch2_b!\n");
    /* 将电机设置为待机状态 */
    stepMotor_stop();
    /* 设置指针 */
    mode = NULL;
    return 0;
}

static int stepMotor_remove(struct platform_device *pdev)
{
    stepMotor_ProbErr(6);
    return 0;
}

/* 设备树匹配列表 */
static const struct of_device_id stepMotor_of_match[]={
    {.compatible ="fsl,my_stepMotor_drv"},
    {}
};

/* platform_driver */
static struct platform_driver stepMotor_pdev = {
    .probe      = stepMotor_probe,
    .remove     = stepMotor_remove,
    .driver     = {
        .name   = "my_stepMotor_drv",
        .of_match_table = stepMotor_of_match,
    },
};

/* 驱动入口函数 */
static int __init stepMotor_init(void)
{
    int err;
    err = platform_driver_register(&stepMotor_pdev); 
    if(err) printk(KERN_ERR"Platform device register failed!\n");
    return err;
}

/* 驱动出口函数 */
static void __exit stepMotor_exit(void)
{
  platform_driver_unregister(&stepMotor_pdev);
}

module_init(stepMotor_init);
module_exit(stepMotor_exit);
MODULE_LICENSE("GPL");


