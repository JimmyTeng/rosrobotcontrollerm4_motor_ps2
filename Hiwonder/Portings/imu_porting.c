#include "global.h"
#include "lwmem_porting.h"
#include "global_conf.h"
#include "QMI8658.h"
#include "sensor_telemetry.h"
#include "log.h"

#if ENABLE_IMU
struct QMI8658 qmi8658;

void imu_task_entry(void *argument)
{
    (void)argument;

    if(begin() == 0) {
        LOG_WARN("qmi8658_init fail\r\n");
    } else {
        LOG_INFO("qmi8658_init ok\r\n");
        osDelay(50);
        if(imu_static_calibrate(0)) {
            LOG_INFO("imu static gyro cal ok\r\n");
        } else {
            LOG_WARN("imu static gyro cal fail (keep still at boot)\r\n");
        }
    }
    osDelay(100);

    for(;;) {
        sensor_telemetry_wait_tick();
        sensor_telemetry_publish();
    }
}
#endif
