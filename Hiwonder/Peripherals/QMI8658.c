#include "QMI8658.h"
#include <math.h>
#include "global.h"
#include "lwmem_porting.h"
#include "global_conf.h"
#include "Fusion.h"

#define M_PI			(3.14159265358979323846f)
#define ONE_G			(9.807f)
#define QFABS(x)		(((x)<0.0f)?(-1.0f*(x)):(x))
#define IMU_FUSION_DT_S         (0.01f)
#define IMU_RAD_TO_DEG          (57.295779513082320876798154814105f)
#define IMU_FUSION_RATE_HZ      (100u)
/* 静态标定：默认 200 点 × 10 ms ≈ 2 s，仅需静止，不要求水平 */
#define IMU_STATIC_CAL_INTERVAL_MS  (10u)
#define IMU_STATIC_CAL_DEFAULT_SAMPLES (200u)
#define IMU_STATIC_GYRO_MAX_RAD_S   (0.35f)
#define IMU_STATIC_ACC_MIN_G        (0.85f)
#define IMU_STATIC_ACC_MAX_G        (1.15f)

static qmi8658_state g_imu;
static FusionAhrs g_fusion_ahrs;
static FusionOffset g_gyro_offset;
static float g_gyro_bias_rad[3];
static bool g_gyro_bias_valid = false;
/** 可选：用户显式调用 imu_set_pose_zero() 后，将当时姿态作为 roll/pitch/yaw 零点 */
static FusionQuaternion g_pose_zero_offset = {
    .element = {.w = 1.0f, .x = 0.0f, .y = 0.0f, .z = 0.0f},
};
static bool g_pose_zero_active = false;
static ImuFusedPose g_fused_pose = {
    .q = {1.0f, 0.0f, 0.0f, 0.0f},
};
static bool g_fused_pose_valid = false;

static FusionQuaternion imu_quaternion_conjugate(const FusionQuaternion quaternion)
{
    return (FusionQuaternion){
        .element = {
            .w = quaternion.element.w,
            .x = -quaternion.element.x,
            .y = -quaternion.element.y,
            .z = -quaternion.element.z,
        },
    };
}

static bool imu_is_stationary_sample(float gx, float gy, float gz, float ax, float ay, float az)
{
    const float acc_norm_g = sqrtf(ax * ax + ay * ay + az * az) / ONE_G;
    const float gyro_mag = sqrtf(gx * gx + gy * gy + gz * gz);

    if(acc_norm_g < IMU_STATIC_ACC_MIN_G || acc_norm_g > IMU_STATIC_ACC_MAX_G) {
        return false;
    }
    if(gyro_mag > IMU_STATIC_GYRO_MAX_RAD_S) {
        return false;
    }
    return true;
}

static void imu_seed_gyro_offset_from_bias(void)
{
    if(!g_gyro_bias_valid) {
        return;
    }

    g_gyro_offset.gyroscopeOffset.axis.x = g_gyro_bias_rad[0] * IMU_RAD_TO_DEG;
    g_gyro_offset.gyroscopeOffset.axis.y = g_gyro_bias_rad[1] * IMU_RAD_TO_DEG;
    g_gyro_offset.gyroscopeOffset.axis.z = g_gyro_bias_rad[2] * IMU_RAD_TO_DEG;
    g_gyro_offset.timer = g_gyro_offset.timeout;
    g_gyro_offset.cal_count = 0;
}

static void imu_fusion_reset_attitude(void)
{
    FusionAhrsInitialise(&g_fusion_ahrs);
    const FusionAhrsSettings settings = {
        .convention = FusionConventionNwu,
        .gain = 0.5f,
        .accelerationRejection = 10.0f,
        .magneticRejection = 0.0f,
        .rejectionTimeout = 0,
    };
    FusionAhrsSetSettings(&g_fusion_ahrs, &settings);
    g_fused_pose.q[0] = 1.0f;
    g_fused_pose.q[1] = 0.0f;
    g_fused_pose.q[2] = 0.0f;
    g_fused_pose.q[3] = 0.0f;
    g_fused_pose.roll = 0.0f;
    g_fused_pose.pitch = 0.0f;
    g_fused_pose.yaw = 0.0f;
    g_fused_pose_valid = false;
}

