#ifndef __STEPMOTOR_H__
#define __STEPMOTOR_H__

#define drvName "myStepMotor"
typedef struct 
{
    unsigned char direction;        /* 0为正转 1为反转 */
    unsigned long stepInterval;     /* 每个节拍之间的时间间隔 单位是毫秒 */
}stepMotor_workmode;

#endif