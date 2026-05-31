#include "usb_host.h"
#include "usbh_core.h"
#include "usbh_hid.h"
#include "usbh_hid_gamepad.h"
#include "cmsis_os2.h"

#define ATC_THRESHOLD 60

static char analog_to_direction(int8_t analog_x, int8_t analog_y);

void USBH_HID_EventCallback(USBH_HandleTypeDef *phost)
{
    extern osMessageQueueId_t moving_ctrl_queueHandle;
    static HID_GAMEPAD_Info_TypeDef last_info;
    static char last_direction_msg = 'I';

    switch(USBH_HID_GetDeviceType(phost)) {
        case 0xFF: {
            HID_GAMEPAD_Info_TypeDef *info = USBH_HID_GetGamepadInfo(phost);
            if(info == NULL) {
                break;
            }

            char direction_msg = ' ';
            if(info->hat & 0x08) {
                direction_msg = (((info->hat & 0x07) + 1) & 0x07) + 0x41;
                osMessageQueuePut(moving_ctrl_queueHandle, &direction_msg, 0, 0);
            } else {
                if(last_info.hat != info->hat) {
                    direction_msg = 'I';
                    last_direction_msg = 'I';
                    osMessageQueuePut(moving_ctrl_queueHandle, &direction_msg, 0, 0);
                } else {
                    direction_msg = analog_to_direction(info->lx, info->ly);
                    if(direction_msg != 'I') {
                        osMessageQueuePut(moving_ctrl_queueHandle, &direction_msg, 0, 0);
                    } else if(last_direction_msg != 'I') {
                        osMessageQueuePut(moving_ctrl_queueHandle, &direction_msg, 0, 0);
                    }
                    last_direction_msg = direction_msg;
                }
            }

            if(!GAMEPAD_GET_BUTTON(&last_info, GAMEPAD_BUTTON_MASK_TRIANGLE)
               && GAMEPAD_GET_BUTTON(info, GAMEPAD_BUTTON_MASK_TRIANGLE)) {
                osMessageQueuePut(moving_ctrl_queueHandle, "j", 0, 0);
            }
            if(!GAMEPAD_GET_BUTTON(&last_info, GAMEPAD_BUTTON_MASK_CROSS)
               && GAMEPAD_GET_BUTTON(info, GAMEPAD_BUTTON_MASK_CROSS)) {
                osMessageQueuePut(moving_ctrl_queueHandle, "n", 0, 0);
            }
            if(!GAMEPAD_GET_BUTTON(&last_info, GAMEPAD_BUTTON_MASK_START)
               && GAMEPAD_GET_BUTTON(info, GAMEPAD_BUTTON_MASK_START)) {
                osMessageQueuePut(moving_ctrl_queueHandle, "S", 0, 0);
            }

            memcpy(&last_info, info, sizeof(HID_GAMEPAD_Info_TypeDef));
            break;
        }
        default:
            break;
    }
}

static char analog_to_direction(int8_t analog_x, int8_t analog_y)
{
    if(analog_x < -ATC_THRESHOLD) {
        if(analog_y < -ATC_THRESHOLD) {
            return 'D';
        }
        if(analog_y > ATC_THRESHOLD) {
            return 'B';
        }
        return 'C';
    }
    if(analog_x > ATC_THRESHOLD) {
        if(analog_y < -ATC_THRESHOLD) {
            return 'F';
        }
        if(analog_y > ATC_THRESHOLD) {
            return 'H';
        }
        return 'G';
    }
    if(analog_y < -ATC_THRESHOLD) {
        return 'E';
    }
    if(analog_y > ATC_THRESHOLD) {
        return 'A';
    }
    return 'I';
}
