/* SCHsensor.c
 * بازنویسی نام‌ها: توابع => PascalCase، متغیرها => camelCase
 * منطق و الگوریتم بدون تغییر از نسخه‌ی ارسال‌شده توسط کاربر باقی مانده.
 */

#include "SCHsensor.h"
#include "main.h"
#include <stdio.h>
#include <stdbool.h>
#include "spi.h"
#include "tim.h"
#include <stdint.h>

/**
 * Internal function prototypes (static where appropriate)
 */
static uint8_t Crc8(uint64_t spiFrame);
static uint8_t Crc3(uint32_t spiFrame);

/**
 * GPIO helpers (PascalCase names)
 */

void SCHExtresnHigh(void)
{
    HAL_GPIO_WritePin(EXTRESN_GPIO_Port, EXTRESN_Pin, GPIO_PIN_SET);
}

void SCHExtresnLow(void)
{
    HAL_GPIO_WritePin(EXTRESN_GPIO_Port, EXTRESN_Pin, GPIO_PIN_RESET);
}

void SCHCsHigh(void)
{
    HAL_GPIO_WritePin(CS_PIN_GPIO_Port, CS_PIN_Pin, GPIO_PIN_SET);
}

void SCHCsLow(void)
{
    HAL_GPIO_WritePin(CS_PIN_GPIO_Port, CS_PIN_Pin, GPIO_PIN_RESET);
}

void SCHReset(void)
{
    SCHExtresnLow();
    HAL_Delay(2);
    SCHExtresnHigh();
}

/**
 * @brief Send/receive 48-bit SPI request.
 *
 * @param request - 48-bit MOSI data
 * @return 48-bit received MISO data
 */
uint64_t SCHSpi48SendRequest(uint64_t request)
{
    uint64_t receivedData = 0;
    uint16_t txBuffer[3];
    uint16_t rxBuffer[3];
    uint8_t index;
    uint8_t size = 3;   // 48-bit SPI-transfer consists of three 16-bit transfers.

    // Split request qword (MOSI data) to tx buffer.
    for (index = 0; index < size; index++)
    {
        txBuffer[size - index - 1] = (request >> (index << 4)) & 0xFFFF;
    }

    // Send tx buffer and receive rx buffer simultaneously.
    SCHCsLow();
   // HAL_Delay(5);
    HAL_SPI_TransmitReceive(&hspi1, (uint8_t*)txBuffer, (uint8_t*)rxBuffer, size, 10);
  //  HAL_Delay(5);
    SCHCsHigh();
   // HAL_Delay(5);

    // Create receivedData qword from received rx buffer (MISO data).
    for (index = 0; index < size; index++)
    {
        receivedData |= (uint64_t)rxBuffer[index] << ((size - index - 1) << 4);
    }

    return receivedData;
}

void HwTimerSetFreq(uint32_t freq)
{
    // Set sample timer frequency. MCU APB1-bus frequency that clocks
    // the timer TIM2 is set to 1 MHz in HW.c function TIM_Init().
    htim2.Init.Period = 1000000 / freq;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
        while (1) {}
}

void SCHSendSpiReset(void)
{
    SCHSpi48SendRequest(REQ_SOFTRESET);
}

/**
 * Validation helpers
 */

bool SCHIsValidFilterFreq(uint32_t freq)
{
    if (freq == 13 || freq == 30 || freq == 68 || freq == 235 || freq == 280 || freq == 370 || freq == SCH_FILTER_BYPASS)
        return true;
    else
        return false;
}

bool SCHIsValidRateSens(uint32_t sens)
{
    if (sens == 1600 || sens == 3200 || sens == 6400)
        return true;
    else
        return false;
}

bool SCHIsValidAccSens(uint32_t sens)
{
    if (sens == 3200 || sens == 6400 || sens == 12800 || sens == 25600)
        return true;
    else
        return false;
}

bool SCHIsValidDecimation(uint32_t decimation)
{
    if (decimation == 2 || decimation == 4 || decimation == 8 || decimation == 16 || decimation == 32)
        return true;
    else
        return false;
}

bool SCHIsValidSampleRate(uint32_t freq)
{
    if ((freq >= 1) && (freq <= 10000))
        return true;

    return false;
}

/**
 * Set filters
 */
