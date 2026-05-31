/**
 * Host-side sanity test for IMU Fusion AHRS (same params as QMI8658.c).
 * Build: gcc -I../../Third_Party/Fusion/Fusion -o test_imu_fusion test_imu_fusion.c \
 *          ../../Third_Party/Fusion/Fusion/FusionAhrs.c \
 *          ../../Third_Party/Fusion/Fusion/FusionCompass.c \
 *          -lm
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "Fusion.h"

#define ONE_G          9.807f
#define IMU_FUSION_DT  0.01f
#define RAD_TO_DEG     57.295779513082320876798154814105f
#define DEG_TO_RAD     0.017453292519943295769236907684886f
#define FRAMES         500   /* 5 s @ 100 Hz */

static FusionAhrs g_ahrs;

static void fusion_reset(void)
{
    FusionAhrsInitialise(&g_ahrs);
    const FusionAhrsSettings settings = {
        .convention = FusionConventionNwu,
        .gain = 0.5f,
        .accelerationRejection = 10.0f,
        .magneticRejection = 0.0f,
        .rejectionTimeout = 0,
    };
    FusionAhrsSetSettings(&g_ahrs, &settings);
}

static void fusion_step(float gx_rad, float gy_rad, float gz_rad,
                        float ax_ms2, float ay_ms2, float az_ms2,
                        float *roll_deg, float *pitch_deg, float *yaw_deg,
                        float *qw, float *qx, float *qy, float *qz)
{
    const FusionVector gyroscope = {
        .axis = { gx_rad * RAD_TO_DEG, gy_rad * RAD_TO_DEG, gz_rad * RAD_TO_DEG },
    };
    const FusionVector accelerometer = {
        .axis = { ax_ms2 / ONE_G, ay_ms2 / ONE_G, az_ms2 / ONE_G },
    };

    FusionAhrsUpdateNoMagnetometer(&g_ahrs, gyroscope, accelerometer, IMU_FUSION_DT);

    const FusionQuaternion q = FusionAhrsGetQuaternion(&g_ahrs);
    const FusionEuler e = FusionQuaternionToEuler(q);

    *roll_deg = e.angle.roll;
    *pitch_deg = e.angle.pitch;
    *yaw_deg = e.angle.yaw;
    *qw = q.element.w;
    *qx = q.element.x;
    *qy = q.element.y;
    *qz = q.element.z;
}

static int near(float a, float b, float tol)
{
    return fabsf(a - b) <= tol;
}

static int test_flat_static(void)
{
    fusion_reset();
    float roll = 0, pitch = 0, yaw = 0;
    float qw = 0, qx = 0, qy = 0, qz = 0;

    /* Z-up, 1g on Z, no rotation */
    for(int i = 0; i < FRAMES; ++i) {
        fusion_step(0, 0, 0, 0, 0, ONE_G, &roll, &pitch, &yaw, &qw, &qx, &qy, &qz);
    }

    printf("[flat static] roll=%.2f pitch=%.2f yaw=%.2f  q=(%.3f,%.3f,%.3f,%.3f)\n",
           roll, pitch, yaw, qw, qx, qy, qz);

    if(!near(roll, 0.0f, 2.0f) || !near(pitch, 0.0f, 2.0f)) {
        fprintf(stderr, "FAIL: flat static roll/pitch not near 0\n");
        return 1;
    }
    const float qnorm = sqrtf(qw*qw + qx*qx + qy*qy + qz*qz);
    if(!near(qnorm, 1.0f, 0.01f)) {
        fprintf(stderr, "FAIL: quaternion not normalized (norm=%.4f)\n", qnorm);
        return 1;
    }
    printf("PASS: flat static\n");
    return 0;
}

