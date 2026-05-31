#ifndef __GLOBAL_CONF_H
#define __GLOBAL_CONF_H

#define ENABLE_DEBUG_UART    1              /* 1: printf/LOG -> USART1 115200 (rrc_flash) */
#define DEBUG_UART_TELEM_STATS 1            /* 1: 每秒打印 V2 遥测发送统计（仅 USART1） */
#define DEBUG_BATTERY_ADC_RAW  0            /* 1: 每 50ms 打印 ADC raw（刷屏，仅排查电池时开） */

#define ENABLE_IMU  1                     /* IMU 任务是否启动 */
#define ENABLE_LVGL 0                     /* LVGL 任务是否启动 */
#define ENABLE_BLUETOOTH                1 /* 蓝牙是否开启 */
#define ENABLE_BLUETOOTH_BATTERY_REPORT 1 /* 蓝牙电压报告是否开启 */
#define ENABLE_BATTERY_LOW_ALARM        0 /* 低电压报警是否开启 */


#define BATTERY_LOW_ALARM_THRESHOLD 6300  /* 低电压报警阈值, 单位毫伏 */
//#define BATTERY_LOW_ALARM_THRESHOLD 9500


#define KEY1_PUSHED_LEVEL 0
#define KEY2_PUSHED_LEVEL 0
#define LED_SYS_LEVEL_ON  0

#define LED_TASK_PERIOD     30u /* LED状态刷新间隔 */
#define BUZZER_TASK_PERIOD  30u /* 蜂鸣器状态刷新间隔 */
#define BUTTON_TASK_PERIOD  30u /* 板载按键扫描间隔 */
#define BATTERY_TASK_PERIOD 50u /* 电池电量检测间隔 */

#define USE_PACKET_V2 1           /* 1: COBS + 逻辑帧 V2；0: V1 AA55 */

#endif

