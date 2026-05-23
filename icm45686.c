#include "icm45686.h"

/* ตัวแปรไว้สำหรับ Debug ดูสถานะใน Live Expressions */
volatile uint8_t debug_spi_ok = 0;
volatile uint8_t debug_whoami = 0;

/* ฟังก์ชันดึงขา CS ลงและขึ้น (อ้างอิงชื่อตามที่ตั้งใน CubeMX) */
static inline void CS_Low(void)  {
    HAL_GPIO_WritePin(ICM45686_CS_GPIO_Port, ICM45686_CS_Pin, GPIO_PIN_RESET);
}
static inline void CS_High(void) {
    HAL_GPIO_WritePin(ICM45686_CS_GPIO_Port, ICM45686_CS_Pin, GPIO_PIN_SET);
}

/* ฟังก์ชันเขียนค่าลง Register 1 Byte */
static void ICM45686_WriteReg(SPI_HandleTypeDef *hspi, uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { reg & 0x7F, val }; /* bit ที่ 7 เป็น 0 หมายถึง Write */
    CS_Low();
    HAL_SPI_Transmit(hspi, tx, 2, 10);
    CS_High();
}

/* ฟังก์ชันอ่านค่าจาก Register 1 Byte */
static uint8_t ICM45686_ReadReg(SPI_HandleTypeDef *hspi, uint8_t reg)
{
    uint8_t tx[2] = { reg | 0x80, 0x00 }; /* bit ที่ 7 เป็น 1 หมายถึง Read */
    uint8_t rx[2] = { 0, 0 };

    CS_Low();
    HAL_SPI_TransmitReceive(hspi, tx, rx, 2, 10);
    CS_High();

    debug_spi_ok = 1;
    return rx[1];
}

/* ฟังก์ชันเริ่มต้นการทำงานของเซนเซอร์ */
HAL_StatusTypeDef ICM45686_Init(SPI_HandleTypeDef *hspi)
{
    HAL_Delay(50);

    /* อ่านค่า WHO_AM_I (Address 0x72) เพื่อทดสอบการสื่อสาร */
    debug_whoami = ICM45686_ReadReg(hspi, 0x72);

    /* 1. เปิดโหมด Low Noise สำหรับ Gyro และ Accel (Address 0x10) */
    ICM45686_WriteReg(hspi, 0x10, 0x0F);
    HAL_Delay(50);

    /* 2. ตั้งค่า ACCEL_CONFIG0 */
    ICM45686_WriteReg(hspi, 0x1B, 0x37);
    HAL_Delay(10);

    /* 3. ตั้งค่า GYRO_CONFIG0 (±2000dps, 200Hz) */
    ICM45686_WriteReg(hspi, 0x1C, 0x17);
    HAL_Delay(50);

    /* ตรวจสอบ ID ของชิป (ต้องเป็น 0xE9 หรือ 233) */
    if (debug_whoami != 0xE9) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

/* ฟังก์ชันอ่านข้อมูลรวดเดียว 12 Bytes */
HAL_StatusTypeDef ICM45686_ReadData(SPI_HandleTypeDef *hspi, ICM45686_Data_t *data)
{
    /* Array ขนาด 13: 1 Byte ส่ง Address + 12 Bytes รับข้อมูล 6 แกน */
    uint8_t tx[13] = {0};
    uint8_t rx[13] = {0};

    /* Address 0x00 คือจุดเริ่มต้นของข้อมูล (OR 0x80 เพื่อสั่งอ่าน) */
    tx[0] = 0x00 | 0x80;

    CS_Low();
    HAL_SPI_TransmitReceive(hspi, tx, rx, 13, 20);
    CS_High();

    /* การประกอบข้อมูลแบบ Little Endian (Default ของ ICM-45686) */
    int16_t raw_ax = (int16_t)((rx[2]  << 8) | rx[1]);
    int16_t raw_ay = (int16_t)((rx[4]  << 8) | rx[3]);
    int16_t raw_az = (int16_t)((rx[6]  << 8) | rx[5]);

    /* -------- แก้ไขตรงนี้: สลับข้อมูลระหว่าง Gyro X และ Gyro Y -------- */
    /* นำ byte ของแกน Y (rx[9], rx[10]) มาคำนวณใส่ตัวแปร X */
    int16_t raw_gx = (int16_t)((rx[10] << 8) | rx[9]);

    /* นำ byte ของแกน X (rx[7], rx[8]) มาคำนวณใส่ตัวแปร Y */
    int16_t raw_gy = (int16_t)((rx[8]  << 8) | rx[7]);
    /* ---------------------------------------------------------------- */

    int16_t raw_gz = (int16_t)((rx[12] << 8) | rx[11]);

    /* แปลงค่าดิบให้กลายเป็นหน่วยจริง
     * Accel: ใช้ตัวหาร 8192.0f เพื่อปรับสเกลให้ได้ค่าแรงโน้มถ่วง (1.0g) ที่แม่นยำ
     * Gyro : ใช้ตัวหาร 16.4f สำหรับ ±2000dps
     */
    data->accel_x = (float)raw_ax / 8192.0f;
    data->accel_y = (float)raw_ay / 8192.0f;
    data->accel_z = (float)raw_az / 8192.0f;

    data->gyro_x = (float)raw_gx / 16.4f;
    data->gyro_y = (float)raw_gy / 16.4f;
    data->gyro_z = (float)raw_gz / 16.4f;

    return HAL_OK;
}
