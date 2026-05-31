/**
 * @file chassis.h
 * @brief 悬挂履带 TANKBLACK 几何参数（vx/角速度 → 电机 rps）
 */

#ifndef __CHASSIS_H_
#define __CHASSIS_H_
#include <stdbool.h>

typedef enum {
	CHASSIS_TYPE_TANKBLACK,
} ChassisTypeEnum;

typedef struct {
	ChassisTypeEnum chassis_type;
	void (*set_velocity)(void *self, float vx, float vy, float angular_rate);
	void (*set_velocity_radius)(void *self, float linear, float r, bool insitu);
	void (*stop)(void *self);
} ChassisTypeDef;

#define TANKBLACK_WHEEL_DIAMETER 54.0f   /* mm */
#define TANKBLACK_CORRECITION_FACTOR 1.0f
#define TANKBLACK_SHAFT_LENGTH 152.8f    /* mm，左右驱动轮中心距 */

#endif
