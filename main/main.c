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

static const char *id = "202321234567";

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
static const char *TAG_TCP = "tcp_client";

// define para o sensor BME280
#define SDA_PIN GPIO_NUM_5
#define SCL_PIN GPIO_NUM_4
#define I2C_MASTER_ACK 0
#define I2C_MASTER_NACK 1

// static const char *TAG_BME280 = "BME280-TST02";

// Variáveis compartilhadas entre as tarefas
// SemaphoreHandle_t xSemaphore;
// static float pressure_s32 = 9535.84;
// static float temperature_s32 = 34.69;
// static float humidity_s32 = 56.947;
static uint8_t type_msg = 0;

static s32 pressure_s32;
static s32 temperature_s32;
static s32 humidity_s32;

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
            // if (xSemaphoreTake(xSemaphore, portMAX_DELAY) && (type_msg == 0) && (xSemaphoreTake(xMutex, portMAX_DELAY)))
            if (xSemaphoreTake(xSemaphore, portMAX_DELAY) && (type_msg == 0))
            {
                if ((xSemaphoreTake(xMutex, portMAX_DELAY)))
                {
                    sprintf(tx_buffer, "{\"temperature\": %.2f, \"pressure\": %.2f, \"humidity\": %.2f}", (float)temperature_s32, (float)pressure_s32, (float)humidity_s32);
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
            }
            // else if (xSemaphoreTake(xSemaphore, portMAX_DELAY) && (type_msg == 1) && (xSemaphoreTake(xMutex, portMAX_DELAY)))
            if (xSemaphoreTake(xSemaphore, portMAX_DELAY) && (type_msg == 1))
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

// tarefa para leitura do sensor BME280
void bme280_task(void *pvParameter)
{
    while (1)
    {
        xSemaphoreGive(xSemaphore);
        type_msg = 0;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void alive_task(void *pvParameter)
{
    while (1)
    {
        printf("alive\n");

        xSemaphoreGive(xSemaphore);
        type_msg = 1;
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

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

    xSemaphore = xSemaphoreCreateBinary();

    // Cria o mutex
    xMutex = xSemaphoreCreateMutex();

    xTaskCreate(&alive_task, "alive massege", 1024, NULL, 5, NULL);
    xTaskCreate(&bme280_task, "read sensor bme280", 2048, NULL, 6, NULL);
    xTaskCreate(&tcp_client, "tcp_client", 4096, NULL, 5, NULL);
}