void imu_fusion_reset(void)
{
    FusionOffsetInitialise(&g_gyro_offset, IMU_FUSION_RATE_HZ);
    imu_seed_gyro_offset_from_bias();
    imu_fusion_reset_attitude();
}

bool imu_static_calibrate(uint16_t sample_count)
{
    if(sample_count == 0) {
        sample_count = IMU_STATIC_CAL_DEFAULT_SAMPLES;
    }

    float sum_gx = 0.0f;
    float sum_gy = 0.0f;
    float sum_gz = 0.0f;
    uint16_t accepted = 0;
    const uint16_t max_attempts = (uint16_t)(sample_count * 3U);

    for(uint16_t attempt = 0; attempt < max_attempts && accepted < sample_count; ++attempt) {
        float acc[3];
        float gyro[3];

        read_xyz(acc, gyro);
        if(!imu_is_stationary_sample(gyro[0], gyro[1], gyro[2], acc[0], acc[1], acc[2])) {
            osDelay(IMU_STATIC_CAL_INTERVAL_MS);
            continue;
        }

        sum_gx += gyro[0];
        sum_gy += gyro[1];
        sum_gz += gyro[2];
        ++accepted;
        osDelay(IMU_STATIC_CAL_INTERVAL_MS);
    }

    if(accepted < (sample_count / 4U)) {
        LOG_WARN("imu_static_calibrate: only %u/%u stable samples\r\n",
                 (unsigned)accepted, (unsigned)sample_count);
        return false;
    }

    g_gyro_bias_rad[0] = sum_gx / (float)accepted;
    g_gyro_bias_rad[1] = sum_gy / (float)accepted;
    g_gyro_bias_rad[2] = sum_gz / (float)accepted;
    g_gyro_bias_valid = true;

    imu_seed_gyro_offset_from_bias();
    imu_fusion_reset_attitude();

    LOG_INFO("imu gyro bias rad/s: %+.5f %+.5f %+.5f (n=%u)\r\n",
             g_gyro_bias_rad[0], g_gyro_bias_rad[1], g_gyro_bias_rad[2],
             (unsigned)accepted);
    return true;
}

bool imu_get_gyro_bias(float bias_rad[3])
{
    if(!g_gyro_bias_valid || bias_rad == NULL) {
        return false;
    }

    bias_rad[0] = g_gyro_bias_rad[0];
    bias_rad[1] = g_gyro_bias_rad[1];
    bias_rad[2] = g_gyro_bias_rad[2];
    return true;
}

bool imu_is_gyro_calibrated(void)
{
    return g_gyro_bias_valid;
}

void imu_clear_gyro_bias(void)
{
    g_gyro_bias_valid = false;
    g_gyro_bias_rad[0] = 0.0f;
    g_gyro_bias_rad[1] = 0.0f;
    g_gyro_bias_rad[2] = 0.0f;
    FusionOffsetInitialise(&g_gyro_offset, IMU_FUSION_RATE_HZ);
}

void imu_set_pose_zero(void)
{
    const FusionQuaternion quaternion = FusionAhrsGetQuaternion(&g_fusion_ahrs);
    g_pose_zero_offset = imu_quaternion_conjugate(quaternion);
    g_pose_zero_active = true;
}

void imu_clear_pose_zero(void)
{
    g_pose_zero_active = false;
    g_pose_zero_offset.element.w = 1.0f;
    g_pose_zero_offset.element.x = 0.0f;
    g_pose_zero_offset.element.y = 0.0f;
    g_pose_zero_offset.element.z = 0.0f;
}

bool imu_is_pose_zero_active(void)
{
    return g_pose_zero_active;
}

static bool imu_sample_valid(float ax, float ay, float az)
{
    const float mag_sq = ax * ax + ay * ay + az * az;
    return mag_sq > (0.05f * ONE_G * ONE_G);
}