int SCHSetFilters(uint32_t freqRate12, uint32_t freqAcc12, uint32_t freqAcc3)
{
    uint32_t dataField;
    uint64_t requestFrameRate12;
    uint64_t responseFrameRate12;
    uint64_t requestFrameAcc12;
    uint64_t responseFrameAcc12;
    uint64_t requestFrameAcc3;
    uint64_t responseFrameAcc3;
    uint8_t crcValue;

    if (SCHIsValidFilterFreq(freqRate12) == false) {
        return SCH_ERR_INVALID_PARAM;
    }
    if (SCHIsValidFilterFreq(freqAcc12) == false) {
        return SCH_ERR_INVALID_PARAM;
    }
    if (SCHIsValidFilterFreq(freqAcc3) == false) {
        return SCH_ERR_INVALID_PARAM;
    }

    // Set filters for Rate_XYZ1 (interpolated) and Rate_XYZ2 (decimated) outputs.
    requestFrameRate12 = REQ_SET_FILT_RATE;
    dataField = SCHConvertFilterToBitfield(freqRate12);
    requestFrameRate12 |= dataField;
    requestFrameRate12 <<= 8;
    crcValue = Crc8(requestFrameRate12);
    requestFrameRate12 |= crcValue;
    SCHSpi48SendRequest(requestFrameRate12);

    // Set filters for Acc_XYZ1 (interpolated) and Acc_XYZ2 (decimated) outputs.
    requestFrameAcc12 = REQ_SET_FILT_ACC12;
    dataField = SCHConvertFilterToBitfield(freqAcc12);
    requestFrameAcc12 |= dataField;
    requestFrameAcc12 <<= 8;
    crcValue = Crc8(requestFrameAcc12);
    requestFrameAcc12 |= crcValue;
    SCHSpi48SendRequest(requestFrameAcc12);

    // Set filters for Acc_XYZ3 (interpolated) output.
    requestFrameAcc3 = REQ_SET_FILT_ACC3;
    dataField = SCHConvertFilterToBitfield(freqAcc3);
    requestFrameAcc3 |= dataField;
    requestFrameAcc3 <<= 8;
    crcValue = Crc8(requestFrameAcc3);
    requestFrameAcc3 |= crcValue;
    SCHSpi48SendRequest(requestFrameAcc3);

    // Read back filter register contents.
    SCHSpi48SendRequest(REQ_READ_FILT_RATE);
    responseFrameRate12 = SCHSpi48SendRequest(REQ_READ_FILT_ACC12);
    responseFrameAcc12 = SCHSpi48SendRequest(REQ_READ_FILT_ACC3);
    responseFrameAcc3 = SCHSpi48SendRequest(REQ_READ_FILT_ACC3);

    // Check that return frame is not blank.
    if ((responseFrameRate12 == 0xFFFFFFFFFFFF) || (responseFrameRate12 == 0x00))
        return SCH_ERR_OTHER;
    if ((responseFrameAcc12 == 0xFFFFFFFFFFFF) || (responseFrameAcc12 == 0x00))
        return SCH_ERR_OTHER;
    if ((responseFrameAcc3 == 0xFFFFFFFFFFFF) || (responseFrameAcc3 == 0x00))
        return SCH_ERR_OTHER;

    // Check that Source Address matches Target Address.
    if (((requestFrameRate12 & TA_FIELD_MASK) >> 38) != ((responseFrameRate12 & SA_FIELD_MASK) >> 37))
        return SCH_ERR_OTHER;
    if (((requestFrameAcc12 & TA_FIELD_MASK) >> 38) != ((responseFrameAcc12 & SA_FIELD_MASK) >> 37))
        return SCH_ERR_OTHER;
    if (((requestFrameAcc3 & TA_FIELD_MASK) >> 38) != ((responseFrameAcc3 & SA_FIELD_MASK) >> 37))
        return SCH_ERR_OTHER;

    // Check that read and written data match.
    if ((requestFrameRate12 & DATA_FIELD_MASK) != (responseFrameRate12 & DATA_FIELD_MASK))
        return SCH_ERR_OTHER;
    if ((requestFrameAcc12 & DATA_FIELD_MASK) != (responseFrameAcc12 & DATA_FIELD_MASK))
        return SCH_ERR_OTHER;
    if ((requestFrameAcc3 & DATA_FIELD_MASK) != (responseFrameAcc3 & DATA_FIELD_MASK))
        return SCH_ERR_OTHER;

    return SCH_OK;
}

/**
 * Set rate sensitivities and decimation
 */
int SCHSetRateSensDec(uint16_t sensRate1, uint16_t sensRate2, uint16_t decRate2)
{
    uint32_t dataField;
    uint32_t bitField;
    uint64_t requestFrameRateCtrl;
    uint64_t responseFrameRateCtrl;
    uint8_t crcValue;

    if (SCHIsValidRateSens(sensRate1) == false) {
        return SCH_ERR_INVALID_PARAM;
    }
    if (SCHIsValidRateSens(sensRate2) == false) {
        return SCH_ERR_INVALID_PARAM;
    }
    if (SCHIsValidDecimation(decRate2) == false) {
        return SCH_ERR_INVALID_PARAM;
    }

    // Set sensitivities for Rate_XYZ1 (interpolated) and Rate_XYZ2 (decimated) outputs.
    // Also set decimation for Rate_XYZ2.
    requestFrameRateCtrl = REQ_SET_RATE_CTRL;
    dataField = SCHConvertRateSensToBitfield(sensRate1);
    dataField <<= 3;
    bitField = SCHConvertRateSensToBitfield(sensRate2);
    dataField |= bitField;
    dataField <<= 3;
    bitField = SCHConvertDecimationToBitfield(decRate2);
    dataField |= bitField;
    dataField <<= 3;
    dataField |= bitField;
    dataField <<= 3;
    dataField |= bitField;

    requestFrameRateCtrl |= dataField;
    requestFrameRateCtrl <<= 8;
    crcValue = Crc8(requestFrameRateCtrl);
    requestFrameRateCtrl |= crcValue;
    SCHSpi48SendRequest(requestFrameRateCtrl);

    // Read back rate control register contents.
    SCHSpi48SendRequest(REQ_READ_RATE_CTRL);
    responseFrameRateCtrl = SCHSpi48SendRequest(REQ_READ_RATE_CTRL);

    // Check that return frame is not blank.
    if ((responseFrameRateCtrl == 0xFFFFFFFFFFFF) || (responseFrameRateCtrl == 0x00))
        return SCH_ERR_OTHER;

    // Check that Source Address matches Target Address.
    if (((requestFrameRateCtrl & TA_FIELD_MASK) >> 38) != ((responseFrameRateCtrl & SA_FIELD_MASK) >> 37))
        return SCH_ERR_OTHER;

    // Check that read and written data match.
    if ((requestFrameRateCtrl & DATA_FIELD_MASK) != (responseFrameRateCtrl & DATA_FIELD_MASK))
        return SCH_ERR_OTHER;

    return SCH_OK;
}

