#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <stdio.h>
#define BTN_1 16 // $1
#define BTN_2 17 // $5
#define BTN_3 22 // $10
#define BTN_4 23 // $20
#define PIN_LED1 5//cambio 10
#define PIN_LED2 18//cambio 5
#define PIN_LED3 19//cambio 1
#define PIN_LED4 21// ticket

//const static gpio_num_t INPUT_PINS[4] = {BTN_1,BTN_2,BTN_3,BTN_4};
const static gpio_num_t PIN_LEDS[4] = {PIN_LED1,PIN_LED2,PIN_LED3,PIN_LED4};

static uint16_t saldo = 0;
static int64_t last_time = 0;
static int64_t current_time;
static int64_t time_inactivity = 0;
static uint8_t flag_ticket = 0;
QueueHandle_t handlerQueue;
static uint8_t flag_finishInsert = 0;

typedef enum
{
    Q0_COIN_WAIT = 0,
    Q1_COIN_1_PESO = BTN_1,//btn
    Q2_COIN_5_PESOS = BTN_2,//btn
    Q3_COIN_10_PESOS = BTN_3,//btn
    Q4_COIN_20_PESOS = BTN_4,//btn
    Q5_FULL_PAYMENT = 1,//btn
    Q6_EXCHANGE = 2
} estado_t;

estado_t current_state = Q0_COIN_WAIT;
void gpios_init();
// int insertarMonedas();//retorna la cantidad total de numberCoinss insertadas
void paymentSequence(estado_t);
void secuenciaCambio(uint8_t cambio, uint8_t piNumber); // funcion para representar cambio en leds
void cambio(uint8_t, uint8_t, uint8_t, uint8_t);        // funcion que retorna cambio total y  utliza la func secuenciaCambio
void LED_Control_Task(void *params);
static void IRAM_ATTR gpio_interrupt_handler(void *args);

void app_main(void)
{
    //
}
static void IRAM_ATTR gpio_interrupt_handler(void *args)
{
    int pinNumber = (int)args;
    int64_t current_time = esp_timer_get_time();
    if (((current_time - last_time) >= 150000))
    {
        last_time = current_time;
        xQueueSendFromISR(handlerQueue, &pinNumber, NULL);
    }
}
void LED_Control_Task(void *params)
{
    int pinNumber = 0, count = 0;
    while (1)
    {
        if (xQueueReceive(handlerQueue, &pinNumber, portMAX_DELAY))
        {
            paymentSequence((estado_t)pinNumber);
        }
    }
}
void cambio(uint8_t cantidad, uint8_t diez, uint8_t cinco, uint8_t un_peso)
{
    if (cantidad == 0)
    {
        secuenciaCambio(diez, PIN_LEDS[0]);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        secuenciaCambio(cinco, PIN_LEDS[1]);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        secuenciaCambio(un_peso, PIN_LEDS[2]);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    else if (cantidad >= 10)
    {
        cambio(cantidad - 10, diez + 1, cinco, un_peso);
    }
    else if (cantidad >= 5)
    {
        cambio(cantidad - 5, diez, cinco + 1, un_peso);
    }
    else if (cantidad >= 1)
    {
        cambio(cantidad - 1, diez, cinco, un_peso + 1);
    }
}
void secuenciaCambio(uint8_t numberCoins, uint8_t piNumber)
{
    for (uint8_t i = 0; i < numberCoins; i++)
    {
        gpio_set_level(piNumber, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(piNumber, 0);
    }
}
void paymentSequence(estado_t current_state_t)
{
    uint8_t val_coin = 0;
    current_time = esp_timer_get_time();
    switch (current_state_t)
    {
        case Q0_COIN_WAIT:
            current_state = Q0_COIN_WAIT;
            current_time = esp_timer_get_time();
            time_inactivity = current_time;
            val_coin = 0;
        case Q1_COIN_1_PESO:
            val_coin = 1;
            break;
        case Q2_COIN_5_PESOS:
            val_coin = 5;
            break;
        case Q3_COIN_10_PESOS:
            val_coin = 10;
            break;
        case Q4_COIN_20_PESOS:
            val_coin = 20;
            break;
        case Q5_FULL_PAYMENT:
            if((saldo-15) > 0){
            }
            break;
        case Q6_EXCHANGE:
            break;
        default:
            current_state = Q0_COIN_WAIT;
            return;
    }
    saldo += val_coin;
    if ((current_time - time_inactivity) >= 200000 && (saldo > 0))
    {
        //time_inactivity = 0;
        current_state = Q5_FULL_PAYMENT;
    }
}

/*int insertarMonedas()
{
    int pinNumber;
    if (xQueueReceive(handlerQueue, &pinNumber, portMAX_DELAY))
    {
        switch (pinNumber)
        {
        case UN_PESO:
            saldo++;
            break;
        case CINCO_PESOS:
            saldo += 5;
            break;
        case DIEZ_PESOS:
            saldo += 10;
            break;
        case VEINTE_PESOS:
            saldo += 20;
            break;
        default:
            break;
        }
        time_inactivity = current_time;
        if ((current_time - time_inactivity) >= 200000 && (saldo > 0))
        {
            flag_finishInsert = 1;
            // flag_ticket = 1;
            return saldo; // aqui nose aun si es lo correcto xdd
        }
        else
        {
            flag_finishInsert = 0;
            // flag_ticket = 0;
        }
    } // help me
}*/
