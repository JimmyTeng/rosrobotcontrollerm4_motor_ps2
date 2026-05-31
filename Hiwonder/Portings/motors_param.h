/**
 * @file motors_param.h
 * @brief 各型号霍尔编码器电机的减速比、每圈计数与 PID 参数
 *
 * ticks_per_circle：输出轴转一圈时，定时器编码器累计的计数值（含 AB 相 4 倍频）。
 * 计算公式：磁环线数 × 4 × 减速比 = ticks_per_circle。
 *
 * set_motor_type() 在 chassis_porting.c 的 set_chassis_type() 里按底盘型号切换；
 * motors_init() 完成前默认使用 MOTOR_DEFAULT（同 JGB520）。
 */

#ifndef __MOTORS_PARAM_H
#define __MOTORS_PARAM_H
#include "encoder_motor.h"

/** 电机型号枚举，与幻尔 JGB/JGA 系列减速电机对应 */
typedef enum {
	MOTOR_TYPE_JGB520,  /**< 减速比 90:1，JetAuto / JetAcker 等 */
	MOTOR_TYPE_JGB37,   /**< 减速比 45:1，悬挂履带 TANKBLACK（520 电机手册） */
	MOTOR_TYPE_JGA27,   /**< 减速比 20:1，TI4WD */
	MOTOR_TYPE_JGB528,  /**< 减速比 131:1，JetTank */
} MotorTypeEnum;

/* ---------- JGB520：减速比 90:1 ---------- */
/* 11 脉冲/圈 × AB 四倍频 × 90 = 3960 counts/输出轴圈 */
#define MOTOR_JGB520_TICKS_PER_CIRCLE 3960.0f
#define MOTOR_JGB520_PID_KP  63.0f
#define MOTOR_JGB520_PID_KI  2.6f
#define MOTOR_JGB520_PID_KD  2.4f
#define MOTOR_JGB520_RPS_LIMIT 1.5f   /* 输出轴转速上限，rps */

/* ---------- JGB37：减速比 45:1（CHASSIS_TYPE_TANKBLACK） ---------- */
/* 11 × 4 × 45 = 1980；与《悬挂式履带车》520 编码器电机手册 45:1 一致 */
#define MOTOR_JGB37_TICKS_PER_CIRCLE 1980.0f
#define MOTOR_JGB37_PID_KP  40.0f
#define MOTOR_JGB37_PID_KI  2.0f
#define MOTOR_JGB37_PID_KD  2.0f
#define MOTOR_JGB37_RPS_LIMIT 3.0f

/* ---------- JGA27：减速比 20:1（CHASSIS_TYPE_TI4WD） ---------- */
/* 13 脉冲/圈 × 4 × 20 = 1040 */
#define MOTOR_JGA27_TICKS_PER_CIRCLE 1040.0f
#define MOTOR_JGA27_PID_KP  -36.0f
#define MOTOR_JGA27_PID_KI  -1.0f
#define MOTOR_JGA27_PID_KD  -1.0f
#define MOTOR_JGA27_RPS_LIMIT 6.0f

/* ---------- JGB528：减速比 131:1（CHASSIS_TYPE_JETTANK） ---------- */
/* 11 × 4 × 131 = 5764 */
#define MOTOR_JGB528_TICKS_PER_CIRCLE 5764.0f
#define MOTOR_JGB528_PID_KP  300.0f
#define MOTOR_JGB528_PID_KI  2.0f
#define MOTOR_JGB528_PID_KD  12.0f
#define MOTOR_JGB528_RPS_LIMIT 1.1f

/* ---------- 默认参数（motors_init 至 set_chassis_type 之前） ---------- */
/* 与 JGB520 相同：90:1 / 3960 */
#define MOTOR_DEFAULT_TICKS_PER_CIRCLE 3960.0f
#define MOTOR_DEFAULT_PID_KP  63.0f
#define MOTOR_DEFAULT_PID_KI  2.6f
#define MOTOR_DEFAULT_PID_KD  2.4f
#define MOTOR_DEFAULT_RPS_LIMIT 1.35f

/** 按型号写入 ticks_per_circle、PID 与 rps 限制 */
void set_motor_type(EncoderMotorObjectTypeDef *motor, MotorTypeEnum type);

#endif