/**
 * Get rate sensitivities and decimation
 */
int SCHGetRateSensDec(uint16_t *sensRate1, uint16_t *sensRate2, uint16_t *decRate2)
{
    uint32_t dataField;
    uint64_t responseFrameRateCtrl;

    // Read Rate control register contents.
    SCHSpi48SendRequest(REQ_READ_RATE_CTRL);
    responseFrameRateCtrl = SCHSpi48SendRequest(REQ_READ_RATE_CTRL);

    // Check that return frame is not blank.
    if ((responseFrameRateCtrl == 0xFFFFFFFFFFFF) || (responseFrameRateCtrl == 0x00))
        return SCH_ERR_OTHER;

    // Check that Source Address matches Target Address.
    if (((REQ_READ_RATE_CTRL & TA_FIELD_MASK) >> 38) != ((responseFrameRateCtrl & SA_FIELD_MASK) >> 37))
        return SCH_ERR_OTHER;

    // First get the Rate_XYZ2 decimation
    dataField = (uint16_t)(responseFrameRateCtrl >> 8) & 0x07;
    *decRate2 = (uint16_t)SCHConvertBitfieldToDecimation(dataField);

    // Rate_XYZ2 sensitivity
    dataField = (uint16_t)(responseFrameRateCtrl >> 17) & 0x07;
    *sensRate2 = (uint16_t)SCHConvertBitfieldToRateSens(dataField);

    // Rate_XYZ1 sensitivity
    dataField = (uint16_t)(responseFrameRateCtrl >> 20) & 0x07;
    *sensRate1 = (uint16_t)SCHConvertBitfieldToRateSens(dataField);

    return SCH_OK;
}

/**
 * Set acc sensitivities and decimation
 */
int SCHSetAccSensDec(uint16_t sensAcc1, uint16_t sensAcc2, uint16_t sensAcc3, uint16_t decAcc2)
{
    uint32_t dataField;
    uint32_t bitField;
    uint64_t requestFrameAcc12Ctrl;
    uint64_t responseFrameAcc12Ctrl;
    uint64_t requestFrameAcc3Ctrl;
    uint64_t responseFrameAcc3Ctrl;
    uint8_t crcValue;

    if (SCHIsValidAccSens(sensAcc1) == false) {
        return SCH_ERR_INVALID_PARAM;
    }
    if (SCHIsValidAccSens(sensAcc2) == false) {
        return SCH_ERR_INVALID_PARAM;
    }
    if (SCHIsValidAccSens(sensAcc3) == false) {
        return SCH_ERR_INVALID_PARAM;
    }
    if (SCHIsValidDecimation(decAcc2) == false) {
        return SCH_ERR_INVALID_PARAM;
    }

    // Set sensitivities for Acc_XYZ1 (interpolated) and Acc_XYZ2 (decimated) outputs.
    // Also set decimation for Acc_XYZ2.
    requestFrameAcc12Ctrl = REQ_SET_ACC12_CTRL;
    dataField = SCHConvertAccSensToBitfield(sensAcc1);
    dataField <<= 3;
    bitField = SCHConvertAccSensToBitfield(sensAcc2);
    dataField |= bitField;
    dataField <<= 3;
    bitField = SCHConvertDecimationToBitfield(decAcc2);
    dataField |= bitField;
    dataField <<= 3;
    dataField |= bitField;
    dataField <<= 3;
    dataField |= bitField;

    requestFrameAcc12Ctrl |= dataField;
    requestFrameAcc12Ctrl <<= 8;
    crcValue = Crc8(requestFrameAcc12Ctrl);
    requestFrameAcc12Ctrl |= crcValue;
    SCHSpi48SendRequest(requestFrameAcc12Ctrl);

    // Set sensitivity for Acc_XYZ3 (interpolated) output.
    requestFrameAcc3Ctrl = REQ_SET_ACC3_CTRL;
    dataField = SCHConvertAccSensToBitfield(sensAcc3);
    requestFrameAcc3Ctrl |= dataField;
    requestFrameAcc3Ctrl <<= 8;
    crcValue = Crc8(requestFrameAcc3Ctrl);
    requestFrameAcc3Ctrl |= crcValue;
    SCHSpi48SendRequest(requestFrameAcc3Ctrl);

    // Read back sensitivity control register contents.
    SCHSpi48SendRequest(REQ_READ_ACC12_CTRL);
    responseFrameAcc12Ctrl = SCHSpi48SendRequest(REQ_READ_ACC3_CTRL);
    responseFrameAcc3Ctrl = SCHSpi48SendRequest(REQ_READ_ACC3_CTRL);

    // Check that return frame is not blank.
    if ((responseFrameAcc12Ctrl == 0xFFFFFFFFFFFF) || (responseFrameAcc12Ctrl == 0x00))
        return SCH_ERR_OTHER;
    if ((responseFrameAcc3Ctrl == 0xFFFFFFFFFFFF) || (responseFrameAcc3Ctrl == 0x00))
        return SCH_ERR_OTHER;

    // Check that Source Address matches Target Address.
    if (((requestFrameAcc12Ctrl & TA_FIELD_MASK) >> 38) != ((responseFrameAcc12Ctrl & SA_FIELD_MASK) >> 37))
        return SCH_ERR_OTHER;
    if (((requestFrameAcc3Ctrl & TA_FIELD_MASK) >> 38) != ((responseFrameAcc3Ctrl & SA_FIELD_MASK) >> 37))
        return SCH_ERR_OTHER;

    // Check that read and written data match.
    if ((requestFrameAcc12Ctrl & DATA_FIELD_MASK) != (responseFrameAcc12Ctrl & DATA_FIELD_MASK))
        return SCH_ERR_OTHER;
    if ((requestFrameAcc3Ctrl & DATA_FIELD_MASK) != (responseFrameAcc3Ctrl & DATA_FIELD_MASK))
        return SCH_ERR_OTHER;

    return SCH_OK;
}

