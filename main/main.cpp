#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"

#include "esp_brookesia.hpp"
#include "app_examples/phone/squareline/src/phone_app_squareline.hpp"
#include "apps.h"

#include <stdio.h>
#include <string.h>

#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "ethernet_init.h"
#include "sdkconfig.h"
#include "setting/app_sntp.h"

static const char *TAG = "main";



/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");

    app_sntp_init();  // Initialize SNTP after getting IP address
}


void example_ethernet_connect_init(void)
{
    // Initialize Ethernet driver
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    ESP_ERROR_CHECK(example_eth_init(&eth_handles, &eth_port_cnt));

    // // Initialize TCP/IP network interface aka the esp-netif (should be called only once in application)
    // ESP_ERROR_CHECK(esp_netif_init());
    // // Create default event loop that running in background
    // ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *eth_netifs[eth_port_cnt];
    esp_eth_netif_glue_handle_t eth_netif_glues[eth_port_cnt];

    // Create instance(s) of esp-netif for Ethernet(s)
    if (eth_port_cnt == 1) 
    {
        // Use ESP_NETIF_DEFAULT_ETH when just one Ethernet interface is used and you don't need to modify
        // default esp-netif configuration parameters.
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        eth_netifs[0] = esp_netif_new(&cfg);
        eth_netif_glues[0] = esp_eth_new_netif_glue(eth_handles[0]);
        // Attach Ethernet driver to TCP/IP stack
        ESP_ERROR_CHECK(esp_netif_attach(eth_netifs[0], eth_netif_glues[0]));
    } 
    else 
    {
        // Use ESP_NETIF_INHERENT_DEFAULT_ETH when multiple Ethernet interfaces are used and so you need to modify
        // esp-netif configuration parameters for each interface (name, priority, etc.).
        esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
        esp_netif_config_t cfg_spi = {
            .base = &esp_netif_config,
            .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
        };
        char if_key_str[10];
        char if_desc_str[10];
        char num_str[3];
        for (int i = 0; i < eth_port_cnt; i++) {
            itoa(i, num_str, 10);
            strcat(strcpy(if_key_str, "ETH_"), num_str);
            strcat(strcpy(if_desc_str, "eth"), num_str);
            esp_netif_config.if_key = if_key_str;
            esp_netif_config.if_desc = if_desc_str;
            esp_netif_config.route_prio -= i*5;
            eth_netifs[i] = esp_netif_new(&cfg_spi);
            eth_netif_glues[i] = esp_eth_new_netif_glue(eth_handles[0]);
            // Attach Ethernet driver to TCP/IP stack
            ESP_ERROR_CHECK(esp_netif_attach(eth_netifs[i], eth_netif_glues[i]));
        }
    }

    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    // Start Ethernet driver state machine
    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
    }

}

void print_system_tasks() {
    // const char *TAG = "TaskInfo";

    ESP_LOGI(TAG, "Gathering task information...");
    UBaseType_t task_count = uxTaskGetNumberOfTasks();

    TaskStatus_t *task_status_array = (TaskStatus_t *)malloc(task_count * sizeof(TaskStatus_t));
    if (task_status_array == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for task status array");
        return;
    }

    uint32_t total_run_time;
    task_count = uxTaskGetSystemState(task_status_array, task_count, &total_run_time);
    if (task_count == 0) {
        ESP_LOGI(TAG, "No tasks found");
        free(task_status_array);
        return;
    }
    ESP_LOGI(TAG, "Task Count: %d", task_count);
    ESP_LOGI(TAG, "Total Run Time: %u", total_run_time);

    for (UBaseType_t i = 0; i < task_count; i++) {
        ESP_LOGI(TAG, "Task Name: %s, State: %d, Priority: %d, Stack: %u, Task Number: %d",
                 task_status_array[i].pcTaskName,
                 task_status_array[i].eCurrentState,
                 task_status_array[i].uxCurrentPriority,
                 task_status_array[i].usStackHighWaterMark,
                 task_status_array[i].xTaskNumber);
    }


    free(task_status_array);
}


extern "C" void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(bsp_spiffs_mount());
    ESP_LOGI(TAG, "SPIFFS mount successfully");

#if CONFIG_EXAMPLE_ENABLE_SD_CARD
    ESP_ERROR_CHECK(bsp_sdcard_mount());
    ESP_LOGI(TAG, "SD card mount successfully");
#endif

    ESP_ERROR_CHECK(bsp_extra_codec_init());

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = true,
        }
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    bsp_display_lock(0);

    ESP_Brookesia_Phone *phone = new ESP_Brookesia_Phone();
    assert(phone != nullptr && "Failed to create phone");

    ESP_Brookesia_PhoneStylesheet_t *phone_stylesheet = new ESP_Brookesia_PhoneStylesheet_t ESP_BROOKESIA_PHONE_800_1280_DARK_STYLESHEET();
    ESP_BROOKESIA_CHECK_NULL_EXIT(phone_stylesheet, "Create phone stylesheet failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->addStylesheet(*phone_stylesheet), "Add phone stylesheet failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->activateStylesheet(*phone_stylesheet), "Activate phone stylesheet failed");

    assert(phone->begin() && "Failed to begin phone");

    // PhoneAppSquareline *smart_gadget = new PhoneAppSquareline();
    // assert(smart_gadget != nullptr && "Failed to create phone app squareline");
    // assert((phone->installApp(smart_gadget) >= 0) && "Failed to install phone app squareline");

    // Drawpanel *drawpanel = new Drawpanel();
    // assert(drawpanel != nullptr && "Failed to create calculator");
    // assert((phone->installApp(drawpanel) >= 0) && "Failed to begin calculator");

    // Calculator *calculator = new Calculator();
    // assert(calculator != nullptr && "Failed to create calculator");
    // assert((phone->installApp(calculator) >= 0) && "Failed to begin calculator");

    // MusicPlayer *music_player = new MusicPlayer();
    // assert(music_player != nullptr && "Failed to create music_player");
    // assert((phone->installApp(music_player) >= 0) && "Failed to begin music_player");

    AppSettings *app_settings = new AppSettings();
    assert(app_settings != nullptr && "Failed to create app_settings");
    assert((phone->installApp(app_settings) >= 0) && "Failed to begin app_settings");


    example_ethernet_connect_init();
    // Game2048 *game_2048 = new Game2048();
    // assert(game_2048 != nullptr && "Failed to create game_2048");
    // assert((phone->installApp(game_2048) >= 0) && "Failed to begin game_2048");

    // Camera *camera = new Camera(1280, 720);
    // assert(camera != nullptr && "Failed to create camera");
    // assert((phone->installApp(camera) >= 0) && "Failed to begin camera");

    // AppImageDisplay *image = new AppImageDisplay();
    // assert(image != nullptr && "Failed to create image");
    // assert((phone->installApp(image) >= 0) && "Failed to begin image");

#if CONFIG_EXAMPLE_ENABLE_SD_CARD
    ESP_LOGW(TAG, "Using Video Player example requires inserting the SD card in advance and saving an MJPEG format video on the SD card");
    AppVideoPlayer *app_video_player = new AppVideoPlayer();
    assert(app_video_player != nullptr && "Failed to create app_video_player");
    assert((phone->installApp(app_video_player) >= 0) && "Failed to begin app_video_player");
#endif

    bsp_display_unlock();

    while (1)
    {
        /* code */
        // print_system_tasks();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
