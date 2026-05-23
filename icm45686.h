#ifndef ICM45686_H
#define ICM45686_H

#include "main.h"

/* โครงสร้างสำหรับเก็บข้อมูลทั้ง 6 แกน */
typedef struct {
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
} ICM45686_Data_t;

/* ฟังก์ชันที่เราจะเรียกใช้ */
HAL_StatusTypeDef ICM45686_Init(SPI_HandleTypeDef *hspi);
HAL_StatusTypeDef ICM45686_ReadData(SPI_HandleTypeDef *hspi, ICM45686_Data_t *data);

#endif /* ICM45686_H */