/**
 * Get acc sensitivities and decimation
 */
int SCHGetAccSensDec(uint16_t *sensAcc1, uint16_t *sensAcc2, uint16_t *sensAcc3, uint16_t *decAcc2)
{
    uint32_t dataField;
    uint64_t responseFrameAcc12Ctrl;
    uint64_t responseFrameAcc3Ctrl;

    // Read Acc12 and Acc3 control register contents.
    SCHSpi48SendRequest(REQ_READ_ACC12_CTRL);
    responseFrameAcc12Ctrl = SCHSpi48SendRequest(REQ_READ_ACC3_CTRL);
    responseFrameAcc3Ctrl = SCHSpi48SendRequest(REQ_READ_ACC3_CTRL);

    // Check that return frame is not blank.
    if ((responseFrameAcc12Ctrl == 0xFFFFFFFFFFFF) || (responseFrameAcc12Ctrl == 0x00))
        return SCH_ERR_OTHER;
    if ((responseFrameAcc3Ctrl == 0xFFFFFFFFFFFF) || (responseFrameAcc3Ctrl == 0x00))
        return SCH_ERR_OTHER;

    // Check that Source Address matches Target Address.
    if (((REQ_READ_ACC12_CTRL & TA_FIELD_MASK) >> 38) != ((responseFrameAcc12Ctrl & SA_FIELD_MASK) >> 37))
        return SCH_ERR_OTHER;
    if (((REQ_READ_ACC3_CTRL & TA_FIELD_MASK) >> 38) != ((responseFrameAcc3Ctrl & SA_FIELD_MASK) >> 37))
        return SCH_ERR_OTHER;

    // First get the Acc_XYZ2 decimation
    dataField = (uint16_t)(responseFrameAcc12Ctrl >> 8) & 0x07;
    *decAcc2 = (uint16_t)SCHConvertBitfieldToDecimation(dataField);

    // Acc_XYZ2 sensitivity
    dataField = (uint16_t)(responseFrameAcc12Ctrl >> 17) & 0x07;
    *sensAcc2 = (uint16_t)SCHConvertBitfieldToAccSens(dataField);

    // Acc_XYZ1 sensitivity
    dataField = (uint16_t)(responseFrameAcc12Ctrl >> 20) & 0x07;
    *sensAcc1 = (uint16_t)SCHConvertBitfieldToAccSens(dataField);

    // Acc_XYZ3 sensitivity
    dataField = (uint16_t)(responseFrameAcc3Ctrl >> 8) & 0x07;
    *sensAcc3 = (uint16_t)SCHConvertBitfieldToAccSens(dataField);

    return SCH_OK;
}

/**
 * Enable/disable measurement and optionally set EOI
 */