ImuFusedPose imu_fusion_update(float gx, float gy, float gz, float ax, float ay, float az)
{
    if(!imu_sample_valid(ax, ay, az)) {
        if(g_fused_pose_valid) {
            return g_fused_pose;
        }
        ImuFusedPose identity = {
            .q = {1.0f, 0.0f, 0.0f, 0.0f},
        };
        return identity;
    }

    FusionVector gyroscope = {
        .axis = {
            .x = gx * IMU_RAD_TO_DEG,
            .y = gy * IMU_RAD_TO_DEG,
            .z = gz * IMU_RAD_TO_DEG,
        },
    };
    const FusionVector accelerometer = {
        .axis = {
            .x = ax / ONE_G,
            .y = ay / ONE_G,
            .z = az / ONE_G,
        },
    };

    gyroscope = FusionOffsetUpdate(&g_gyro_offset, gyroscope);
    FusionAhrsUpdateNoMagnetometer(&g_fusion_ahrs, gyroscope, accelerometer, IMU_FUSION_DT_S);

    FusionQuaternion quaternion = FusionAhrsGetQuaternion(&g_fusion_ahrs);
    if(g_pose_zero_active) {
        quaternion = FusionQuaternionNormalise(
            FusionQuaternionMultiply(g_pose_zero_offset, quaternion));
    }

    const FusionEuler euler = FusionQuaternionToEuler(quaternion);

    g_fused_pose.q[0] = quaternion.element.w;
    g_fused_pose.q[1] = quaternion.element.x;
    g_fused_pose.q[2] = quaternion.element.y;
    g_fused_pose.q[3] = quaternion.element.z;
    g_fused_pose.roll = FusionDegreesToRadians(euler.angle.roll);
    g_fused_pose.pitch = FusionDegreesToRadians(euler.angle.pitch);
    g_fused_pose.yaw = FusionDegreesToRadians(euler.angle.yaw);
    g_fused_pose_valid = true;
    return g_fused_pose;
}

EulerAngles get_euler_angles(float gx, float gy, float gz, float ax, float ay, float az)
{
    const ImuFusedPose pose = imu_fusion_update(gx, gy, gz, ax, ay, az);
    EulerAngles euler;

    euler.roll = pose.roll * IMU_RAD_TO_DEG;
    euler.pitch = pose.pitch * IMU_RAD_TO_DEG;
    euler.yaw = pose.yaw * IMU_RAD_TO_DEG;
    return euler;
}


void write_reg(uint8_t reg,uint8_t value)
{
    HAL_I2C_Mem_Write(&hi2c2, QMI8658_ADDR << 1, reg, I2C_MEMADD_SIZE_8BIT, &value, 1, 0xFF);
}

uint8_t read_reg(uint8_t reg)
{
	uint8_t ret=0;
    HAL_I2C_Mem_Read(&hi2c2, QMI8658_ADDR << 1, reg, I2C_MEMADD_SIZE_8BIT, &ret, 1, 0xFF);
	return ret;
}

uint16_t readWord_reg(uint8_t reg)
{
	uint16_t ret[2]={0,0};
    HAL_I2C_Mem_Read(&hi2c2, QMI8658_ADDR << 1, reg, I2C_MEMADD_SIZE_8BIT, (uint8_t*)ret, 2, 0xFF);
	return ((ret[1] << 8) | ret[0]);
}


bool GetEulerAngles(float *pitch,float *roll, float *yaw)
{
  float acc[3];
  float gyro[3];  
  read_xyz(acc,gyro);

  EulerAngles ea = get_euler_angles(gyro[0], gyro[1],gyro[2], acc[0], acc[1], acc[2]);
  *pitch = ea.pitch;
  *roll = ea.roll;
  *yaw = ea.yaw;
  return true;
}



