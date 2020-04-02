// C++プロジェクトのサンプル
// Ref:https://github.com/espressif/esp-idf/tree/release/v3.3/examples/system/cpp_exceptions
// Ref:https://github.com/espressif/esp-idf/tree/release/v3.3/examples/system/cpp_pthread

#include <iostream>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "icm20648.h"

const static gpio_num_t GPIO_MOSI = GPIO_NUM_19;
const static gpio_num_t GPIO_MISO = GPIO_NUM_21;
const static gpio_num_t GPIO_SCLK = GPIO_NUM_18;
const static gpio_num_t GPIO_CS = GPIO_NUM_5;

/* Inside .cpp file, app_main function must be declared with C linkage */
extern "C" void app_main(){

    icm20648 driver(GPIO_MOSI, GPIO_MISO, GPIO_SCLK, GPIO_CS);

    std::cout<< "WHO AM I:" << std::hex << driver.readWhoAmI() << std::endl;
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    while(1){
        std::cout << "AccelX:" << driver.getAccelX() << "g" << std::endl;
        std::cout << "AccelY:" << driver.getAccelY() << "g" << std::endl;
        std::cout << "AccelZ:" << driver.getAccelZ() << "g" << std::endl;
        // std::cout << "GyroX:" << driver.getGyroX()<< "dps" << std::endl;
        // std::cout << "GyroY:" << driver.getGyroY()<< "dps" << std::endl;
        // std::cout << "GyroZ:" << driver.getGyroZ()<< "dps" << std::endl;

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}