int SCHEnableMeas(bool enableSensor, bool setEOI)
{
    uint64_t requestFrameModeCtrl;
    uint64_t responseFrameModeCtrl;
    uint8_t crcValue;

    requestFrameModeCtrl = REQ_SET_MODE_CTRL;

    // Handle EN_SENSOR -bit
    if (enableSensor)
        requestFrameModeCtrl |= 0x01;

    // Handle EOI_CTRL -bit
    if (setEOI)
        requestFrameModeCtrl |= 0x02;

    requestFrameModeCtrl <<= 8;
    crcValue = Crc8(requestFrameModeCtrl);
    requestFrameModeCtrl |= crcValue;
    SCHSpi48SendRequest(requestFrameModeCtrl);

    // Read back mode control register contents.
    SCHSpi48SendRequest(REQ_READ_MODE_CTRL);
    responseFrameModeCtrl = SCHSpi48SendRequest(REQ_READ_MODE_CTRL);

    // Check that return frame is not blank.
    if ((responseFrameModeCtrl == 0xFFFFFFFFFFFF) || (responseFrameModeCtrl == 0x00))
        return SCH_ERR_OTHER;

    // Check that Source Address matches Target Address.
    if (((requestFrameModeCtrl & TA_FIELD_MASK) >> 38) != ((responseFrameModeCtrl & SA_FIELD_MASK) >> 37))
        return SCH_ERR_OTHER;

    return SCH_OK;
}

/**
 * Configure DRY pin
 */
int SCHSetDry(int8_t polarity, bool enable)
{
    uint64_t requestFrameUserIfCtrl;
    uint64_t responseFrameUserIfCtrl;
    uint64_t dataContent;
    uint8_t crcValue;

    if ((polarity < -1) || (polarity > 1))
        return SCH_ERR_INVALID_PARAM;

    // Read USER_IF_CTRL -register content
    SCHSpi48SendRequest(REQ_READ_USER_IF_CTRL);
    responseFrameUserIfCtrl = SCHSpi48SendRequest(REQ_READ_USER_IF_CTRL);
    dataContent = (responseFrameUserIfCtrl & DATA_FIELD_MASK) >> 8;

    if (polarity == 0)
        dataContent &= (uint16_t)~0x40;   // Set DRY active high (0b01000000)
    else if (polarity == 1)
        dataContent |= 0x40;              // Set DRY active low

    if (enable)
        dataContent |= 0x20;              // Set DRY enabled (0b00100000)
    else
        dataContent &= (uint16_t)~0x20;   // Set DRY disabled

    requestFrameUserIfCtrl = REQ_SET_USER_IF_CTRL;
    requestFrameUserIfCtrl |= dataContent;
    requestFrameUserIfCtrl <<= 8;
    crcValue = Crc8(requestFrameUserIfCtrl);
    requestFrameUserIfCtrl |= crcValue;
    SCHSpi48SendRequest(requestFrameUserIfCtrl);

    // Read back USER_IF_CTRL register contents.
    SCHSpi48SendRequest(REQ_READ_USER_IF_CTRL);
    responseFrameUserIfCtrl = SCHSpi48SendRequest(REQ_READ_USER_IF_CTRL);

    // Check that return frame is not blank.
    if ((responseFrameUserIfCtrl == 0xFFFFFFFFFFFF) || (responseFrameUserIfCtrl == 0x00))
        return SCH_ERR_OTHER;

    // Check that Source Address matches Target Address.
    if (((requestFrameUserIfCtrl & TA_FIELD_MASK) >> 38) != ((responseFrameUserIfCtrl & SA_FIELD_MASK) >> 37))
        return SCH_ERR_OTHER;

    // Check that read and written data match.
    if ((requestFrameUserIfCtrl & DATA_FIELD_MASK) != (responseFrameUserIfCtrl & DATA_FIELD_MASK))
        return SCH_ERR_OTHER;

    return SCH_OK;
}

/**
 * Convert filter freq to bitfield
 */
uint32_t SCHConvertFilterToBitfield(uint32_t freq)
{
    switch (freq)
    {
        case 13:
            return 0x092;   // 010 010 010
        case 30:
            return 0x049;   // 001 001 001
        case 68:
            return 0x000;   // 000 000 000
        case 235:
            return 0x16D;   // 101 101 101
        case 280:
            return 0x0DB;   // 011 011 011
        case 370:
            return 0x124;   // 100 100 100
        case SCH_FILTER_BYPASS:
            return 0x1FF;   // 111 111 111, filter bypass mode
        default:
            return 0x000;
    }
}

/**
 * Convert rate sensitivity to bitfield
 */
uint32_t SCHConvertRateSensToBitfield(uint32_t sens)
{
    switch (sens)
    {
        case 1600:
            return 0x02;   // 010
        case 3200:
            return 0x03;   // 011
        case 6400:
            return 0x04;   // 100
        default:
            return 0x01;
    }
}

/**
 * Convert bitfield to rate sensitivity
 */
uint32_t SCHConvertBitfieldToRateSens(uint32_t bitfield)
{
    switch (bitfield)
    {
        case 0x02:          // 010
            return 1600;
        case 0x03:          // 011
            return 3200;
        case 0x04:          // 100
            return 6400;
        default:
            return 0x00;
    }
}

/**
 * Convert acc sensitivity to bitfield
 */
uint32_t SCHConvertAccSensToBitfield(uint32_t sens)
{
    switch (sens)
    {
        case 3200:
            return 0x01;   // 001
        case 6400:
            return 0x02;   // 010
        case 12800:
            return 0x03;   // 011
        case 25600:
            return 0x04;   // 100
        default:
            return 0x00;
    }
}

