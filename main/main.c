#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"

#include "sdkconfig.h"

#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "nvs_flash.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "bme280.h"

// defines para conexao na rede wifi
#define SSID "brisa-594111"
#define PASS "gbalklxz"
#define TCPServerIP "159.203.79.141"
#define PORT 50000

// id do dispositivo
static const char *id = "202321234567";

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
static const char *TAG_TCP = "tcp_client";

// defines para o sensor BME280
#define SDA_PIN GPIO_NUM_5
#define SCL_PIN GPIO_NUM_4
#define I2C_MASTER_ACK 0
#define I2C_MASTER_NACK 1

// Variáveis globais
static uint8_t flag_alive = 0;
static uint8_t flag_sensor = 0;

static double temperature_double;
static double pressure_double;
static double humidity_double;

static const char *TAG_BME280 = "bme280";

// Cria o semáforo
SemaphoreHandle_t xSemaphore = NULL;
SemaphoreHandle_t xMutex = NULL;

// conectar na rede wifi
void wifi_connect()
{
    wifi_config_t cfg = {
        .sta = {
            .ssid = SSID,
            .password = PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_connect());
}

// tratamento de eventos wifi
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
    case SYSTEM_EVENT_STA_START:
        wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

// inicializa o wifi
static void initialise_wifi(void)
{
    esp_log_level_set("wifi", ESP_LOG_NONE); // disable wifi driver logging
    tcpip_adapter_init();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// tarefa tcp client
void tcp_client(void *pvParam)
{
    printf("tcp_client task started \n");
    char rx_buffer[128];
    char tx_buffer[128];
    char host_ip[] = TCPServerIP;
    struct sockaddr_in tcpServerAddr;
    tcpServerAddr.sin_addr.s_addr = inet_addr(TCPServerIP);
    tcpServerAddr.sin_family = AF_INET;
    tcpServerAddr.sin_port = htons(PORT);
    int sock = -1, err, len;
    int error_count = 0;
    int loging_status = 0;

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    while (1)
    {
        // Verifica se o socket está conectado e se o login foi realizado
        if (loging_status == 0)
        {
            if (sock < 0)
            {
                sock = socket(AF_INET, SOCK_STREAM, 0);
                if (sock < 0)
                {
                    ESP_LOGE(TAG_TCP, "Unable to create socket: errno %d", errno);
                    continue;
                }
                ESP_LOGI(TAG_TCP, "Socket created, connecting to %s:%d", host_ip, PORT);

                err = connect(sock, (struct sockaddr *)&tcpServerAddr, sizeof(tcpServerAddr));
                if (err != 0)
                {
                    ESP_LOGE(TAG_TCP, "Socket unable to connect: errno %d", errno);
                    close(sock);
                    sock = -1;
                    continue;
                }
                ESP_LOGI(TAG_TCP, "Successfully connected");
            }

            err = send(sock, id, strlen(id), 0);
            if (err < 0)
            {
                ESP_LOGE(TAG_TCP, "Error occurred during sending: errno %d", errno);
            }
            else
            {
                int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
                // Error occurred during receiving
                if (len < 0)
                {
                    ESP_LOGE(TAG_TCP, "recv failed: errno %d", errno);
                    loging_status = 0;
                }
                // Data received
                else
                {
                    rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                    if (strcmp(rx_buffer, "ok") == 0)
                    {
                        printf("Login realizado com sucesso\n");
                        loging_status = 1;
                    }
                }
            }
        }
        // login realizado, envia dados
        else if (loging_status == 1)
        {
            if (xSemaphoreTake(xSemaphore, portMAX_DELAY) && (flag_sensor == 1))
            {
                if ((xSemaphoreTake(xMutex, portMAX_DELAY)))
                {
                    sprintf(tx_buffer, "{\"temperature\": %.2f, \"pressure\": %.2f, \"humidity\": %.2f}", temperature_double, pressure_double, humidity_double);
                    ESP_LOGI(TAG_TCP, "Sending: %s", tx_buffer);
                    err = send(sock, tx_buffer, strlen(tx_buffer), 0);
                    if (err < 0)
                    {
                        ESP_LOGE(TAG_TCP, "Error occurred during sending msg0: errno %d", errno);
                        error_count++;
                        if (error_count >= 10)
                        {
                            ESP_LOGI(TAG_TCP, "Too many errors msg0, reconnecting...");
                            shutdown(sock, 0);
                            close(sock);
                            sock = -1;
                            error_count = 0;
                            loging_status = 0;
                        }
                    }
                    else
                    {
                        error_count = 0; // Reset error count after successful send
                    }
                    // Libera o mutex
                    xSemaphoreGive(xMutex);
                }
                flag_sensor = 0;
            }

            if (xSemaphoreTake(xSemaphore, portMAX_DELAY) && (flag_alive == 1))
            {
                if ((xSemaphoreTake(xMutex, portMAX_DELAY)))
                {

                    sprintf(tx_buffer, "alive");
                    ESP_LOGI(TAG_TCP, "Sending: %s", tx_buffer);
                    err = send(sock, tx_buffer, strlen(tx_buffer), 0);
                    if (err < 0)
                    {
                        ESP_LOGE(TAG_TCP, "Error occurred during sending msg1: errno %d", errno);
                        error_count++;
                        if (error_count >= 10)
                        {
                            ESP_LOGI(TAG_TCP, "Too many errors msg1, reconnecting...");
                            shutdown(sock, 0);
                            close(sock);
                            sock = -1;
                            error_count = 0;
                            loging_status = 0;
                        }
                    }
                    else
                    {
                        error_count = 0; // Reset error count after successful send

                        int len_recv = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
                        // Error occurred during receiving
                        if (len_recv < 0)
                        {
                            ESP_LOGE(TAG_TCP, "recv failed: errno %d", errno);
                        }
                        // Data received
                        else
                        {
                            rx_buffer[len_recv] = 0; // Null-terminate whatever we received and treat like a string
                            if (strcmp(rx_buffer, "ok") == 0)
                            {
                                printf("Alive recebido com sucesso\n");
                            }
                        }
                    }
                    // Libera o mutex
                    xSemaphoreGive(xMutex);
                }
                flag_alive = 0;
            }
            continue;
        }

        if (loging_status == 0)
        {
            err = send(sock, tx_buffer, strlen(tx_buffer), 0);
            if (err < 0)
            {
                ESP_LOGE(TAG_TCP, "Error occurred during sending: errno %d", errno);
                error_count++;
                if (error_count >= 10)
                {
                    ESP_LOGI(TAG_TCP, "Too many errors, reconnecting...");
                    shutdown(sock, 0);
                    close(sock);
                    sock = -1;
                    error_count = 0;
                }
            }
            else
            {
                error_count = 0; // Reset error count after successful send
            }
        }

        // Aguarda um pouco antes de tentar novamente
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    if (sock != -1)
    {
        ESP_LOGE(TAG_TCP, "Shutting down socket...");
        shutdown(sock, 0);
        close(sock);
    }
    vTaskDelete(NULL);
}

// funções para o sensor BME280

// inicializa o i2c
void i2c_master_init()
{
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 1000000};
    i2c_param_config(I2C_NUM_0, &i2c_config);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

// Escreve no barramento I2C
s8 BME280_I2C_bus_write(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
    s32 iError = BME280_INIT_VALUE;

    esp_err_t espRc;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);

    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write(cmd, reg_data, cnt, true);
    i2c_master_stop(cmd);

    espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    if (espRc == ESP_OK)
    {
        iError = SUCCESS;
    }
    else
    {
        iError = FAIL;
    }
    i2c_cmd_link_delete(cmd);

    return (s8)iError;
}

// Le do barramento I2C
s8 BME280_I2C_bus_read(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
    s32 iError = BME280_INIT_VALUE;
    esp_err_t espRc;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);

    if (cnt > 1)
    {
        i2c_master_read(cmd, reg_data, cnt - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, reg_data + cnt - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    if (espRc == ESP_OK)
    {
        iError = SUCCESS;
    }
    else
    {
        iError = FAIL;
    }

    i2c_cmd_link_delete(cmd);

    return (s8)iError;
}

// Delay
void BME280_delay_msek(u32 msek)
{
    vTaskDelay(msek / portTICK_PERIOD_MS);
}

// Thread para leitura do sensor BME280
void bme280_task(void *pvParameter)
{
    struct bme280_t bme280 = {
        .bus_write = BME280_I2C_bus_write,
        .bus_read = BME280_I2C_bus_read,
        .dev_addr = BME280_I2C_ADDRESS1,
        .delay_msec = BME280_delay_msek};

    s32 com_rslt;

    com_rslt = bme280_init(&bme280);

    com_rslt += bme280_set_oversamp_pressure(BME280_OVERSAMP_16X);
    com_rslt += bme280_set_oversamp_temperature(BME280_OVERSAMP_2X);
    com_rslt += bme280_set_oversamp_humidity(BME280_OVERSAMP_1X);

    com_rslt += bme280_set_standby_durn(BME280_STANDBY_TIME_1_MS);
    com_rslt += bme280_set_filter(BME280_FILTER_COEFF_16);

    com_rslt += bme280_set_power_mode(BME280_NORMAL_MODE);

    static s32 pressure_s32;
    static s32 temperature_s32;
    static s32 humidity_s32;

    while (true)
    {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        com_rslt = bme280_read_uncomp_pressure_temperature_humidity(
            &pressure_s32, &temperature_s32, &humidity_s32);

        if (com_rslt == SUCCESS)
        {
            temperature_double = bme280_compensate_temperature_double(temperature_s32);
            pressure_double = bme280_compensate_pressure_double(pressure_s32)/100;
            humidity_double = bme280_compensate_humidity_double(humidity_s32);
            xSemaphoreGive(xSemaphore);
        }
        else
        {
            ESP_LOGE(TAG_BME280, "measure error. code: %d", com_rslt);
        }
        xSemaphoreGive(xSemaphore);
        flag_sensor = 1;
    }
}


// Thread para enviar alive
void alive_task(void *pvParameter)
{
    while (true)
    {
        printf("alive\n");
        xSemaphoreGive(xSemaphore);
        flag_alive = 1;
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

// main
void app_main()
{
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_event_group = xEventGroupCreate();
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    initialise_wifi();
    i2c_master_init();

    // Cria o mutex
    xMutex = xSemaphoreCreateMutex();
    xSemaphore = xSemaphoreCreateBinary();

    xTaskCreate(&alive_task, "alive massege", 1024, NULL, 5, NULL);
    xTaskCreate(&bme280_task, "read sensor bme280", 4096, NULL, 6, NULL);
    xTaskCreate(&tcp_client, "tcp_client", 4096, NULL, 5, NULL);
}