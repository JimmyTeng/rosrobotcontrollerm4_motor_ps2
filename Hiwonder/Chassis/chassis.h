/**
 * @file chassis.h
 * @brief 各款底盘几何参数与类型枚举（用于 vx/角速度 → 电机 rps 换算）
 *
 * 差速底盘（jetank / tank_black / ti4wd）在 differential_chassis.c 中使用：
 *   - wheel_diameter：等效滚动直径，线速度(mm/s) 转输出轴 rps
 *   - shaft_length：左右驱动轮中心距（轮距），转弯差速用
 *   - correction_factor：里程/速度标定系数，默认 1.0
 *
 * 麦轮/阿克曼另用 wheelbase 等字段，见对应 chassis 实现。
 */

#ifndef __CHASSIS_H_
#define __CHASSIS_H_
#include <stdbool.h>

/** 底盘型号，与 set_chassis_type()、chassis_porting.c 中实例对应 */
typedef enum {
	CHASSIS_TYPE_JETAUTO,    /**< 麦轮小车 JetAuto */
	CHASSIS_TYPE_JETTANK,    /**< 履带/坦克 JetTank */
	CHASSIS_TYPE_JETACKER,   /**< 阿克曼 JetAcker */
	CHASSIS_TYPE_TI4WD,      /**< 四轮差速 TI4WD */
	CHASSIS_TYPE_TANKBLACK,  /**< 悬挂式黑履带车（本项目 app 默认） */
}ChassisTypeEnum;

typedef struct {
	ChassisTypeEnum chassis_type;
	void (*set_velocity)(void *self, float vx, float vy, float angular_rate);
	void (*set_velocity_radius)(void *self, float linear, float r, bool insitu);
	void (*stop)(void *self);
} ChassisTypeDef;

/* ---------- JetTank 差速履带 ---------- */
#define JETTANK_WHEEL_DIAMETER 54.0 /* mm，驱动轮等效直径 */
#define JETTANK_CORRECITION_FACTOR 1.0 /* 速度标定 */
#define JETTANK_SHAFT_LENGTH 203.8 /* mm，左右轮中心距 */

/* ---------- 悬挂履带车 TANKBLACK（CHASSIS_TYPE_TANKBLACK） ---------- */
#define TANKBLACK_WHEEL_DIAMETER 54.0 /* mm，原厂标称；含履带厚度可在上位机标定 */
#define TANKBLACK_CORRECITION_FACTOR 1.0 /* 速度标定 */
#define TANKBLACK_SHAFT_LENGTH 152.8 /* mm，左右驱动轮中心距 */

/* ---------- JetAuto 麦轮 ---------- */
#define JETAUTO_WHEEL_DIAMETER 96.5 /* mm */
#define JETAUTO_CORRECITION_FACTOR 1.0 /* 速度标定 */
#define JETAUTO_SHAFT_LENGTH 218.0 /* mm */
#define JETAUTO_WHEELBASE  195.0 /* mm，前后轮距 */

/* ---------- TI4WD 四轮差速 ---------- */
#define TI4WD_WHEEL_DIAMETER 45.0 /* mm */
#define TI4WD_CORRECITION_FACTOR 1.0 /* 速度标定 */
#define TI4WD_SHAFT_LENGTH 129.0 /* mm */
#define TI4WD_WHEELBASE  145.0 /* mm */

/* ---------- JetAcker 阿克曼 ---------- */
#define JETACKER_WHEEL_DIAMETER 100.0 /* mm */
#define JETACKER_CORRECITION_FACTOR 1.0 /* 速度标定 */
#define JETACKER_SHAFT_LENGTH 213.33 /* mm，轴距相关 */
#define JETACKER_WHEELBASE  224.92 /* mm */

#endif