void read_sensor_data(float acc[3], float gyro[3])
{
//	unsigned char	buf_reg[12];
	short 			raw_acc_xyz[3];
	short 			raw_gyro_xyz[3];

	raw_acc_xyz[0] = (short)((unsigned short)( readWord_reg(Qmi8658Register_Ax_L) ));
	raw_acc_xyz[1] = (short)((unsigned short)( readWord_reg(Qmi8658Register_Ay_L) ));
	raw_acc_xyz[2] = (short)((unsigned short)( readWord_reg(Qmi8658Register_Az_L) ));

	raw_gyro_xyz[0] = (short)((unsigned short)( readWord_reg(Qmi8658Register_Gx_L) ));
	raw_gyro_xyz[1] = (short)((unsigned short)( readWord_reg(Qmi8658Register_Gy_L) ));
	raw_gyro_xyz[2] = (short)((unsigned short)( readWord_reg(Qmi8658Register_Gz_L) ));

#if defined(QMI8658_UINT_MG_DPS)
	// mg
	acc[0] = (float)(raw_acc_xyz[0]*1000.0f)/g_imu.ssvt_a;
	acc[1] = (float)(raw_acc_xyz[1]*1000.0f)/g_imu.ssvt_a;
	acc[2] = (float)(raw_acc_xyz[2]*1000.0f)/g_imu.ssvt_a;
#else
	// m/s2
	acc[0] = (float)(raw_acc_xyz[0]*ONE_G)/g_imu.ssvt_a;
	acc[1] = (float)(raw_acc_xyz[1]*ONE_G)/g_imu.ssvt_a;
	acc[2] = (float)(raw_acc_xyz[2]*ONE_G)/g_imu.ssvt_a;
#endif

#if defined(QMI8658_UINT_MG_DPS)
	// dps
	gyro[0] = (float)(raw_gyro_xyz[0]*1.0f)/g_imu.ssvt_g;
	gyro[1] = (float)(raw_gyro_xyz[1]*1.0f)/g_imu.ssvt_g;
	gyro[2] = (float)(raw_gyro_xyz[2]*1.0f)/g_imu.ssvt_g;
#else
	// rad/s
	gyro[0] = (float)(raw_gyro_xyz[0]*M_PI)/(g_imu.ssvt_g*180);		// *pi/180
	gyro[1] = (float)(raw_gyro_xyz[1]*M_PI)/(g_imu.ssvt_g*180);
	gyro[2] = (float)(raw_gyro_xyz[2]*M_PI)/(g_imu.ssvt_g*180);
#endif
}



void axis_convert(float data_a[3], float data_g[3], int layout)
{
	float raw[3],raw_g[3];

	raw[0] = data_a[0];
	raw[1] = data_a[1];
	//raw[2] = data[2];
	raw_g[0] = data_g[0];
	raw_g[1] = data_g[1];
	//raw_g[2] = data_g[2];

	if(layout >=4 && layout <= 7)
	{
		data_a[2] = -data_a[2];
		data_g[2] = -data_g[2];
	}

	if(layout%2)
	{
		data_a[0] = raw[1];
		data_a[1] = raw[0];
		
		data_g[0] = raw_g[1];
		data_g[1] = raw_g[0];
	}
	else
	{
		data_a[0] = raw[0];
		data_a[1] = raw[1];

		data_g[0] = raw_g[0];
		data_g[1] = raw_g[1];
	}

	if((layout==1)||(layout==2)||(layout==4)||(layout==7))
	{
		data_a[0] = -data_a[0];
		data_g[0] = -data_g[0];
	}
	if((layout==2)||(layout==3)||(layout==6)||(layout==7))
	{
		data_a[1] = -data_a[1];
		data_g[1] = -data_g[1];
	}
}



void read_raw_lsb(int16_t acc[3], int16_t gyro[3], uint16_t *acc_ssvt, uint16_t *gyr_ssvt)
{
    short raw_acc_xyz[3];
    short raw_gyro_xyz[3];
    float acc_f[3];
    float gyro_f[3];

    raw_acc_xyz[0] = (short)((unsigned short)(readWord_reg(Qmi8658Register_Ax_L)));
    raw_acc_xyz[1] = (short)((unsigned short)(readWord_reg(Qmi8658Register_Ay_L)));
    raw_acc_xyz[2] = (short)((unsigned short)(readWord_reg(Qmi8658Register_Az_L)));
    raw_gyro_xyz[0] = (short)((unsigned short)(readWord_reg(Qmi8658Register_Gx_L)));
    raw_gyro_xyz[1] = (short)((unsigned short)(readWord_reg(Qmi8658Register_Gy_L)));
    raw_gyro_xyz[2] = (short)((unsigned short)(readWord_reg(Qmi8658Register_Gz_L)));

    acc_f[0] = (float)raw_acc_xyz[0];
    acc_f[1] = (float)raw_acc_xyz[1];
    acc_f[2] = (float)raw_acc_xyz[2];
    gyro_f[0] = (float)raw_gyro_xyz[0];
    gyro_f[1] = (float)raw_gyro_xyz[1];
    gyro_f[2] = (float)raw_gyro_xyz[2];
    axis_convert(acc_f, gyro_f, 0);
    acc[0] = (int16_t)acc_f[0];
    acc[1] = (int16_t)acc_f[1];
    acc[2] = (int16_t)acc_f[2];
    gyro[0] = (int16_t)gyro_f[0];
    gyro[1] = (int16_t)gyro_f[1];
    gyro[2] = (int16_t)gyro_f[2];
    if(acc_ssvt != NULL) {
        *acc_ssvt = g_imu.ssvt_a;
    }
    if(gyr_ssvt != NULL) {
        *gyr_ssvt = g_imu.ssvt_g;
    }
}

