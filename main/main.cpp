
#include "bsp_wifi_station.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <stdio.h>
#include <string.h>
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_log.h"
#include "string.h"


/**
 *  @brief 该代码要实现连接wifi，并且获取时间
 * 
 **/
static const int RX_BUF_SIZE = 1024;
#define MAX_HTTP_OUTPUT_BUFFER 2048
#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_16)

void init(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,                    //设置波特率    115200
        .data_bits = UART_DATA_8_BITS,          //设置数据位    8位
        .parity = UART_PARITY_DISABLE,          //设置奇偶校验  不校验
        .stop_bits = UART_STOP_BITS_1,          //设置停止位    1
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,  //设置硬件流控制 不使能
        .source_clk = UART_SCLK_APB,            //设置时钟源
    };
    //安装串口驱动 串口编号、接收buff、发送buff、事件队列、分配中断的标志
    uart_driver_install(UART_NUM_2, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    //串口参数配置 串口号、串口配置参数
    uart_param_config(UART_NUM_2, &uart_config);
    //设置串口引脚号 串口编号、tx引脚、rx引脚、rts引脚、cts引脚
    uart_set_pin(UART_NUM_2, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int sendData(const char* logName, const char* data)
{
    const int len = strlen(data);                               //获取数据长度  字符数据       
    const int txBytes = uart_write_bytes(UART_NUM_2, data, len);//发送数据
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);               //log打印
    return txBytes;
}

static void tx_task(void *arg)
{
    static const char *TX_TASK_TAG = "TX_TASK";                 //发送内容
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);               //设置log打印等级
    while (1) {
        //sendData(TX_TASK_TAG,"11111");                   //发送数据
        vTaskDelay(2000 / portTICK_PERIOD_MS);                  //延时
    }
}


static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";                 //接收任务
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);               //设置log打印
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);           //申请动态内存
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_2, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);//读取数据
        if (rxBytes > 0) {                                      //判断数据长度
            data[rxBytes] = 0;                                  //清空data中的数据
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);        //log打印
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);   //打印16进制数据
        }
    }
    free(data);                                                 //释放内存
}
void addr(char* str) {
    int len = strlen(str);
    str[len] = 'R';
    str[len+1] = '\0';
}

static void http_test_task(void *pvParameters)
{
//02-1 定义需要的变量
      char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};   //用于接收通过http协议返回的数据
    int content_length = 0;  //http协议头的长度
    //02-2 配置http结构体
   //定义http配置结构体，并且进行清零
    esp_http_client_config_t config ;
    memset(&config,0,sizeof(config));

    //向配置结构体内部写入url
    static const char *URL = "http://quan.suning.com/getSysTime.do";
    config.url = URL;
    //初始化结构体
    esp_http_client_handle_t client = esp_http_client_init(&config);	//初始化http连接
    //设置发送请求 
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    //02-3 循环通讯
    while(1)
    {
    // 与目标主机创建连接，并且声明写入内容长度为0
    esp_err_t err = esp_http_client_open(client, 0);
    //如果连接失败
    if (err != ESP_OK) {
        //ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_deep_sleep_start();
    } 
    //如果连接成功
    else {

        //读取目标主机的返回内容的协议头
        content_length = esp_http_client_fetch_headers(client);

        //如果协议头长度小于0，说明没有成功读取到
        if (content_length < 0) {
            //ESP_LOGE(TAG, "HTTP client fetch headers failed");
        } 

        //如果成功读取到了协议头
        else {
            //读取目标主机通过http的响应内容
            int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
                cJSON* root = NULL;
                root = cJSON_Parse(output_buffer);
                cJSON* time =cJSON_GetObjectItem(root,"sysTime2");
                char char_add='R';
                static const char *TX_TASK_TAG = "TX_TASK";
                char* Y = new char[strlen(time->valuestring)+1];
                char* j=time->valuestring;
                strcpy(Y,j);
                addr(Y);
                printf("%s",Y);
                sendData(TX_TASK_TAG,Y);
                esp_deep_sleep_start();
            } 
            //如果不成功
            else {
                //ESP_LOGE(TAG, "Failed to read response");
            }
        }
    }

    //关闭连接
    esp_http_client_close(client);

    //延时
    vTaskDelay(3000/portTICK_PERIOD_MS);

    }
	


}

extern "C" void app_main(void)
{

     init();                                                                             //初始化
    xTaskCreate(rx_task, "uart_rx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);     //接收任务
    xTaskCreate(tx_task, "uart_tx_task", 1024*2, NULL, configMAX_PRIORITIES-1, NULL);   //发送任务
 //01 联网
    bsp_wifi_init_sta();
   //02 创建进程，用于处理http通讯
    xTaskCreate(&http_test_task, "http_test_task", 8192, NULL, 5, NULL);
  
}


