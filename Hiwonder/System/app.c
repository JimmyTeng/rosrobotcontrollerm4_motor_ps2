/**
 * @file app.c
 * @brief TANKBLACK 悬挂履带车主应用：PS2 手柄 → 差速底盘
 */

#include "cmsis_os2.h"
#include "lwmem_porting.h"
#include "global.h"
#include "adc.h"
#include "music_handle.h"

#define BUZZER_FREQ 1300

void buzzers_init(void);
void buttons_init(void);
void motors_init(void);
void chassis_init(void);

static void tank_control(char msg);

void app_task_entry(void *argument)
{
    extern osTimerId_t buzzer_timerHandle;
    extern osTimerId_t button_timerHandle;
    extern osTimerId_t battery_check_timerHandle;
    extern osMessageQueueId_t moving_ctrl_queueHandle;

    motors_init();
    buzzers_init();
    buttons_init();
    button_register_callback(buttons[0], music_key1_callback);
    music_play_init();

    osTimerStart(buzzer_timerHandle, BUZZER_TASK_PERIOD);
    osTimerStart(button_timerHandle, BUTTON_TASK_PERIOD);
    osTimerStart(battery_check_timerHandle, BATTERY_TASK_PERIOD);

    char msg = '\0';
    uint8_t msg_prio;
    osMessageQueueReset(moving_ctrl_queueHandle);

    chassis_init();
    set_chassis_type();
    chassis->stop(chassis);

    for(;;) {
        if(osMessageQueueGet(moving_ctrl_queueHandle, &msg, &msg_prio, 100) != osOK) {
            chassis->stop(chassis);
            continue;
        }
        tank_control(msg);
    }
}

static void tank_control(char msg)
{
    static float speed = 300.0f;

    switch(msg) {
        case 'S':
            buzzer_didi(buzzers[0], BUZZER_FREQ, 150, 200, 1);
            break;

        case 'I':
            chassis->stop(chassis);
            break;

        case 'A':
            chassis->set_velocity(chassis, speed, 0, 0);
            break;
        case 'B':
            chassis->set_velocity_radius(chassis, speed, 300, false);
            break;
        case 'C':
            chassis->set_velocity_radius(chassis, speed, 150, true);
            break;
        case 'D':
            chassis->set_velocity_radius(chassis, -speed, 300, false);
            break;
        case 'E':
            chassis->set_velocity(chassis, -speed, 0, 0);
            break;
        case 'F':
            chassis->set_velocity_radius(chassis, -speed, -300, false);
            break;
        case 'G':
            chassis->set_velocity_radius(chassis, speed, -150, true);
            break;
        case 'H':
            chassis->set_velocity_radius(chassis, speed, -300, false);
            break;

        case 'j':
            speed += 50.0f;
            speed = speed > 450.0f ? 450.0f : speed;
            break;
        case 'n':
            speed -= 50.0f;
            speed = speed < 50.0f ? 50.0f : speed;
            break;

        default:
            break;
    }
}