void read_xyz(float acc[3], float gyro[3])
{
	unsigned char	status;
	unsigned char data_ready = 0;

#if defined(QMI8658_SYNC_SAMPLE_MODE)
	qmi8658_read_reg(Qmi8658Register_StatusInt, &status, 1);
	if(status&0x01)
	{
		data_ready = 1;
		qmi8658_delay_us(6);	// delay 6us
	}
#else
	status = read_reg(Qmi8658Register_Status0);
	if(status&0x03)
	{
		data_ready = 1;
	}
#endif
	if(data_ready)
	{
		read_sensor_data(acc, gyro);
		axis_convert(acc, gyro, 0);
#if defined(QMI8658_USE_CALI)
		qmi8658_data_cali(1, acc);
		qmi8658_data_cali(2, gyro);
#endif
		g_imu.imu[0] = acc[0];
		g_imu.imu[1] = acc[1];
		g_imu.imu[2] = acc[2];
		g_imu.imu[3] = gyro[0];
		g_imu.imu[4] = gyro[1];
		g_imu.imu[5] = gyro[2];
	}
	else
	{
		acc[0] = g_imu.imu[0];
		acc[1] = g_imu.imu[1];
		acc[2] = g_imu.imu[2];
		gyro[0] = g_imu.imu[3];
		gyro[1] = g_imu.imu[4];
		gyro[2] = g_imu.imu[5];
	}
}



void config_acc(enum qmi8658_AccRange range, enum qmi8658_AccOdr odr, enum qmi8658_LpfConfig lpfEnable, enum qmi8658_StConfig stEnable)
{
	unsigned char ctl_dada;

	switch(range)
	{
		case Qmi8658AccRange_2g:
			g_imu.ssvt_a = (1<<14);
			break;
		case Qmi8658AccRange_4g:
			g_imu.ssvt_a = (1<<13);
			break;
		case Qmi8658AccRange_8g:
			g_imu.ssvt_a = (1<<12);
			break;
		case Qmi8658AccRange_16g:
			g_imu.ssvt_a = (1<<11);
			break;
		default: 
			range = Qmi8658AccRange_8g;
			g_imu.ssvt_a = (1<<12);
	}
	if(stEnable == Qmi8658St_Enable)
		ctl_dada = (unsigned char)range|(unsigned char)odr|0x80;
	else
		ctl_dada = (unsigned char)range|(unsigned char)odr;
		
	write_reg(Qmi8658Register_Ctrl2, ctl_dada);
// set LPF & HPF
	ctl_dada = read_reg(Qmi8658Register_Ctrl5);
	ctl_dada &= 0xf0;
	if(lpfEnable == Qmi8658Lpf_Enable)
	{
		ctl_dada |= A_LSP_MODE_3;
		ctl_dada |= 0x01;
	}
	else
	{
		ctl_dada &= ~0x01;
	}
	//ctl_dada = 0x00;
	write_reg(Qmi8658Register_Ctrl5,ctl_dada);
// set LPF & HPF
}