/**
 * Convert bitfield to acc sensitivity
 */
uint32_t SCHConvertBitfieldToAccSens(uint32_t bitfield)
{
    switch (bitfield)
    {
        case 0x01:          // 001
            return 3200;
        case 0x02:          // 010
            return 6400;
        case 0x03:          // 011
            return 12800;
        case 0x04:          // 100
            return 25600;
        default:
            return 0x00;
    }
}

/**
 * Convert decimation to bitfield
 */
uint32_t SCHConvertDecimationToBitfield(uint32_t decimation)
{
    switch (decimation)
    {
        case 2:
            return 0x00;   // 001
        case 4:
            return 0x01;   // 010
        case 8:
            return 0x02;   // 011
        case 16:
            return 0x03;   // 100
        case 32:
            return 0x04;   // 100
        default:
            return 0x00;
    }
}

/**
 * Convert bitfield to decimation
 */
uint32_t SCHConvertBitfieldToDecimation(uint32_t bitfield)
{
    switch (bitfield)
    {
        case 0x00:      // 001
            return 2;
        case 0x01:      // 010
            return 4;
        case 0x02:      // 011
            return 8;
        case 0x03:      // 100
            return 16;
        case 0x04:      // 100
            return 32;
        default:
            return 0x00;
    }
}

/**
 * Read status registers
 */
int32_t SCHGetStatus(SCHStatus *statusOut)
{
    if (statusOut == NULL) {
        return SCH_ERR_NULL_POINTER;
    }

    SCHSpi48SendRequest(REQ_READ_STAT_SUM);
    statusOut->summary     = SPI48_DATA_UINT16(SCHSpi48SendRequest(REQ_READ_STAT_SUM_SAT));
    statusOut->summarySat = SPI48_DATA_UINT16(SCHSpi48SendRequest(REQ_READ_STAT_COM));
    statusOut->common      = SPI48_DATA_UINT16(SCHSpi48SendRequest(REQ_READ_STAT_RATE_COM));
    statusOut->rateCommon = SPI48_DATA_UINT16(SCHSpi48SendRequest(REQ_READ_STAT_RATE_X));
    statusOut->rateX      = SPI48_DATA_UINT16(SCHSpi48SendRequest(REQ_READ_STAT_RATE_Y));
    statusOut->rateY      = SPI48_DATA_UINT16(SCHSpi48SendRequest(REQ_READ_STAT_RATE_Z));
    statusOut->rateZ      = SPI48_DATA_UINT16(SCHSpi48SendRequest(REQ_READ_STAT_ACC_X));
    statusOut->accX       = SPI48_DATA_UINT16(SCHSpi48SendRequest(REQ_READ_STAT_ACC_Y));
    statusOut->accY       = SPI48_DATA_UINT16(SCHSpi48SendRequest(REQ_READ_STAT_ACC_Z));
    statusOut->accZ       = SPI48_DATA_UINT16(SCHSpi48SendRequest(REQ_READ_STAT_ACC_Z));

    return SCH_OK;
}

/**
 * Verify status registers (all OK)
 */
bool SCHVerifyStatus(SCHStatus *statusIn)
{
    if (statusIn == NULL) {
        return SCH_ERR_NULL_POINTER;
    }

    if (statusIn->summary != 0xffff)
        return false;
    if (statusIn->summarySat != 0xffff)
        return false;
    if (statusIn->common != 0xffff)
        return false;
    if (statusIn->rateCommon != 0xffff)
        return false;
    if (statusIn->rateX != 0xffff)
        return false;
    if (statusIn->rateY != 0xffff)
        return false;
    if (statusIn->rateZ != 0xffff)
        return false;
    if (statusIn->accX != 0xffff)
        return false;
    if (statusIn->accY != 0xffff)
        return false;
    if (statusIn->accZ != 0xffff)
        return false;

    return true;
}

/**
 * Read serial number
 */
char* SCHGetSnbr(void)
{
    uint16_t snId1;
    uint16_t snId2;
    uint16_t snId3;
    static char strBuffer[15];

    SCHSpi48SendRequest(REQ_READ_SN_ID1);
    snId1 = SPI48_DATA_UINT16(SCHSpi48SendRequest(REQ_READ_SN_ID2));
    snId2 = SPI48_DATA_UINT16(SCHSpi48SendRequest(REQ_READ_SN_ID3));
    snId3 = SPI48_DATA_UINT16(SCHSpi48SendRequest(REQ_READ_SN_ID3));

    // Build serial number string
    snprintf(strBuffer, 14, "%05d%01X%04X", snId2, snId1 & 0x000F, snId3);

    return strBuffer;
}

/**
 * CRC8 calculation for 48-bit SPI frame
 */
static uint8_t Crc8(uint64_t spiFrame)
{
    uint64_t data = spiFrame & 0xFFFFFFFFFF00LL;
    uint8_t crc = 0xFF;

    for (int i = 47; i >= 0; i--)
    {
        uint8_t dataBit = (data >> i) & 0x01;
        crc = crc & 0x80 ? (uint8_t)((crc << 1) ^ 0x2F) ^ dataBit : (uint8_t)(crc << 1) | dataBit;
    }

    return crc;
}

