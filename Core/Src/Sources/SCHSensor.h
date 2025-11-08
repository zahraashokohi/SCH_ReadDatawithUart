#ifndef _SCHSENSOR_H
#define _SCHSENSOR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * API return codes
 */
#define SCH_OK                      0
#define SCH_ERR_NULL_POINTER       -1
#define SCH_ERR_INVALID_PARAM      -2
#define SCH_ERR_SENSOR_INIT        -3
#define SCH_ERR_OTHER              -4

/**
 * SCH1 Standard requests
 */
// Rate and acceleration
#define REQ_READ_RATE_X1            0x0048000000AC
#define REQ_READ_RATE_Y1            0x00880000009A
#define REQ_READ_RATE_Z1            0x00C80000006D
#define REQ_READ_ACC_X1             0x0108000000F6
#define REQ_READ_ACC_Y1             0x014800000001
#define REQ_READ_ACC_Z1             0x018800000037
#define REQ_READ_ACC_X3             0x01C8000000C0
#define REQ_READ_ACC_Y3             0x02080000002E
#define REQ_READ_ACC_Z3             0x0248000000D9
#define REQ_READ_RATE_X2            0x0288000000EF
#define REQ_READ_RATE_Y2            0x02C800000018
#define REQ_READ_RATE_Z2            0x030800000083
#define REQ_READ_ACC_X2             0x034800000074
#define REQ_READ_ACC_Y2             0x038800000042
#define REQ_READ_ACC_Z2             0x03C8000000B5

// Status
#define REQ_READ_STAT_SUM           0x05080000001C
#define REQ_READ_STAT_SUM_SAT       0x0548000000EB
#define REQ_READ_STAT_COM           0x0588000000DD
#define REQ_READ_STAT_RATE_COM      0x05C80000002A
#define REQ_READ_STAT_RATE_X        0x0608000000C4
#define REQ_READ_STAT_RATE_Y        0x064800000033
#define REQ_READ_STAT_RATE_Z        0x068800000005
#define REQ_READ_STAT_ACC_X         0x06C8000000F2
#define REQ_READ_STAT_ACC_Y         0x070800000069
#define REQ_READ_STAT_ACC_Z         0x07480000009E

// Temperature and traceability
#define REQ_READ_TEMP               0x0408000000B1
#define REQ_READ_SN_ID1             0x0F4800000065
#define REQ_READ_SN_ID2             0x0F8800000053
#define REQ_READ_SN_ID3             0x0FC8000000A4
#define REQ_READ_COMP_ID            0x0F0800000092

// Filters
#define REQ_READ_FILT_RATE          0x0948000000FA
#define REQ_READ_FILT_ACC12         0x0988000000CC
#define REQ_READ_FILT_ACC3          0x09C80000003B
#define REQ_READ_RATE_CTRL          0x0A08000000D5
#define REQ_READ_ACC12_CTRL         0x0A4800000022
#define REQ_READ_ACC3_CTRL          0x0A8800000014
#define REQ_READ_MODE_CTRL          0x0D4800000010
#define REQ_SET_FILT_RATE           0x0968000000
#define REQ_SET_FILT_ACC12          0x09A8000000
#define REQ_SET_FILT_ACC3           0x09E8000000

// Sensitivity and decimation
#define REQ_SET_RATE_CTRL           0x0A28000000
#define REQ_SET_ACC12_CTRL          0x0A68000000
#define REQ_SET_ACC3_CTRL           0x0AA8000000
#define REQ_SET_MODE_CTRL           0x0D68000000

// DRY/SYNC configuration
#define REQ_READ_USER_IF_CTRL       0x0CC80000007C
#define REQ_SET_USER_IF_CTRL        0x0CE8000000

// Other
#define REQ_SOFTRESET               0x0DA800000AC3

/**
 * Frame field masks
 */
#define TA_FIELD_MASK               0xFFC000000000
#define SA_FIELD_MASK               0x7FE000000000
#define DATA_FIELD_MASK             0x00000FFFFF00
#define CRC_FIELD_MASK              0x0000000000FF
#define ERROR_FIELD_MASK            0x001E00000000