void config_gyro(enum qmi8658_GyrRange range, enum qmi8658_GyrOdr odr, enum qmi8658_LpfConfig lpfEnable, enum qmi8658_StConfig stEnable)
{
	// Set the CTRL3 register to configure dynamic range and ODR
	unsigned char ctl_dada; 

	// Store the scale factor for use when processing raw data
	switch (range)
	{
		case Qmi8658GyrRange_16dps:
			g_imu.ssvt_g = 2048;
			break;			
		case Qmi8658GyrRange_32dps:
			g_imu.ssvt_g = 1024;
			break;
		case Qmi8658GyrRange_64dps:
			g_imu.ssvt_g = 512;
			break;
		case Qmi8658GyrRange_128dps:
			g_imu.ssvt_g = 256;
			break;
		case Qmi8658GyrRange_256dps:
			g_imu.ssvt_g = 128;
			break;
		case Qmi8658GyrRange_512dps:
			g_imu.ssvt_g = 64;
			break;
		case Qmi8658GyrRange_1024dps:
			g_imu.ssvt_g = 32;
			break;
		case Qmi8658GyrRange_2048dps:
			g_imu.ssvt_g = 16;
			break;
//		case Qmi8658GyrRange_4096dps:
//			g_imu.ssvt_g = 8;
//			break;
		default: 
			range = Qmi8658GyrRange_512dps;
			g_imu.ssvt_g = 64;
			break;
	}

	if(stEnable == Qmi8658St_Enable)
		ctl_dada = (unsigned char)range|(unsigned char)odr|0x80;
	else
		ctl_dada = (unsigned char)range | (unsigned char)odr;
	write_reg(Qmi8658Register_Ctrl3, ctl_dada);

// Conversion from degrees/s to rad/s if necessary
// set LPF & HPF
	ctl_dada = read_reg(Qmi8658Register_Ctrl5);
	ctl_dada &= 0x0f;
	if(lpfEnable == Qmi8658Lpf_Enable)
	{
		ctl_dada |= G_LSP_MODE_3;
		ctl_dada |= 0x10;
	}
	else
	{
		ctl_dada &= ~0x10;
	}
	//ctl_dada = 0x00;
	write_reg(Qmi8658Register_Ctrl5,ctl_dada);
// set LPF & HPF
}

void enableSensors(unsigned char enableFlags)
{
#if defined(QMI8658_SYNC_SAMPLE_MODE)
	qmi8658_write_reg(Qmi8658Register_Ctrl7, enableFlags | 0x80);
#elif defined(QMI8658_USE_FIFO)
	//qmi8658_write_reg(Qmi8658Register_Ctrl7, enableFlags|QMI8658_DRDY_DISABLE);
	write_reg(Qmi8658Register_Ctrl7, enableFlags);
#else
	qmi8658_write_reg(Qmi8658Register_Ctrl7, enableFlags);
#endif
	g_imu.cfg.enSensors = enableFlags&0x03;

	osDelay(1);
}

void config_reg(unsigned char low_power)
{
	enableSensors(QMI8658_DISABLE_ALL);
	if(low_power)
	{
		g_imu.cfg.enSensors = QMI8658_ACC_ENABLE;
		g_imu.cfg.accRange = Qmi8658AccRange_8g;
		g_imu.cfg.accOdr = Qmi8658AccOdr_LowPower_21Hz;
		g_imu.cfg.gyrRange = Qmi8658GyrRange_1024dps;
		g_imu.cfg.gyrOdr = Qmi8658GyrOdr_250Hz;
	}
	else
	{		
		g_imu.cfg.enSensors = QMI8658_ACCGYR_ENABLE;
		g_imu.cfg.accRange = Qmi8658AccRange_8g;
		g_imu.cfg.accOdr = Qmi8658AccOdr_250Hz;
		g_imu.cfg.gyrRange = Qmi8658GyrRange_1024dps;
		g_imu.cfg.gyrOdr = Qmi8658GyrOdr_250Hz;
	}
	
	if(g_imu.cfg.enSensors & QMI8658_ACC_ENABLE)
	{
		config_acc(g_imu.cfg.accRange, g_imu.cfg.accOdr, Qmi8658Lpf_Disable, Qmi8658St_Disable);
	}
	if(g_imu.cfg.enSensors & QMI8658_GYR_ENABLE)
	{
		config_gyro(g_imu.cfg.gyrRange, g_imu.cfg.gyrOdr, Qmi8658Lpf_Disable, Qmi8658St_Disable);
	}
}

