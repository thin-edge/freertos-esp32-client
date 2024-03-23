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
#include "esp_mac.h"
#include "protocol_examples_common.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#define STORAGE_NAMESPACE "storage"
#define CONFIG_MQTT_HOST "mqtt_host"
#define CONFIG_MQTT_PORT "mqtt_port"

static const char *TAG = "TEDGE";
char APPLICATION_VERSION[] = "1.1.1";

//
// Discover settings
//
// Manually set the host (only recommended if you are not running thin-edge.io with mdns enabled)
const char *MANUAL_MQTT_HOST = NULL;

// Match the first service matching the given pattern
const char *MDNS_DISCOVER_PATTERN = NULL;

//
// Persistence
//
// Enable/disable reading and saving settings to persistent storage (NVM flash)
bool READ_FROM_NVM = true;
bool SAVE_TO_NVM = true;

// Internal
char DEVICE_ID[32] = {0};
char TOPIC_ID[256] = {0};
char *cmd_topic;
char *cmd_data;

struct Server{
    uint16_t port;
    char *host;
};

typedef enum {
    OPSTATE_NONE = 0,
    OPSTATE_INIT = 1,
    OPSTATE_EXECUTING = 2,
    OPSTATE_SUCCESSFUL = 3,
    OPSTATE_FAILED = 4,
} operation_state_t;

// Current operation state
operation_state_t OPERATION_STATE = OPSTATE_NONE;

/*
    Convert operation state to a string to a human readable format
*/
const char *tedge_operation_state_to_name(operation_state_t code) {
    switch (code) {
    case OPSTATE_NONE:
        return "NONE";
    case OPSTATE_INIT:
        return "INIT";
    case OPSTATE_EXECUTING:
        return "EXECUTING";
    case OPSTATE_SUCCESSFUL:
        return "SUCCESSFUL";
    case OPSTATE_FAILED:
        return "FAILED";
    }
    return "NONE";
}

/* 
    Set the device identify and topic identifier.
    The MAC address from the Wifi is used to unique identify the device
*/
void set_device_id(char *device_id, char *topic_id) {
    unsigned char mac[6] = {0};
    esp_efuse_mac_get_default(mac);
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(device_id, "esp32-%02x%02x%02x%02x%02x%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    sprintf(topic_id, "te/device/%s//", device_id);
}

/*
    Publish an MQTT message to the device's topic identifier
*/
int publish_mqtt_message(esp_mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain) {
    char full_topic[256] = {0};
    strcat(full_topic, TOPIC_ID);
    strcat(full_topic, topic);
    ESP_LOGI(TAG, "Publishing message. topic=%s, data=%s", full_topic, data);
    int msg_id = esp_mqtt_client_publish(client, full_topic, data, len, qos, retain);
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
    return msg_id;
}

/*
    Build mqtt topic using the device's topic identifier and the give partial topic
*/
void build_mqtt_topic(char *dst, char *topic) {
    strcat(dst, TOPIC_ID);
    strcat(dst, topic);
}

/*
    thin-edge.io service discovery which uses mdns to find the thin-edge.io MQTT broker in the local network
*/
static const char *ip_protocol_str[] = {"V4", "V6", "MAX"};

static mdns_result_t* mdns_find_match(mdns_result_t *results, const char *pattern) {
    mdns_result_t *r = results;
    mdns_ip_addr_t *a = NULL;
    int i = 1, t;
    while (r) {
        if (r->esp_netif) {
            printf("%d: Interface: %s, Type: %s, TTL: %" PRIu32 "\n", i++, esp_netif_get_ifkey(r->esp_netif),
                   ip_protocol_str[r->ip_protocol], r->ttl);
        }
        if (r->instance_name) {
            printf("  PTR : %s.%s.%s\n", r->instance_name, r->service_type, r->proto);
        }
        if (r->hostname) {
            printf("  SRV : %s.local:%u\n", r->hostname, r->port);
        }
        if (r->txt_count) {
            printf("  TXT : [%zu] ", r->txt_count);
            for (t = 0; t < r->txt_count; t++) {
                printf("%s=%s(%d); ", r->txt[t].key, r->txt[t].value ? r->txt[t].value : "NULL", r->txt_value_len[t]);
            }
            printf("\n");
        }
        a = r->addr;
        while (a) {
            if (a->addr.type == ESP_IPADDR_TYPE_V6) {
                printf("  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
            } else {
                printf("  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
            }
            a = a->next;
        }

        if (r != NULL && pattern != NULL && strstr(r->hostname, pattern) != NULL) {
            ESP_LOGI(TAG, "Selecting first match. host=%s, port=%d", r->hostname, r->port);
            return r;
        }

        r = r->next;
    }
    return NULL;
}

void discover_tedge_broker(struct Server *server, const char *service_name, const char *proto, const char *pattern) {
    ESP_LOGI(TAG, "Query PTR: %s.%s.local", service_name, proto);

    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query_ptr(service_name, proto, 3000, 20,  &results);
    if (err) {
        ESP_LOGE(TAG, "Query Failed: %s", esp_err_to_name(err));
        return;
    }
    if (!results) {
        ESP_LOGW(TAG, "No results found!");
        return;
    }

    mdns_result_t *match = mdns_find_match(results, pattern);
    if (match != NULL) {
        server->port = match->port;
        char host_url[256] = {0};
        sprintf(host_url, "mqtt://%s.local", match->hostname);
        server->host = strdup(host_url);
        ESP_LOGI(TAG, "Found matching service. host=%s, port=%d", server->host, server->port);
    }
    mdns_query_results_free(results);
}

/*
    Read settings from NVM flash
 */
esp_err_t read_settings(struct Server *mqtt_server)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) return err;

    // Read
    char *host = calloc(256, sizeof(char));
    size_t size = 256;
    err = nvs_get_str(my_handle, CONFIG_MQTT_HOST, host, &size);
    if (err == ESP_OK) {
        mqtt_server->host = host;
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        free(host);
    } else if (err != ESP_OK) {
        free(host);
        return err;
    }

    u_int16_t port = 0;
    err = nvs_get_u16(my_handle, CONFIG_MQTT_PORT, &port);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        mqtt_server->port = port;
    } else if (err != ESP_OK) {
        return err;
    }
    
    // Close
    nvs_close(my_handle);
    return ESP_OK;
}


