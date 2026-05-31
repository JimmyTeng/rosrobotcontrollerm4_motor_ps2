#include "global.h"
#include "differential_chassis.h"
#include "encoder_motor.h"
#include "chassis.h"
#include "motors_param.h"

DifferentialChassisTypeDef tank_black;
ChassisTypeDef *chassis = (ChassisTypeDef *)&tank_black;

static void tankblack_set_motors(void *self, float rps_l, float rps_r)
{
    (void)self;
    encoder_motor_set_speed(motors[0], -rps_l);
    encoder_motor_set_speed(motors[1], rps_r);
}

void chassis_init(void)
{
    diff_chassis_object_init(&tank_black);
    tank_black.base.chassis_type = CHASSIS_TYPE_TANKBLACK;
    tank_black.correction_factor = TANKBLACK_CORRECITION_FACTOR;
    tank_black.wheel_diameter = TANKBLACK_WHEEL_DIAMETER;
    tank_black.shaft_length = TANKBLACK_SHAFT_LENGTH;
    tank_black.set_motors = tankblack_set_motors;
}

void set_chassis_type(void)
{
    chassis = (ChassisTypeDef *)&tank_black;
    for(int i = 0; i < 4; ++i) {
        if(motors[i] != NULL) {
            set_motor_type(motors[i], MOTOR_TYPE_JGB37);
        }
    }
}