/**
 * Check CRC8
 */
bool SCHCheckCrc8(uint64_t spiFrame)
{
    if ((uint8_t)(spiFrame & 0xff) == Crc8(spiFrame))
        return true;
    else
        return false;
}

/**
 * CRC3 calculation for 32-bit SPI frame
 */
static uint8_t Crc3(uint32_t spiFrame)
{
    uint32_t data = spiFrame & 0xFFFFFFF8;
    uint8_t crc = 0x05;

    for (int i = 31; i >= 0; i--)
    {
        uint8_t dataBit = (data >> i) & 0x01;
        crc = crc & 0x4 ? (uint8_t)((crc << 1) ^ 0x3) ^ dataBit : (uint8_t)(crc << 1) | dataBit;
        crc &= 0x07;
    }

    return crc;
}

/**
 * Check CRC3
 */
bool SCHCheckCrc3(uint32_t spiFrame)
{
    if ((uint8_t)(spiFrame & 0x07) == Crc3(spiFrame))
        return true;
    else
        return false;
}

/**
 * Initialize sensor (keeps original logic)
 */
int32_t SCHInit(SCHFilter sFilter, SCHSensitivity sSensitivity, SCHDecimation sDecimation, bool enableDry)
{
    int ret = SCH_OK;
    uint8_t startupAttempt = 0;
    bool sch1status = false;
    SCHStatus sch1statusAll;

    // SCH1 startup sequence specified in section "5 Component Operation,
    // Reset and Power Up" in the data sheet.

    SCHReset(); // Reset sensor

    for (startupAttempt = 0; startupAttempt < 2; startupAttempt++) {

        // Wait 32 ms for the non-volatile memory (NVM) Read
        HAL_Delay(32);

        // Set user controls
        SCHSetFilters(sFilter.rate, sFilter.acc, sFilter.acc3);
        SCHSetRateSensDec(sSensitivity.rate1, sSensitivity.rate2, sDecimation.rate2);
        SCHSetAccSensDec(sSensitivity.acc1, sSensitivity.acc2, sSensitivity.acc3, sDecimation.acc2);
        if (enableDry)
            SCHSetDry(0, true);   // 0 = DRY active high
        else
            SCHSetDry(0, false);

        // Write EN_SENSOR = 1
        SCHEnableMeas(true, false);

        // Wait 215 ms
        HAL_Delay(215);

        // Read all status registers once. No critization
        SCHGetStatus(&sch1statusAll);

        // Write EOI = 1 (End of Initialization command)
        SCHEnableMeas(true, true);

        // b 3 ms
        HAL_Delay(3);

        // Read all status registers twice.
        SCHGetStatus(&sch1statusAll);
        SCHGetStatus(&sch1statusAll);

        // Read all user control registers and verify content - Add verification here if needed for FuSa.

        // Check that all status registers have OK status.
        if (!SCHVerifyStatus(&sch1statusAll)) {
            sch1status = false;
            SCHReset();    // Sensor failed, reset and retry.
        }
        else {
            sch1status = true;
            break;
        }

    } // for (startupAttempt = 0; startupAttempt < 2; startupAttempt++)

    if (sch1status != true)
        ret = SCH_ERR_SENSOR_INIT;

    return ret;
}

/**
 * Read rate, acceleration and temperature data (Rate1/Acc1/Temp)
 */
void SCHGetData(SCHRawData *data)
{
    SCHSpi48SendRequest(REQ_READ_RATE_X1);
    uint64_t rateXRaw = SCHSpi48SendRequest(REQ_READ_RATE_Y1);
    uint64_t rateYRaw = SCHSpi48SendRequest(REQ_READ_RATE_Z1);
    uint64_t rateZRaw = SCHSpi48SendRequest(REQ_READ_ACC_X1);
    uint64_t accXRaw  = SCHSpi48SendRequest(REQ_READ_ACC_Y1);
    uint64_t accYRaw  = SCHSpi48SendRequest(REQ_READ_ACC_Z1);
    uint64_t accZRaw  = SCHSpi48SendRequest(REQ_READ_TEMP);
    uint64_t tempRaw  = SCHSpi48SendRequest(REQ_READ_TEMP);

    // Get possible frame errors
    uint64_t misoWords[] = {rateXRaw, rateYRaw, rateZRaw, accXRaw, accYRaw, accZRaw, tempRaw};
    data->frameError = SCHCheck48BitFrameError(misoWords, (sizeof(misoWords) / sizeof(uint64_t)));

    // Parse MISO data to structure
    data->rate1Raw[AXIS_X] = SPI48_DATA_INT32(rateXRaw);
    data->rate1Raw[AXIS_Y] = SPI48_DATA_INT32(rateYRaw);
    data->rate1Raw[AXIS_Z] = SPI48_DATA_INT32(rateZRaw);
    data->acc1Raw[AXIS_X]  = SPI48_DATA_INT32(accXRaw);
    data->acc1Raw[AXIS_Y]  = SPI48_DATA_INT32(accYRaw);
    data->acc1Raw[AXIS_Z]  = SPI48_DATA_INT32(accZRaw);

    // Temperature data is always 16 bits wide. Drop 4 LSBs as they are not used.
    data->tempRaw = SPI48_DATA_INT32(tempRaw) >> 4;
}

