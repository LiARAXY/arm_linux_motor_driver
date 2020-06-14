#ifndef __STEPMOTOR_DRV_H__
#define __STEPMOTOR_DRV_H__

#define DTS_COMPATIBLE "fsl,my_stepMotor_drv"
#define drvName "myStepMotor"
#define drvCNT  1


#define VAL_BREAK 0xf
#define VAL_STOP  0x0
#define DELAY_MS  10
#define TIMER_DEVIATION 1000
#define TIMER_UNIT_MS 1000 
const char val_step[8]= {0x2,0x3,0x1,0x9,0x8,0xc,0x4,0x6};

#endif
