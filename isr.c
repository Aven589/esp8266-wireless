#include "stdint.h"

// ESP8266 下发给单片机的参数帧：目标点 + PID。
#pragma pack(1)
typedef struct
{
    uint8_t head;
    float target_x;
    float target_y;
    float pid_kp;
    float pid_ki;
    float pid_kd;
    uint8_t tail;
} Esp_Uart4_Data_t;
#pragma pack()

typedef union
{
    Esp_Uart4_Data_t data;
    uint8_t bytes[sizeof(Esp_Uart4_Data_t)];
} Esp_Uart4_Union_t;

// 单片机回传给 ESP8266 的状态帧：当前小车坐标。
#pragma pack(1)
typedef struct
{
    uint8_t head;
    float car_x;
    float car_y;
    uint8_t tail;
} Car_To_Esp_Data_t;
#pragma pack()

typedef union
{
    Car_To_Esp_Data_t data;
    uint8_t bytes[sizeof(Car_To_Esp_Data_t)];
} Car_To_Esp_Union_t;

// 接收完成标志和最近一次从 ESP8266 收到的参数。
volatile int esp_uart4_rx_flag = 0;
float esp_target_x = 0.0f;
float esp_target_y = 0.0f;
float esp_pid_kp = 0.0f;
float esp_pid_ki = 0.0f;
float esp_pid_kd = 0.0f;

// 中断接收计数和收包缓存。
uint16_t esp_uart4_cnt = 0;
Esp_Uart4_Union_t esp_uart4_packet;

// 将当前小车坐标按协议封包后发给 ESP8266。
void ESP_Uart4_SendCarPosition(float car_x, float car_y)
{
    Car_To_Esp_Union_t tx_packet;

    tx_packet.data.head = 0xB5;
    tx_packet.data.car_x = car_x;
    tx_packet.data.car_y = car_y;
    tx_packet.data.tail = 0x5B;

    LPUART_WriteBlocking(LPUART4, tx_packet.bytes, sizeof(Car_To_Esp_Data_t));
}

void LPUART4_IRQHandler(void)//以下中断函数换成你自己的中断函数
{
    if(kLPUART_RxDataRegFullFlag & LPUART_GetStatusFlags(LPUART4))
    {
        uint8_t byte = LPUART_ReadByte(LPUART4);

        // 接收状态机：帧头 0xB5 + 5 个 float + 帧尾 0x5B。
        if(esp_uart4_cnt == 0)
        {
            if(byte == 0xB5)
            {
                // 收到合法帧头后开始缓存整帧。
                esp_uart4_packet.bytes[esp_uart4_cnt++] = byte;
                esp_uart4_rx_flag = 0;
            }
        }
        else
        {
            esp_uart4_packet.bytes[esp_uart4_cnt++] = byte;
            if(esp_uart4_cnt >= sizeof(Esp_Uart4_Data_t))
            {
                if(esp_uart4_packet.data.head == 0xB5 && esp_uart4_packet.data.tail == 0x5B)
                {
                    // 只有帧头帧尾都正确时，才更新收到的参数。
                    esp_target_x = esp_uart4_packet.data.target_x;
                    esp_target_y = esp_uart4_packet.data.target_y;
                    esp_pid_kp = esp_uart4_packet.data.pid_kp;
                    esp_pid_ki = esp_uart4_packet.data.pid_ki;
                    esp_pid_kd = esp_uart4_packet.data.pid_kd;
                    esp_uart4_rx_flag = 1;
                }

                // 无论本帧是否有效，收满后都回到等待下一帧头。
                esp_uart4_cnt = 0;
            }
        }
    }
        
    // 清掉溢出标志，避免后续接收被异常状态卡住。
    LPUART_ClearStatusFlags(LPUART4, kLPUART_RxOverrunFlag);
}