/**
 * Read rate2/acc2 (decimated) data
 */
void SCHGetData2(SCHRawData *data)
{
    SCHSpi48SendRequest(REQ_READ_RATE_X2);
    uint64_t rateXRaw = SCHSpi48SendRequest(REQ_READ_RATE_Y2);
    uint64_t rateYRaw = SCHSpi48SendRequest(REQ_READ_RATE_Z2);
    uint64_t rateZRaw = SCHSpi48SendRequest(REQ_READ_ACC_X2);
    uint64_t accXRaw  = SCHSpi48SendRequest(REQ_READ_ACC_Y2);
    uint64_t accYRaw  = SCHSpi48SendRequest(REQ_READ_ACC_Z2);
    uint64_t accZRaw  = SCHSpi48SendRequest(REQ_READ_TEMP);
    uint64_t tempRaw  = SCHSpi48SendRequest(REQ_READ_TEMP);

    // Get possible frame errors
    uint64_t misoWords[] = {rateXRaw, rateYRaw, rateZRaw, accXRaw, accYRaw, accZRaw, tempRaw};
    data->frameError = SCHCheck48BitFrameError(misoWords, (sizeof(misoWords) / sizeof(uint64_t)));

    // Parse MISO data to structure
    data->rate2Raw[AXIS_X] = SPI48_DATA_INT32(rateXRaw);
    data->rate2Raw[AXIS_Y] = SPI48_DATA_INT32(rateYRaw);
    data->rate2Raw[AXIS_Z] = SPI48_DATA_INT32(rateZRaw);
    data->acc2Raw[AXIS_X]  = SPI48_DATA_INT32(accXRaw);
    data->acc2Raw[AXIS_Y]  = SPI48_DATA_INT32(accYRaw);
    data->acc2Raw[AXIS_Z]  = SPI48_DATA_INT32(accZRaw);
}

/**
 * Convert raw summed data to scaled results
 */
void SCHConvertData(SCHRawData *dataIn, SCHResult *dataOut)
{
    // Convert from raw counts to sensitivity and calculate averages here for faster execution
    dataOut->rate1[AXIS_X] = (float)dataIn->rate1Raw[AXIS_X] / (SENSITIVITY_RATE1 * (float)AVG_FACTOR);
    dataOut->rate1[AXIS_Y] = (float)dataIn->rate1Raw[AXIS_Y] / (SENSITIVITY_RATE1 * (float)AVG_FACTOR);
    dataOut->rate1[AXIS_Z] = (float)dataIn->rate1Raw[AXIS_Z] / (SENSITIVITY_RATE1 * (float)AVG_FACTOR);
    dataOut->acc1[AXIS_X]  = (float)dataIn->acc1Raw[AXIS_X] / (SENSITIVITY_ACC1 * (float)AVG_FACTOR);
    dataOut->acc1[AXIS_Y]  = (float)dataIn->acc1Raw[AXIS_Y] / (SENSITIVITY_ACC1 * (float)AVG_FACTOR);
    dataOut->acc1[AXIS_Z]  = (float)dataIn->acc1Raw[AXIS_Z] / (SENSITIVITY_ACC1 * (float)AVG_FACTOR);

    // Convert Rate2 and Acc2
    dataOut->rate2[AXIS_X] = (float)dataIn->rate2Raw[AXIS_X] / (SENSITIVITY_RATE1 * (float)AVG_FACTOR);
    dataOut->rate2[AXIS_Y] = (float)dataIn->rate2Raw[AXIS_Y] / (SENSITIVITY_RATE1 * (float)AVG_FACTOR);
    dataOut->rate2[AXIS_Z] = (float)dataIn->rate2Raw[AXIS_Z] / (SENSITIVITY_RATE1 * (float)AVG_FACTOR);
    dataOut->acc2[AXIS_X]  = (float)dataIn->acc2Raw[AXIS_X] / (SENSITIVITY_ACC1 * (float)AVG_FACTOR);
    dataOut->acc2[AXIS_Y]  = (float)dataIn->acc2Raw[AXIS_Y] / (SENSITIVITY_ACC1 * (float)AVG_FACTOR);
    dataOut->acc2[AXIS_Z]  = (float)dataIn->acc2Raw[AXIS_Z] / (SENSITIVITY_ACC1 * (float)AVG_FACTOR);

    // Convert temperature and calculate average
    dataOut->temp = GET_TEMPERATURE((float)dataIn->tempRaw / (float)AVG_FACTOR);
}

/**
 * Check 48-bit MISO frames for error bits
 */
bool SCHCheck48BitFrameError(uint64_t *data, int size)
{
    for (int i = 0; i < size; i++)
    {
        uint64_t value = data[i];
        if (value & ERROR_FIELD_MASK)
            return true;
    }

    return false;
}