/*
    Persist settings to NVM flash
*/
esp_err_t save_settings(struct Server *mqtt_server)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(my_handle, CONFIG_MQTT_HOST, mqtt_server->host);
    if (err != ESP_OK) return err;

    err = nvs_set_u16(my_handle, CONFIG_MQTT_PORT, mqtt_server->port);
    if (err != ESP_OK) return err;

    // Commit written value.
    // After setting any values, nvs_commit() must be called to ensure changes are written
    // to flash storage. Implementations may write to storage at other times,
    // but this is not guaranteed.
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{

    esp_mqtt_client_handle_t client = event->client;
    time_t t;
    srand((unsigned) time(&t));

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

            // Register device
            char reg_message[256] = {0};
            sprintf(reg_message, "{\"name\":\"%s\",\"type\":\"ESP-IDF\",\"@type\":\"child-device\"}", DEVICE_ID);
            publish_mqtt_message(client, "", reg_message, 0, 1, 1);

            // Publish device info
            char hardware_info[256] = {0};
            sprintf(hardware_info, "{\"model\":\"esp32-WROOM-32\",\"revision\":\"esp32\",\"serialNumber\":\"%s\"}", DEVICE_ID);
            publish_mqtt_message(client, "/twin/c8y_Hardware", hardware_info, 0, 1, 1);

            // Register capabilities
            publish_mqtt_message(client, "/cmd/restart", "{}", 0, 1, 1);

            // Publish boot event
            char boot_message[256] = {0};
            sprintf(boot_message, "{\"text\":\"Application started. version=%s\",\"version\":\"%s\"}", APPLICATION_VERSION, APPLICATION_VERSION);
            publish_mqtt_message(client, "/e/boot", boot_message, 0, 1, 0);

            // Subscribe to commands
            char command_topic[256] = {0};
            build_mqtt_topic(command_topic, "/cmd/+/+");
            int msg_id = esp_mqtt_client_subscribe(client, command_topic, 0);
            ESP_LOGI(TAG, "sent subscribe successful, topic=%s msg_id=%d", command_topic, msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "DATA=%.*s (len %d)", event->data_len, event->data, event->data_len);
            if ((event->data_len) > 0)
            {
                if (cmd_topic != NULL)
                {
                    free(cmd_topic);
                    cmd_topic = NULL;
                }
                if (cmd_data != NULL)
                {
                    free(cmd_data);
                    cmd_data = NULL;
                }
                if (strnstr(event->topic, TOPIC_ID, event->topic_len) != NULL) {
                    int topic_len = strlen(TOPIC_ID);
                    cmd_topic = strndup(event->topic+topic_len, event->topic_len-topic_len);
                } else {
                    cmd_topic = strndup(event->topic, event->topic_len);
                }
                cmd_data = strndup(event->data, event->data_len);
                ESP_LOGI(TAG, "COMMAND_TOPIC=%s", cmd_topic);
                ESP_LOGI(TAG, "COMMAND_DATA=%s", cmd_data);

                if (strstr(cmd_topic, "cmd/restart")!=NULL && strstr(cmd_data, "init")!=NULL)
                {
                    OPERATION_STATE = OPSTATE_INIT;
                    ESP_LOGI(TAG, "Got restart request init");
                }
                else if (strstr(cmd_topic, "cmd/restart")!=NULL && strstr(cmd_data, "executing")!=NULL)
                {
                    OPERATION_STATE = OPSTATE_EXECUTING;
                    ESP_LOGI(TAG, "Got restart request executing");
                }
                else if (strstr(cmd_topic, "cmd/restart")!=NULL && strstr(cmd_data, "successful")!=NULL)
                {
                    ESP_LOGI(TAG, "Got restart request successful");
                }
            }
            else
            {
                ESP_LOGI(TAG, "Payload is empty");
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
    // Use mdns to discover which local thin-edge.io to connect to
    ESP_ERROR_CHECK(mdns_init());

    set_device_id(DEVICE_ID, TOPIC_ID);
    ESP_LOGI(TAG, "Device id: %s", DEVICE_ID);
    ESP_LOGI(TAG, "Topic id: %s", TOPIC_ID);

    struct Server server = {
        .port = 1883,
    };

    if (MANUAL_MQTT_HOST != NULL) {
        ESP_LOGI(TAG, "Using manual mqtt host. server=%s", MANUAL_MQTT_HOST);
        server.host = strdup(MANUAL_MQTT_HOST);
    }

    if (READ_FROM_NVM) {
        ESP_LOGI(TAG, "Reading settings from NVM flash");
        err_enum_t err = read_settings(&server);
        if (err == ERR_OK) {
            if (server.host != NULL) {
                ESP_LOGI(TAG, "Read settings from NVM flash. server=%s, port=%d", server.host, server.port);
            } else {
                ESP_LOGW(TAG, "Server host is empty");
            }
        } else {
            ESP_LOGW(TAG, "Failed to read settings from NVM flash. err=%s", esp_err_to_name(err));
        }
    }

    // Retry forever waiting for a valid thin-edge.io instance is found
    while (server.host == NULL) {
        discover_tedge_broker(&server, "_thin-edge_mqtt", "_tcp", MDNS_DISCOVER_PATTERN);
        if (server.host != NULL) {
            ESP_LOGI(TAG, "Using thin-edge.io. host=%s, port=%d", server.host, server.port);

            if (SAVE_TO_NVM) {
                ESP_LOGI(TAG, "Saving settings to NVM flash");
                esp_err_t err = save_settings(&server);
                if (err != ERR_OK) {
                    ESP_LOGW(TAG, "Failed to save settings to NVM flash. err=%s", esp_err_to_name(err));
                }
            }
        } else {
            ESP_LOGE(TAG, "Could not find a thin-edge.io MQTT Broker. Retrying in 10 seconds");
            sleep(10);
        }
    }

    char last_will_topic[256] = {0};
    build_mqtt_topic(last_will_topic, "/e/disconnected");

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = server.host,
        .broker.address.port = server.port,
        .session.last_will.topic = last_will_topic,
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
        int temperature = rand() % 100;
        char temp_payload[] = "{\"temp\": ";
        char temp[16];
        itoa(temperature, temp,10);
        strcat(temp_payload, temp);
        strcat(temp_payload, "}");

        publish_mqtt_message(client, "/m/environment", temp_payload, 0, 0, 0);
        sleep(5);

        if (OPERATION_STATE != OPSTATE_NONE)
        {
            ESP_LOGI(TAG, "operation state: %s", tedge_operation_state_to_name(OPERATION_STATE));
        }
        if (OPERATION_STATE == OPSTATE_INIT)
        {
            publish_mqtt_message(client, "/a/restart", "{\"text\": \"Device will be restarted\",\"severity\": \"critical\"}", 0, 1, 1);
            publish_mqtt_message(client, cmd_topic, "{\"status\": \"executing\"}", 0, 1, 1);
            sleep(5);
            esp_restart();
        }
        else if (OPERATION_STATE == OPSTATE_EXECUTING)
        {
            ESP_LOGI(TAG, "Will create event!!!");
            publish_mqtt_message(client, "/e/restart","{\"text\": \"Device restarted\"}", 0, 0, 0);
            publish_mqtt_message(client, "/a/restart", "", 0, 2, 1);
            sleep(3);
            publish_mqtt_message(client, cmd_topic, "{\"status\": \"successful\"}", 0, 1, 1);
            ESP_LOGI(TAG, "sent successful successful");

            if (cmd_topic != NULL)
                {
                    free(cmd_topic);
                    cmd_topic = NULL;
                }
            OPERATION_STATE = OPSTATE_NONE;
            if (cmd_data != NULL)
                {
                    free(cmd_data);
                    cmd_data = NULL;
                }
            ESP_LOGI(TAG, "Data cleared.");
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
    mqtt_app_start();
}
