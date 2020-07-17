#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

// ESP公式マニュアル:
// Ref: https://docs.espressif.com/projects/esp-idf/en/v3.3.1/api-reference/peripherals/spi_master.html
// MA702データシート:
// https://www.monolithicpower.com/en/documentview/productdocument/index/version/2/document_type/Datasheet/lang/en/sku/MA702/

enum SIDE{
    LEFT = 0,
    RIGHT,
    SIDE_SIZE
};

const size_t DATA_LENGTH = 16;

uint16_t readData(const spi_device_handle_t spidev){
    // MA702から16ビットデータを受け取る
    
    // MA702からはCSピンをLOWにするたびにデータを取得できる
    // そのためSPI MASTERはデータを送信しなくてよい
    uint16_t recv_data;
    spi_transaction_t trans;
    memset(&trans, 0, sizeof(trans)); // 構造体をゼロで初期化
    // flags: SPI_TRANS_ではじまるフラグを設定できる
    // trans.flags = SPI_TRANS_USE_RXDATA;
    // trans.cmd = NULL;
    // trans.addr = NULL;
    trans.length = DATA_LENGTH; // データ長 bit
    // trans.rxlength = 16; // デフォルトでlengthと同じになるので設定不要　
    // trans.user = NULL; // ユーザ定義の変数、コールバックを使うときに役立つ
    // trans.tx_buffer = &tx_data; // 送信バッファのポインタ
    // trans.tx_data; // SPI_TRANS_USE_TXDATAフラグを立てれば使用可能
    trans.rx_buffer = &recv_data; // 受信バッファのポインタ
    // trans.rx_data; // SPI_TRANS_USE_RXDATAのフラグを立てれば使用可能

    // 通信開始
    esp_err_t ret;
    ret=spi_device_polling_transmit(spidev, &trans);
    assert(ret==ESP_OK);

    // ESP32はリトルエンディアンで設計されているので
    // ABCD という16bitデータは、CDABという順番でバッファに保存される
    // SPI_SWAP_DATA_RXを使えば、データ順序を入れ変えられる
    return SPI_SWAP_DATA_RX(recv_data, DATA_LENGTH);
}

uint16_t sendCmdAddrData(const spi_device_handle_t spidev,
        const uint8_t command, const uint8_t address, const uint8_t data){
    // MA702にコマンドとアドレスとデータを送信する

    // コマンド送信時に受け取るデータは角度データなので、
    // この関数実行後にreadData関数を実行して、コマンドの結果を受け取ること
    uint16_t recv_data;
    uint16_t tx_data = command;
    tx_data = (tx_data << 5) | address;
    tx_data = (tx_data << 8) | data;
    // ESP32はリトルエンディアンで設計されているので
    // SPI_SWAP_DATA_TX関数で、データ順序を入れ替える
    tx_data = SPI_SWAP_DATA_TX(tx_data, DATA_LENGTH);

    spi_transaction_t trans;
    memset(&trans, 0, sizeof(trans));
    trans.length = DATA_LENGTH;
    trans.tx_buffer = &tx_data;
    trans.rx_buffer = &recv_data;

    esp_err_t ret;
    ret=spi_device_polling_transmit(spidev, &trans);
    assert(ret==ESP_OK);

    return SPI_SWAP_DATA_RX(recv_data, DATA_LENGTH);
}

float getAngle(const spi_device_handle_t spidev){
    // MA702から角度データを取得し、ラジアンに変換する
    const uint16_t RESOLUTION = 4096;

    uint16_t rawData = readData(spidev);
    // MA702の角度データは、LSBから4bitが常に0なので右に詰める
    rawData >>=4;
    return 2.0*M_PI * (float)rawData / RESOLUTION;
}

uint8_t readRegister(const spi_device_handle_t spidev, const uint8_t address){
    // MA702のレジスタデータを読み取る
    const uint16_t READ_COMMAND = 0b010;

    sendCmdAddrData(spidev, READ_COMMAND, address, 0x00);
    uint16_t rawData = readData(spidev);

    // 上位8ビットにデータが格納されるので、右に詰める
    return rawData >>=8;
}

uint8_t writeRegister(const spi_device_handle_t spidev, const uint8_t address, const uint8_t data){
    // MA702のレジスタにデータを書き込む
    const uint16_t WRITE_COMMAND = 0b100;

    sendCmdAddrData(spidev, WRITE_COMMAND, address, data);
    // 20 msec待機（MA702データシートにかかれている）
    vTaskDelay(20 / portTICK_PERIOD_MS);
    uint16_t rawData = readData(spidev);

    // 上位8ビットにデータが格納されるので、右に詰める
    return rawData >>=8;
}

