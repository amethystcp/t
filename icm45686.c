#include "icm45686.h"

volatile uint8_t debug_spi_ok = 0;
volatile uint8_t debug_whoami = 0;

/* ฟังก์ชันดึงขา CS ลง (Low) และขึ้น (High) อ้างอิงชื่อตามที่ตั้งใน CubeMX */
static inline void CS_Low(void)  {
    HAL_GPIO_WritePin(ICM45686_CS_GPIO_Port, ICM45686_CS_Pin, GPIO_PIN_RESET);
}
static inline void CS_High(void) {
    HAL_GPIO_WritePin(ICM45686_CS_GPIO_Port, ICM45686_CS_Pin, GPIO_PIN_SET);
}

/**
  * @brief  ฟังก์ชันสำหรับเขียนข้อมูลลง Register 1 Byte
  */
static void ICM45686_WriteReg(SPI_HandleTypeDef *hspi, uint8_t reg, uint8_t val)
{
    /* บิตที่ 7 เป็น 0 หมายถึง Write */
    uint8_t tx[2] = { reg & 0x7F, val };

    CS_Low();
    HAL_SPI_Transmit(hspi, tx, 2, 10);
    CS_High();
}

/**
  * @brief  ฟังก์ชันสำหรับอ่านข้อมูลจาก Register 1 Byte
  */
static uint8_t ICM45686_ReadReg(SPI_HandleTypeDef *hspi, uint8_t reg)
{
    /* บิตที่ 7 เป็น 1 หมายถึง Read */
    uint8_t tx[2] = { reg | 0x80, 0x00 };
    uint8_t rx[2] = { 0, 0 };

    CS_Low();
    HAL_SPI_TransmitReceive(hspi, tx, rx, 2, 10);
    CS_High();

    debug_spi_ok = 1;
    return rx[1];
}

/**
  * @brief  ฟังก์ชันเริ่มต้นการทำงานของเซนเซอร์ ICM45686
  */
HAL_StatusTypeDef ICM45686_Init(SPI_HandleTypeDef *hspi)
{
    /* รอให้ระบบเซนเซอร์เสถียรหลังเปิดเครื่อง */
    HAL_Delay(50);

    /* อ่านค่า WHO_AM_I จาก Address 0x72 เพื่อตรวจสอบว่าสื่อสารกับชิปได้ */
    debug_whoami = ICM45686_ReadReg(hspi, 0x72);

    /* 1. เปิดโหมด Low Noise (0x0F) ให้กับทั้ง Gyro และ Accel ที่ Address 0x10 */
    ICM45686_WriteReg(hspi, 0x10, 0x0F);
    HAL_Delay(50);

    /* 2. ตั้งค่า ACCEL_CONFIG0 (Address 0x1B) เป็น 0x37 (±16g, 200Hz) */
    ICM45686_WriteReg(hspi, 0x1B, 0x37);
    HAL_Delay(10);

    /* 3. ตั้งค่า GYRO_CONFIG0 (Address 0x1C) เป็น 0x17 (±2000dps, 200Hz) */
    ICM45686_WriteReg(hspi, 0x1C, 0x17);
    HAL_Delay(50);

    /* เช็คความถูกต้อง ถ้า Who Am I ไม่ตรง 0xE9 ให้ส่ง Error */
    if (debug_whoami != 0xE9) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

/**
  * @brief  ฟังก์ชันอ่านข้อมูล 6 แกนรวดเดียว 12 Bytes
  */
HAL_StatusTypeDef ICM45686_ReadData(SPI_HandleTypeDef *hspi, ICM45686_Data_t *data)
{
    /* Array ขนาด 13 บัฟเฟอร์: 1 ไบต์แรกส่ง Address + 12 ไบต์หลังสำหรับรับข้อมูล */
    uint8_t tx[13] = {0};
    uint8_t rx[13] = {0};

    /* Register ข้อมูลเริ่มต้นที่ 0x00 (ACCEL_DATA_X1_UI)
     * OR กับ 0x80 (บิตที่ 7 เป็น 1) เพื่อส่งคำสั่งอ่าน */
    tx[0] = 0x00 | 0x80;

    /* เปิดการสื่อสาร ดึงและรับข้อมูลพร้อมกันรวดเดียว */
    CS_Low();
    HAL_SPI_TransmitReceive(hspi, tx, rx, 13, 20);
    CS_High();

    /* ประกอบข้อมูลดิบแบบ 16 บิต (Big-Endian: High byte ขึ้นก่อน ตามที่ระบุ) */
    /* ข้าม rx[0] ไปเพราะเป็นช่วงที่ส่ง Address ข้อมูลจะเริ่มที่ rx[1] */
    int16_t raw_ax = (int16_t)((rx[1]  << 8) | rx[2]);   // 0x00: Accel X High, 0x01: Accel X Low
    int16_t raw_ay = (int16_t)((rx[3]  << 8) | rx[4]);   // 0x02: Accel Y High, 0x03: Accel Y Low
    int16_t raw_az = (int16_t)((rx[5]  << 8) | rx[6]);   // 0x04: Accel Z High, 0x05: Accel Z Low
    int16_t raw_gx = (int16_t)((rx[7]  << 8) | rx[8]);   // 0x06: Gyro X High , 0x07: Gyro X Low
    int16_t raw_gy = (int16_t)((rx[9]  << 8) | rx[10]);  // 0x08: Gyro Y High , 0x09: Gyro Y Low
    int16_t raw_gz = (int16_t)((rx[11] << 8) | rx[12]);  // 0x0A: Gyro Z High , 0x0B: Gyro Z Low

    /* แปลงค่าดิบให้เป็นหน่วย g และ dps */
    /* ±16g = 2048 LSB/g, ±2000dps = 16.4 LSB/dps */
    data->accel_x = (float)raw_ax / 2048.0f;
    data->accel_y = (float)raw_ay / 2048.0f;
    data->accel_z = (float)raw_az / 2048.0f;

    data->gyro_x = (float)raw_gx / 16.4f;
    data->gyro_y = (float)raw_gy / 16.4f;
    data->gyro_z = (float)raw_gz / 16.4f;

    return HAL_OK;
}
