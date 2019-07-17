#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "driver/timer.h"

#include "motion.h"
#include "variables.h"


#define GPIO_VSPI_MISO 21
#define GPIO_VSPI_MOSI 19
#define GPIO_VSPI_CLK  18
#define GPIO_VSPI_CS   5

#define READ_FLAG    0x80

#define TIMER_DIVIDER  16  //  Hardware timer clock divider
#define TIMER_SCALE    (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_GROUP    TIMER_GROUP_0
#define TIMER_ID       TIMER_1

const uint8_t ADDR_WHO_AM_I = 0x00;
const uint8_t ADDR_PWR_MGMT_1 = 0x06;
const uint8_t ADDR_PWR_MGMT_2 = 0x07;
const uint8_t ADDR_REG_BANK_SEL = 0x7F;
const uint8_t ADDR2_GYRO_CONFIG1 = 0x01;
const uint8_t ADDR2_ACCEL_CONFIG = 0x14;

const uint8_t GYRO_FS_SEL = 1;
const int ACCEL_SENSITIVITY = 16384; // LSB/g when ACCEL_FS=0
const float GYRO_SENSITIVITY[4] = {131, 65.5, 32.8, 16.4}; // LSB/(dps) when GYRO_FS_SEL=1

const uint8_t ADDR_ACCEL_OUT_H[AXIS_NUM] = {0x2d, 0x2f, 0x31};
const uint8_t ADDR_ACCEL_OUT_L[AXIS_NUM] = {0x2e, 0x30, 0x32};

const uint8_t ADDR_GYRO_OUT_H[AXIS_NUM] = {0x33, 0x35, 0x37};
const uint8_t ADDR_GYRO_OUT_L[AXIS_NUM] = {0x34, 0x36, 0x38};


inline float to_radians(float degrees) {
    return degrees * (M_PI / 180.0);
}

uint8_t read1byte(spi_device_handle_t spi, const uint8_t address){
    // １バイト読み込み

    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       // Zero out the transaction
    t.length = 16;                  // SPI_ADDRESS(8bit) + SPI_DATA(8bit)
    t.flags = SPI_TRANS_USE_RXDATA;

    uint16_t tx_data = (address | READ_FLAG) << 8;
    tx_data = SPI_SWAP_DATA_TX(tx_data , 16);
    t.tx_buffer = &tx_data;

    ret=spi_device_polling_transmit(spi, &t);  // Transmit!
    assert(ret==ESP_OK);            // Should have had no issues.

    uint8_t data = SPI_SWAP_DATA_RX(*(uint16_t*)t.rx_data, 16) & 0x00FF; // FF + Data

    return data;
}

uint8_t write1byte(spi_device_handle_t spi, const uint8_t address, const uint8_t send_data){
    // １バイト読み込み

    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       // Zero out the transaction
    t.length = 16;                  // SPI_ADDRESS(8bit) + SPI_DATA(8bit)
    t.flags = SPI_TRANS_USE_RXDATA;

    uint16_t tx_data = (address << 8) | send_data;
    tx_data = SPI_SWAP_DATA_TX(tx_data , 16);
    t.tx_buffer = &tx_data;

    ret=spi_device_polling_transmit(spi, &t);  // Transmit!
    assert(ret==ESP_OK);            // Should have had no issues.

    uint8_t data = SPI_SWAP_DATA_RX(*(uint16_t*)t.rx_data, 16) & 0x00FF; // FF + Data

    return data;
}