/**
 * Macros
 */
#define SPI48_DATA_INT32(a)         (((int32_t)(((a) << 4)  & 0xfffff000UL)) >> 12)
#define SPI48_DATA_UINT32(a)        ((uint32_t)(((a) >> 8)  & 0x000fffffUL))
#define SPI48_DATA_UINT16(a)        ((uint16_t)(((a) >> 8)  & 0x0000ffffUL))
#define GET_TEMPERATURE(a)          ((a) / 100.0f)

/**
 * Filter bypass mode marker
 */
#define SCH_FILTER_BYPASS   0

#define AVG_FACTOR  1      // SCH1 sample averaging

#define FILTER_RATE         30.0f       // Hz, LPF1 Nominal Cut-off Frequency (-3dB).
#define FILTER_ACC12        30.0f
#define FILTER_ACC3         30.0f
#define SENSITIVITY_RATE1   1600.0f     // LSB / dps, DYN1 Nominal Sensitivity for 20 bit data.
#define SENSITIVITY_RATE2   1600.0f
#define SENSITIVITY_ACC1    3200.0f     // LSB / m/s2, DYN1 Nominal Sensitivity for 20 bit data.
#define SENSITIVITY_ACC2    3200.0f
#define SENSITIVITY_ACC3    3200.0f     // LSB / m/s2, DYN1 Nominal Sensitivity for 20 bit data.
#define DECIMATION_RATE     32          // DEC5, Output sample rate decimation.
#define DECIMATION_ACC      32

/**
 * Structs
 */
typedef struct {
    int32_t rate1Raw[3];
    int32_t rate2Raw[3];
    int32_t acc1Raw[3];
    int32_t acc2Raw[3];
    int32_t acc3Raw[3];
    int32_t tempRaw;
    bool frameError;
} SCHRawData;

typedef struct {
    uint16_t rate;
    uint16_t acc;
    uint16_t acc3;
} SCHFilter;

typedef struct {
    uint16_t summary;
    uint16_t summarySat;
    uint16_t common;
    uint16_t rateCommon;
    uint16_t rateX;
    uint16_t rateY;
    uint16_t rateZ;
    uint16_t accX;
    uint16_t accY;
    uint16_t accZ;
} SCHStatus;

typedef struct {
    float rate1[3];
    float rate2[3];
    float acc1[3];
    float acc2[3];
    float acc3[3];
    float temp;
} SCHResult;

typedef struct {
    uint16_t rate1;
    uint16_t rate2;
    uint16_t acc1;
    uint16_t acc2;
    uint16_t acc3;
} SCHSensitivity;

typedef struct {
    uint16_t rate2;
    uint16_t acc2;
} SCHDecimation;

typedef enum {
    AXIS_X,
    AXIS_Y,
    AXIS_Z
} SCHAxis;

extern SCHRawData raw;

void SCHGetData(SCHRawData *data);
void SCHGetData2(SCHRawData *data);
void SCHReset(void);
int32_t  SCHInit(SCHFilter sFilter, SCHSensitivity sSensitivity, SCHDecimation sDecimation, bool enableDry);
uint32_t SCHConvertFilterToBitfield(uint32_t freq);
uint32_t SCHConvertRateSensToBitfield(uint32_t sens);
uint32_t SCHConvertBitfieldToRateSens(uint32_t bitfield);
uint32_t SCHConvertAccSensToBitfield(uint32_t sens);
uint32_t SCHConvertBitfieldToAccSens(uint32_t sens);
uint32_t SCHConvertDecimationToBitfield(uint32_t decimation);
uint32_t SCHConvertBitfieldToDecimation(uint32_t bitfield);
bool SCHCheck48BitFrameError(uint64_t *data, int size);
void SCHConvertData(SCHRawData *dataIn, SCHResult *dataOut);
int32_t SCHGetStatus(SCHStatus *statusOut);
char* SCHGetSnbr(void);
#endif
