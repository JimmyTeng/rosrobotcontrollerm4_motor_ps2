#include "sensor_telemetry.h"
#include "global.h"
#include "global_conf.h"
#include "packet.h"
#include "packet_reports.h"
#include "encoder_motor.h"
#include "cmsis_os2.h"

#if ENABLE_IMU
#include "QMI8658.h"
#endif

extern EncoderMotorObjectTypeDef *motors[4];

static osSemaphoreId_t sensor_telemetry_tickHandle;

void sensor_telemetry_init(void)
{
    if(sensor_telemetry_tickHandle != NULL) {
        return;
    }
    const osSemaphoreAttr_t attr = {
        .name = "sensor_tick",
    };
    sensor_telemetry_tickHandle = osSemaphoreNew(1, 0, &attr);
}

void sensor_telemetry_tick_from_isr(void)
{
    if(sensor_telemetry_tickHandle != NULL) {
        (void)osSemaphoreRelease(sensor_telemetry_tickHandle);
    }
}

void sensor_telemetry_wait_tick(void)
{
    if(sensor_telemetry_tickHandle == NULL) {
        osDelay(10);
        return;
    }
    (void)osSemaphoreAcquire(sensor_telemetry_tickHandle, osWaitForever);
}

#if !USE_PACKET_V2
void sensor_telemetry_publish(void)
{
    PacketReportMotorEncoder_TypeDef enc_report;
    enc_report.cmd = MOTOR_ENCODER_REPORT_CMD;
    enc_report.motor_num = 4;
    for(int i = 0; i < 4; ++i) {
        enc_report.units[i].motor_id = (uint8_t)i;
        enc_report.units[i].rps = motors[i]->rps;
    }
    packet_transmit(&packet_controller, PACKET_FUNC_MOTOR, &enc_report,
                    sizeof(PacketReportMotorEncoder_TypeDef));

#if ENABLE_IMU
    PacketReportIMU_Raw_TypeDef imu_report;
    read_xyz(imu_report.array.accel_array, imu_report.array.gyro_array);
    packet_transmit(&packet_controller, PACKET_FUNC_IMU, &imu_report,
                    sizeof(PacketReportIMU_Raw_TypeDef));
#endif
}
#endif /* !USE_PACKET_V2 */

#if !ENABLE_IMU
static void sensor_telemetry_task_entry(void *argument)
{
    (void)argument;
    osDelay(200);
    for(;;) {
        sensor_telemetry_wait_tick();
        sensor_telemetry_publish();
    }
}

void sensor_telemetry_task_start(void)
{
    static osThreadId_t task_handle;
    if(task_handle != NULL) {
        return;
    }
    const osThreadAttr_t attr = {
        .name = "sensor_tx",
        .stack_size = 256 * 4,
        .priority = (osPriority_t)osPriorityBelowNormal,
    };
    task_handle = osThreadNew(sensor_telemetry_task_entry, NULL, &attr);
}
#endif
