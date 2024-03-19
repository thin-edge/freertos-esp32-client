#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "pthread.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT_EXAMPLE";
char APPLICATION_VERSION[] = "1.1.1";
char *cmd_topic;
char *cmd_data;
char *restart_cmd[20];
char *restart_topic;


static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    char topic_identifier[] = "te/device/";
    char DEVICE_ID[] = "esp32-c-client";
    strcat( topic_identifier, DEVICE_ID);
    strcat( topic_identifier, "//");
    time_t t;
    srand((unsigned) time(&t));
   
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:    // 
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_publish(client, topic_identifier, "{'name': 'esp32-c-client','type': 'ESP-IDF','@type': 'child-device'}", 0, 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_publish(client, "te/device/esp32-c-client///twin/c8y_Hardware", "{\"model\": \"esp32-WROOM-32\",\"revision\": \"esp32\",\"serialNumber\": \"ESP32\"}", 0, 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_publish(client, "te/device/esp32-c-client///cmd/restart", "", 0, 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            // TODO:
            // char payload[] = "{\"text\": \"Application started. version=";
            // strcat(payload, APPLICATION_VERSION);
            // strcat( payload, "\"}");
            // printf("%s\n", payload);
            // msg_id = esp_mqtt_client_publish(client, "te/device/esp32-c-client///e/boot", payload, 0, 1, 1);
            // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_subscribe(client, "te/device/esp32-c-client///cmd/+/+", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
            // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
            // ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:    
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:    // 
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            // msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
            // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:    
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:    
        //     ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        //     printf("Something published.\n");
            break;
        case MQTT_EVENT_DATA:    // 
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            printf("%d\n", event->data_len);
            printf("%d\n", event->data_len > 0);
            if ((event->data_len) > 0)
            {   
                printf("Point_1\n");
                if (cmd_topic != NULL)
                {
                    printf("Point_2\n");
                    free(cmd_topic);
                    cmd_topic = NULL;
                }
                printf("Point_3\n");
                if (cmd_data != NULL)
                {
                    printf("Point_4\n");
                    free(cmd_data);
                    cmd_data = NULL;
                }
                printf("Point_5\n");
                cmd_topic = malloc(event->topic_len);
                cmd_data = malloc(event->data_len);
                memcpy(cmd_topic, event->topic, event->topic_len);
                cmd_topic[event->topic_len] ='\0';
                memcpy(cmd_data, event->data, event->data_len);
                cmd_data[event->data_len] ='\0';
                printf("%s\n", cmd_topic);
                printf("%s\n", cmd_data);
                
                if (strstr(cmd_topic, "cmd/restart")!=NULL && strstr(cmd_data, "init")!=NULL)
                {
                    restart_cmd[0] = "init";
                    restart_topic = NULL;
                    restart_topic = malloc(event->topic_len);
                    memcpy(restart_topic, event->topic, event->topic_len);
                    restart_topic[event->topic_len] ='\0';
                    // restart_topic = cmd_topic;
                    printf("Got restart request init\n");
                    printf("%s\n", restart_cmd[0]);
                    printf("%s\n", restart_topic);
                }
                else if (strstr(cmd_topic, "cmd/restart")!=NULL && strstr(cmd_data, "executing")!=NULL)
                {
                    restart_cmd[1] = "executing";
                    printf("Got restart request executing\n");
                    printf("%s\n", restart_cmd[1]);
                }
                else if (strstr(cmd_topic, "cmd/restart")!=NULL && strstr(cmd_data, "successful")!=NULL)
                {
                    // restart_cmd[2] = "successful";
                    printf("Got restart request successful\n");
                    // printf("%s\n", restart_cmd[2]);
                }
            }
            else
            {
                printf("Payload is empty.\n");
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);

    mqtt_event_handler_cb(event_data);
}


static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://raspberrypi.local",    
        .broker.address.port = 1883,               
        .session.last_will.topic = "te/device/esp32-c-client///e/disconnected",
        .session.last_will.msg = "{\"text\": \"Disconnected\"}",
        .session.last_will.qos = 1,
        .session.last_will.retain = false
    };
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);

    while (1)
    {   
        int msg_id;
        int temperature = rand() % 100;
        char temp_payload[] = "{\"temp\": ";
        char temp[16];
        itoa(temperature, temp,10);
        strcat(temp_payload, temp);
        strcat(temp_payload, "}");

        msg_id = esp_mqtt_client_publish(client, "te/device/esp32-c-client///m/environment", temp_payload, 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        sleep(5);

        if ((restart_cmd[0] != '\0') && (restart_cmd[1] == '\0'))
        {
            msg_id = esp_mqtt_client_publish(client, "te/device/esp32-c-client///a/restart", "{\"text\": \"Device will be restarted\",\"severity\": \"critical\"}", 0, 1, 1);
            ESP_LOGI(TAG, "sent alarm successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_publish(client, cmd_topic, "{\"status\": \"executing\"}", 0, 1, 1);
            ESP_LOGI(TAG, "sent executing successful, msg_id=%d", msg_id);
            sleep(5);
            esp_restart();
        }
        else if (restart_cmd[1] != '\0')
        {   
            printf("Will create event!!!\n");
            esp_mqtt_client_publish(client, "te/device/esp32-c-client///e/restart","{\"text\": \"Device restarted\"}",0,0,0);
            // ESP_LOGI(TAG, "sent event successful, msg_id=%d", msg_id);
            printf("sent event successful\n");
            esp_mqtt_client_publish(client, "te/device/esp32-c-client///a/restart", "", 0, 2, 1);
            printf("Clear alarm successful\n");
            sleep(3);
            printf("%s\n", restart_cmd[1]);
            esp_mqtt_client_publish(client,cmd_topic,"{\"status\": \"successful\"}", 0, 1, 1);
            // ESP_LOGI(TAG, "sent successful successful, msg_id=%d", msg_id);
            printf("sent successful successful.\n");
            
            if (cmd_topic != NULL)
                {
                    free(cmd_topic);
                    cmd_topic = NULL;
                }
            restart_cmd[0] = '\0';
            restart_cmd[1] = '\0';
            // restart_cmd[2] = '\0';
            printf("cmd 0 %d\n", restart_cmd[0]);
            printf("cmd 1 %d\n", restart_cmd[1]);
            if (cmd_data != NULL)
                {
                    // printf("Point_4\n");
                    free(cmd_data);
                    cmd_data = NULL;
                }
            // printf("cmd 2 %d\n", restart_cmd[2]);
            printf("Data cleared.");
            sleep(5);
        }
 
    }
    
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    restart_cmd[0] = '\0';
    restart_cmd[1] = '\0';
    // restart_cmd[2] = '\0';
    mqtt_app_start();
}