static int test_pitch_tilt(void)
{
    fusion_reset();
    float roll = 0, pitch = 0, yaw = 0;
    float qw = 0, qx = 0, qy = 0, qz = 0;

    /* Simulate ~30 deg nose-up: acc rotated, gyro quiet */
    const float tilt = 30.0f * DEG_TO_RAD;
    const float ax = ONE_G * sinf(tilt);
    const float az = ONE_G * cosf(tilt);

    for(int i = 0; i < FRAMES; ++i) {
        fusion_step(0, 0, 0, ax, 0, az, &roll, &pitch, &yaw, &qw, &qx, &qy, &qz);
    }

    printf("[pitch +30] roll=%.2f pitch=%.2f yaw=%.2f\n", roll, pitch, yaw);

    if(!near(fabsf(pitch), 30.0f, 5.0f)) {
        fprintf(stderr, "FAIL: expected |pitch| ~30 deg, got %.2f\n", pitch);
        return 1;
    }
    printf("PASS: pitch tilt\n");
    return 0;
}

static int test_gyro_integration(void)
{
    fusion_reset();
    float roll = 0, pitch = 0, yaw = 0;
    float qw = 0, qx = 0, qy = 0, qz = 0;

    /* 90 deg/s yaw for 1 s => ~90 deg yaw change (no mag, gyro-only after init) */
    const float yaw_rate = 90.0f * DEG_TO_RAD;
    const int frames = 100;

    for(int i = 0; i < FRAMES; ++i) {
        fusion_step(0, 0, 0, 0, 0, ONE_G, &roll, &pitch, &yaw, &qw, &qx, &qy, &qz);
    }
    const float yaw0 = yaw;

    for(int i = 0; i < frames; ++i) {
        fusion_step(0, 0, yaw_rate, 0, 0, ONE_G, &roll, &pitch, &yaw, &qw, &qx, &qy, &qz);
    }

    const float dyaw = yaw - yaw0;
    printf("[yaw spin] delta_yaw=%.2f deg (expect ~90)\n", dyaw);

    if(!near(dyaw, 90.0f, 15.0f)) {
        fprintf(stderr, "FAIL: yaw integration off (delta=%.2f)\n", dyaw);
        return 1;
    }
    printf("PASS: gyro yaw integration\n");
    return 0;
}

static int test_state_persistence(void)
{
    fusion_reset();
    float roll = 0, pitch = 0, yaw = 0;
    float qw = 0, qx = 0, qy = 0, qz = 0;

    const float tilt = 20.0f * DEG_TO_RAD;
    const float ax = ONE_G * sinf(tilt);
    const float az = ONE_G * cosf(tilt);

    for(int i = 0; i < FRAMES; ++i) {
        fusion_step(0, 0, 0, ax, 0, az, &roll, &pitch, &yaw, &qw, &qx, &qy, &qz);
    }
    const float pitch0 = pitch;

    /* Brief gyro burst should change yaw while pitch estimate holds */
    for(int i = 0; i < 50; ++i) {
        fusion_step(0, 0, 45.0f * DEG_TO_RAD, ax, 0, az, &roll, &pitch, &yaw, &qw, &qx, &qy, &qz);
    }

    if(fabsf(pitch - pitch0) > 10.0f) {
        fprintf(stderr, "FAIL: pitch jumped %.2f -> %.2f after yaw motion\n", pitch0, pitch);
        return 1;
    }
    if(fabsf(yaw) < 5.0f) {
        fprintf(stderr, "FAIL: yaw did not integrate (yaw=%.2f)\n", yaw);
        return 1;
    }
    printf("PASS: state persistence (pitch %.2f stable, yaw=%.2f after spin)\n", pitch, yaw);
    return 0;
}

int main(void)
{
    int fails = 0;
    fails += test_flat_static();
    fails += test_pitch_tilt();
    fails += test_gyro_integration();
    fails += test_state_persistence();

    if(fails == 0) {
        printf("\nAll %d IMU fusion tests passed.\n", 4);
        return 0;
    }
    fprintf(stderr, "\n%d test(s) failed.\n", fails);
    return 1;
}