void device_init(spi_device_handle_t spi){
    // Who AM I
    uint8_t mpu_id = read1byte(spi, ADDR_WHO_AM_I);
    printf("MPU ID: %02X\n", mpu_id);

    // SLEEP未解除確認
    uint8_t recv_data = read1byte(spi, ADDR_PWR_MGMT_1);
    printf("before PWR_MGMT_1: %02X\n", recv_data);

    // USER BANKの切り替え
    uint8_t send_data = 0x02 << 4;
    recv_data = write1byte(spi, ADDR_REG_BANK_SEL, send_data);
    recv_data = read1byte(spi, ADDR_REG_BANK_SEL);
    printf("BANK SEL: %02X\n", recv_data);
    
    // GYROの設定
    send_data = 0x00 | (GYRO_FS_SEL << 1);
    recv_data = write1byte(spi, ADDR2_GYRO_CONFIG1, send_data);
    recv_data = read1byte(spi, ADDR2_GYRO_CONFIG1);
    printf("GYRO CONFIG: %02X\n", recv_data);

    // USER BANKの切り替え
    send_data = 0x00 << 4;
    recv_data = write1byte(spi, ADDR_REG_BANK_SEL, send_data);

    // SLEEP解除
    send_data = 0x01;
    recv_data = write1byte(spi, ADDR_PWR_MGMT_1, send_data);

    // SLEEP解除確認
    recv_data = read1byte(spi, ADDR_PWR_MGMT_1);
    printf("after PWR_MGMT_1: %02X\n", recv_data);

}

float get_accel(spi_device_handle_t spi, enum AXIS axis){
    uint8_t accel_h = read1byte(spi, ADDR_ACCEL_OUT_H[axis]);
    uint8_t accel_l = read1byte(spi, ADDR_ACCEL_OUT_L[axis]);

    // printf("AXIS: %d: accel_h, accel_l: %02X, %02X\n", axis, accel_h, accel_l);
    int16_t accel = (accel_h << 8) | accel_l;

    return (float)accel / ACCEL_SENSITIVITY;
}

float get_gyro(spi_device_handle_t spi, enum AXIS axis){
    uint8_t gyro_h = read1byte(spi, ADDR_GYRO_OUT_H[axis]);
    uint8_t gyro_l = read1byte(spi, ADDR_GYRO_OUT_L[axis]);

    int16_t gyro = (gyro_h << 8) | gyro_l;

    return (float)gyro / GYRO_SENSITIVITY[GYRO_FS_SEL];
}

void updateMeasuredAngle(const float gyroZ, const double currentTime){
    static double prevTime;

    double diffTime = currentTime - prevTime;

    gMeasuredAngle += diffTime * gyroZ;

    prevTime = currentTime;
}

void TaskReadMotion(void *arg){
    esp_err_t ret;
    spi_device_handle_t spi;
    spi_bus_config_t buscfg={
        .miso_io_num=GPIO_VSPI_MISO,
        .mosi_io_num=GPIO_VSPI_MOSI,
        .sclk_io_num=GPIO_VSPI_CLK,
        .quadwp_io_num=-1, // unused
        .quadhd_io_num=-1, // unused
        .max_transfer_sz=4 // bytes
    };
    spi_device_interface_config_t devcfg={
        .clock_speed_hz=7*1000*1000,   //Clock out at 500 kHz
        .mode=3,                    //SPI mode 3
        .spics_io_num=GPIO_VSPI_CS,   //CS pin
        .queue_size=7,              //We want to be able to queue 7 transactions at a time
    };
    //Initialize the SPI bus
    ret=spi_bus_initialize(VSPI_HOST, &buscfg, 2);
    ESP_ERROR_CHECK(ret);
    ret=spi_bus_add_device(VSPI_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);

    device_init(spi);

    // Initialize Timer
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_DIS;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = 1;
    timer_init(TIMER_GROUP, TIMER_ID, &config);
    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP, TIMER_ID, 0x00000000ULL);
    timer_start(TIMER_GROUP, TIMER_ID);

    double currentTime;

    while(1){
        gAccel[AXIS_X] = get_accel(spi, AXIS_X);
        gAccel[AXIS_Y] = get_accel(spi, AXIS_Y);
        gAccel[AXIS_Z] = get_accel(spi, AXIS_Z);

        gGyro[AXIS_X] = to_radians(get_gyro(spi, AXIS_X));
        gGyro[AXIS_Y] = to_radians(get_gyro(spi, AXIS_Y));
        gGyro[AXIS_Z] = to_radians(get_gyro(spi, AXIS_Z));

        // 時間取得
        timer_get_counter_time_sec(TIMER_GROUP, TIMER_ID, &currentTime);
        updateMeasuredAngle(gGyro[AXIS_Z], currentTime);

        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}