void app_main(){
    static const gpio_num_t GPIO_HSPI_CS[SIDE_SIZE] 
        = {GPIO_NUM_15, GPIO_NUM_27};
    static const gpio_num_t GPIO_HSPI_SCLK = GPIO_NUM_14;
    static const gpio_num_t GPIO_HSPI_MOSI = GPIO_NUM_13;
    static const gpio_num_t GPIO_HSPI_MISO = GPIO_NUM_12;

    esp_err_t ret;
    // SPIバスの設定
    spi_bus_config_t buscfg = {
        .mosi_io_num = GPIO_HSPI_MOSI, // Master Out Slave Inのピン
        .miso_io_num = GPIO_HSPI_MISO, // Master In Slave Outのピン
        .sclk_io_num = GPIO_HSPI_SCLK, // SPI Clockのピン
        .quadwp_io_num = -1, // Quad SPIのWPピン。使わないので-1をセット。
        .quadhd_io_num = -1, // Quad SPIのHDピン。使わないので-1をセット。
        .max_transfer_sz = 2, // 最大送信バイト数。
        // flags: SPICOMMON_BUSFLAG_で始まるフラグをセットできる
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_NATIVE_PINS,
        // .intr_flags = NULL, // 割り込みの優先順位フラグ
    };
    ret = spi_bus_initialize(HSPI_HOST, &buscfg, 1);
    ESP_ERROR_CHECK(ret);

    // SPIデバイスの設定
    spi_device_interface_config_t devcfg = {
        // MA702では角度取得時にコマンドアドレスを使用しないので、
        //   command_bitsとaddress_bitsはセットしない
        // .command_bits = 3, // コマンドフェーズのビット長
        // .address_bits = 5, // アドレスフェーズのビット長
        // .dummy_bits = 8, // アドレスフェーズとデータフェーズ間のビット長
        .mode = 3, // SPIのモード
        // .duty_cycle_pos 128, // クロックのデューティ比。デフォルト50%
        // .cs_ena_posttrans = 0, // 半二重通信で送信処理後CSをアクティブにし続けるサイクル数
        .clock_speed_hz = SPI_MASTER_FREQ_20M, // クロックスピード。80MHz の分周
        // input_delay_ns: SCLKとMISOの間にある、
        //   スレーブのデータが有効になるまでの最大遅延時間。
        //   CSをアクティブにして、MISOが送信されるまでに、追加で遅延を設ける。
        //   8MHz以上のクロックスピードを使うときに必要だけど、
        //   正確な値が分からなければ0を設定してね。
        .input_delay_ns = 15, 
        // .spics_io_num // CSピン。後ほど設定する
        // .flags = NULL, // SPI_DEVICE_で始まるフラグを設定できる
        .queue_size = 1, // transactionのキュー数。1以上の値を入れておく。
        // .pre_cb // transactionが始まる前に呼ばれる関数をセットできる
        // .post_cn // transactionが完了した後に呼ばれる関数をセットできる
    };

    // SPIデバイスハンドラーを使って通信する
    spi_device_handle_t spidev[SIDE_SIZE];

    // デバイス設定のCSピンだけ書き換える
    devcfg.spics_io_num = GPIO_HSPI_CS[LEFT];
    ret = spi_bus_add_device(HSPI_HOST, &devcfg, &spidev[LEFT]);
    ESP_ERROR_CHECK(ret);

    devcfg.spics_io_num = GPIO_HSPI_CS[RIGHT];
    ret = spi_bus_add_device(HSPI_HOST, &devcfg, &spidev[RIGHT]);
    ESP_ERROR_CHECK(ret);


    // マウスが前進したら左右のエンコーダがカウントアップするように
    // 左エンコーダの回転方向を反転させる
    uint8_t address = 0b01001; // Rotation Direction
    uint8_t data = 0x80; // 回転方向を逆にする
    uint8_t reg_value;
    // MA702レジスタへの書き込みは1000回までなので、
    // 1回書き込んだらwriteRegister関数をコメントアウトすること
    // reg_value = writeRegister(spidev[LEFT], address, data);
    // printf("write: side:%d, address:%x, value:%x\n", LEFT, address, reg_value);
    // 書き込まれているか確認
    reg_value = readRegister(spidev[LEFT], address);
    printf("read: side:%d, address:%x, value:%x\n", LEFT, address, reg_value);
    vTaskDelay(2000 / portTICK_PERIOD_MS);


    float wheelAngle[SIDE_SIZE];
    while(1){
        wheelAngle[LEFT] = getAngle(spidev[LEFT]);
        wheelAngle[RIGHT] = getAngle(spidev[RIGHT]);
        
        wheelAngle[LEFT] *= 180.0/M_PI; // radian to degree
        wheelAngle[RIGHT] *= 180.0/M_PI;
        printf("wheelAngle: %f, %f\n",wheelAngle[LEFT], wheelAngle[RIGHT]);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
