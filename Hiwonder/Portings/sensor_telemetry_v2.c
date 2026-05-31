#include "sensor_telemetry.h"
#include "debug_uart.h"
#include "global.h"
#include "global_conf.h"
#include "packet.h"
#include "packet_v2.h"
#include "packet_reports_v2.h"
#include "encoder_motor.h"
#include "button.h"
#include "QMI8658.h"

_Static_assert(sizeof(PacketTelemetryV3_TypeDef) == PACKET_TELEMETRY_SIZE_V3,
               "PacketTelemetryV3 size must match schema 0x03");

extern EncoderMotorObjectTypeDef *motors[4];
extern ButtonObjectTypeDef *buttons[2];
extern float battery_volt;

void button_event_latch_update(uint8_t key_id, uint8_t event)
{
    extern uint8_t button_telemetry_event_latch[2];
    if(key_id >= 1 && key_id <= 2) {
        button_telemetry_event_latch[key_id - 1] |= event;
    }
}

uint8_t button_telemetry_event_latch[2];

static PacketTelemetryV3_TypeDef telemetry_report;

void sensor_telemetry_publish(void)
{
    if(motors[0] == NULL) {
        return;
    }
    PacketTelemetryV3_TypeDef *report = &telemetry_report;
    memset(report, 0, sizeof(*report));
    report->schema_id = PACKET_TELEMETRY_SCHEMA_V3;

    report->motors.motor_num = 4;
    for(int i = 0; i < 4; ++i) {
        PacketV2MotorChannel_TypeDef *ch = &report->motors.channels[i];
        ch->motor_id = (uint8_t)i;
        ch->flags = 0x07;
        ch->pwm = (int16_t)motors[i]->current_pulse;
        ch->rps_cmd = motors[i]->pid_controller.set_point;
        ch->rps_meas = motors[i]->rps;
        ch->current_ma = 0xFFFF;
        ch->voltage_mv = 0xFFFF;
    }

    report->power.valid_mask = 0x0001;
    report->power.vin_mv = (uint32_t)(battery_volt + 0.5f);
    report->power.vin_ma = (int32_t)0x80000000;
    report->power.vout_mv = 0xFFFFFFFFu;
    report->power.vout_ma = (int32_t)0x80000000;
    report->power.vmotor_mv = 0xFFFFFFFFu;
    report->power.imotor_bus_ma = (int32_t)0x80000000;

#if ENABLE_IMU
    int16_t raw_acc[3];
    int16_t raw_gyro[3];
    uint16_t ssvt_a = 0;
    uint16_t ssvt_g = 0;
    read_raw_lsb(raw_acc, raw_gyro, &ssvt_a, &ssvt_g);
    report->imu_raw.ax = raw_acc[0];
    report->imu_raw.ay = raw_acc[1];
    report->imu_raw.az = raw_acc[2];
    report->imu_raw.gx = raw_gyro[0];
    report->imu_raw.gy = raw_gyro[1];
    report->imu_raw.gz = raw_gyro[2];
    report->imu_raw.acc_ssvt = ssvt_a;
    report->imu_raw.gyr_ssvt = ssvt_g;

    float acc[3];
    float gyro[3];
    read_xyz(acc, gyro);
    ImuFusedPose pose = imu_fusion_update(gyro[0], gyro[1], gyro[2], acc[0], acc[1], acc[2]);
    report->imu_fused.qw = pose.q[0];
    report->imu_fused.qx = pose.q[1];
    report->imu_fused.qy = pose.q[2];
    report->imu_fused.qz = pose.q[3];
    report->imu_fused.roll = pose.roll;
    report->imu_fused.pitch = pose.pitch;
    report->imu_fused.yaw = pose.yaw;
#endif

    report->buttons.key_num = 2;
    for(int i = 0; i < 2; ++i) {
        if(buttons[i] != NULL) {
            report->buttons.keys[i].key_id = buttons[i]->id;
            report->buttons.keys[i].pin_level = (uint8_t)buttons[i]->read_pin(buttons[i]);
            report->buttons.keys[i].stage = (uint8_t)buttons[i]->stage;
        }
        report->buttons.keys[i].event_latch = button_telemetry_event_latch[i];
        button_telemetry_event_latch[i] = 0;
    }

    int tx = packet_v2_transmit(&packet_controller, PACKET_FUNC_TELEMETRY,
                                report, sizeof(*report));
    debug_uart_telemetry_result(tx);
}
