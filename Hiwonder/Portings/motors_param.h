/**
 * @file motors_param.h
 * @brief TANKBLACK 悬挂履带车 JGB37 编码器电机参数（减速比 45:1）
 *
 * ticks_per_circle = 11 × 4 × 45 = 1980 counts/输出轴圈
 */

#ifndef __MOTORS_PARAM_H
#define __MOTORS_PARAM_H
#include "encoder_motor.h"

typedef enum {
	MOTOR_TYPE_JGB37,
} MotorTypeEnum;

#define MOTOR_JGB37_TICKS_PER_CIRCLE 1980.0f
#define MOTOR_JGB37_PID_KP  40.0f
#define MOTOR_JGB37_PID_KI  2.0f
#define MOTOR_JGB37_PID_KD  2.0f
#define MOTOR_JGB37_RPS_LIMIT 3.0f

#define MOTOR_DEFAULT_TICKS_PER_CIRCLE MOTOR_JGB37_TICKS_PER_CIRCLE
#define MOTOR_DEFAULT_PID_KP  MOTOR_JGB37_PID_KP
#define MOTOR_DEFAULT_PID_KI  MOTOR_JGB37_PID_KI
#define MOTOR_DEFAULT_PID_KD  MOTOR_JGB37_PID_KD
#define MOTOR_DEFAULT_RPS_LIMIT MOTOR_JGB37_RPS_LIMIT

void set_motor_type(EncoderMotorObjectTypeDef *motor, MotorTypeEnum type);

#endif
