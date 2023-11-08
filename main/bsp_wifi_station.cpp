
#include "bsp_wifi_station.h"

/**
@brief 处理wifi连接和ip分配时候事件的回调函数
**/

static void event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {   //如果是wifi连接事件，就进行wifi连接
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {  //如果是wifi连接失败事件
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {   //如果没有达到最高尝试次数，继续尝试
            esp_wifi_connect();
            s_retry_num++;
           // ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);   //如果达到了最高尝试次数，就停止尝试，并且标记连接失败
        }
        //ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {     //如果是ip获取事件，获取到了ip就打印出来
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        //ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);    //如果成功获取到了ip，就标记这次wifi连接成功
    }
}



/**
@brief 用于连接wifi的函数
@param[in] 无
@retval 无
@note 这里wifi连接选项设置了使用nvs，会把每次配置的参数存储在nvs中。因此请查看分区表中是否对nvs分区进行了设置

**/
void bsp_wifi_init_sta()
{

   //00 使能nvs
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)  //如果nvs空间满了，就进行擦除
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

  //00 创建wifi事件的标志组
    s_wifi_event_group = xEventGroupCreate();

  //01 WIFI/LWIP初始化阶段

         //01-1 创建LWIP核心任务
        ESP_ERROR_CHECK(esp_netif_init());


         //01-2 创建系统事件任务
        ESP_ERROR_CHECK(esp_event_loop_create_default());

          //01-3 创建station实例
        esp_netif_create_default_wifi_sta();

          //01-4 创建wifi驱动程序任务，并初始化wifi驱动程序
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        //01-5 注册，用于处理wifi连接的过程中的事件
        esp_event_handler_instance_t instance_any_id;   //用于处理wifi连接时候的事件的句柄
        esp_event_handler_instance_t instance_got_ip;    //用于处理ip分配时候产生的事件的句柄
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,   //该句柄对wifi连接所有事件都产生响应，连接到event_handler回调函数
            ESP_EVENT_ANY_ID,
            &event_handler,
            NULL,
            &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,   //该句柄仅仅处理IP_EVENT事件组中的获取ip地址事件，连接到event_handler回调函数
            IP_EVENT_STA_GOT_IP, 
            &event_handler,
            NULL,
            &instance_got_ip));
//02 WIFI配置阶段

        //02-1 定义wifi配置参数
        wifi_config_t wifi_config;                                           //定义wifi配置参数结构体
        memset(&wifi_config, 0, sizeof(wifi_config));                       //对结构体进行初始化，把参数全部定义为0
        sprintf((char*)wifi_config.sta.ssid, EXAMPLE_ESP_WIFI_SSID);        //配置wifi名称              
        sprintf((char*)wifi_config.sta.password, EXAMPLE_ESP_WIFI_PASS);    //配置wifi密码
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;            //配置加密协议
        wifi_config.sta.pmf_cfg.capable = true;                             
        wifi_config.sta.pmf_cfg.required = false;

        //02-2 配置wifi工作模式
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

        //02-3 写入配置
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

     
 //03 wifi启动阶段

        //03-1 启动wifi驱动程序
        ESP_ERROR_CHECK(esp_wifi_start());   //会触发回调函数
       // ESP_LOGI(TAG, "wifi_init_sta finished.");

  //04 输出wifi连接结果      
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);


        if (bits & WIFI_CONNECTED_BIT) {
            //ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
        }
        else if (bits & WIFI_FAIL_BIT) {
            //ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
        }
        else {
           // ESP_LOGE(TAG, "UNEXPECTED EVENT");
        }

  
//05  事件解绑定

        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
        vEventGroupDelete(s_wifi_event_group);
}
    