unsigned char get_id(void)
{
	unsigned char qmi8658_chip_id = 0x00;
	unsigned char qmi8658_revision_id = 0x00;
	unsigned char qmi8658_slave[2] = {QMI8658_SLAVE_ADDR_L, QMI8658_SLAVE_ADDR_H};
	int retry = 0;
	unsigned char iCount = 0;
//	unsigned char firmware_id[3];
//	unsigned char uuid[6];
//	unsigned int uuid_low, uuid_high;

	while(iCount<2)
	{
		g_imu.slave = qmi8658_slave[iCount];
		retry = 0;
		while((qmi8658_chip_id != 0x05)&&(retry++ < 5))
		{
			qmi8658_chip_id = read_reg(Qmi8658Register_WhoAmI);
//			printf("Qmi8658Register_WhoAmI = 0x%x\n", qmi8658_chip_id);
		}
		if(qmi8658_chip_id == 0x05)
		{
			qmi8658_on_demand_cali();

			g_imu.cfg.ctrl8_value = 0xc0;
			//QMI8658_INT1_ENABLE, QMI8658_INT2_ENABLE
			write_reg(Qmi8658Register_Ctrl1, 0x60|QMI8658_INT2_ENABLE|QMI8658_INT1_ENABLE);
			qmi8658_revision_id = read_reg(Qmi8658Register_Revision);			
			// qmi8658_read_reg(Qmi8658Register_firmware_id, firmware_id, 3);
			// qmi8658_read_reg(Qmi8658Register_uuid, uuid, 6);
			write_reg(Qmi8658Register_Ctrl7, 0x00);
			write_reg(Qmi8658Register_Ctrl8, g_imu.cfg.ctrl8_value);
			// uuid_low = (unsigned int)((unsigned int)(uuid[2]<<16)|(unsigned int)(uuid[1]<<8)|(uuid[0]));
			// uuid_high = (unsigned int)((unsigned int)(uuid[5]<<16)|(unsigned int)(uuid[4]<<8)|(uuid[3]));
			// qmi8658_log("qmi8658_init slave=0x%x Revision=0x%x\n", g_imu.slave, qmi8658_revision_id);
			// qmi8658_log("Firmware ID[0x%x 0x%x 0x%x]\n", firmware_id[2], firmware_id[1],firmware_id[0]);
			// qmi8658_log("UUID[0x%x %x]\n", uuid_high ,uuid_low);
			break;
		}
		iCount++;
	}

	return qmi8658_chip_id;
}


void qmi8658_on_demand_cali(void)
{
//	printf("qmi8658_on_demand_cali start\n");
	write_reg(Qmi8658Register_Reset, 0xb0);
	osDelay(10);	// delay
	write_reg(Qmi8658Register_Ctrl9, (unsigned char)qmi8658_Ctrl9_Cmd_On_Demand_Cali);
	osDelay(2200);	// delay 2000ms above
	write_reg(Qmi8658Register_Ctrl9, (unsigned char)qmi8658_Ctrl9_Cmd_NOP);
	osDelay(100);	// delay
//	printf("qmi8658_on_demand_cali done\n");
}

unsigned char begin(void)
{
	if(get_id() == 0x05)
	{
#if defined(QMI8658_USE_AMD)
		qmi8658_config_amd();
#endif
#if defined(QMI8658_USE_PEDOMETER)
		qmi8658_config_pedometer(125);
		qmi8658_enable_pedometer(1);
#endif
		config_reg(0);
		enableSensors(g_imu.cfg.enSensors);
		imu_fusion_reset();
		dump_reg();
#if defined(QMI8658_USE_CALI)
		memset(&g_cali, 0, sizeof(g_cali));
#endif
		return 1;
	}
	else
	{
		// Serial.print("qmi8658_init fail\n");
		return 0;
	}
}


void dump_reg(void)
{
	// unsigned char read_data[8];

	// qmi8658_read_reg(Qmi8658Register_Ctrl1, read_data, 8);
	// qmi8658_log("Ctrl1[0x%x]\nCtrl2[0x%x]\nCtrl3[0x%x]\nCtrl4[0x%x]\nCtrl5[0x%x]\nCtrl6[0x%x]\nCtrl7[0x%x]\nCtrl8[0x%x]\n",
	// 				read_data[0],read_data[1],read_data[2],read_data[3],read_data[4],read_data[5],read_data[6],read_data[7]);	
}

