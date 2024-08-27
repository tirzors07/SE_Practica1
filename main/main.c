#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <stdio.h>
/
#define DELAY_LEDS 500 
#define DELAY_COINS 500

#define US_INACITIVITY 200000
static uint16_t saldo = 0;
static int64_t last_time = 0;
static int64_t current_time;
static int64_t time_inactivity = 0;
static uint8_t flag_ticket = 0;
QueueHandle_t handlerQueue;
static uint8_t flag_finishInsert = 0;

uint8_t array_init[4][2] = { //matriz para numero de botones y leds
   // btn   leds
    {16, 5},  //$10
    {17, 18}, //$5
    {22, 19}, //$1
    {23, 21}  // ticket
};
typedef enum
{
    UN_PESO = 16,
    CINCO_PESOS = 17,
    DIEZ_PESOS = 22,
    VEINTE_PESOS = 23,
} State_t;
typedef enum
{
    STATE_INIT = 0,
    STATE_INSERTCOIN,
    STATE_RETURNCOIN,
    STATE_TICKET,
} State_m;
void gpios_init();
int insertarMonedas();//retorna la cantidad total de numberCoinss insertadas
void secuenciaCambio(uint8_t cambio, uint8_t piNumber);//funcion para representar cambio en leds
void cambio(uint8_t, uint8_t, uint8_t, uint8_t);//funcion que retorna cambio total y  utliza la func secuenciaCambio
void LED_Control_Task(void *params);
static void IRAM_ATTR gpio_interrupt_handler(void *args);

void app_main(void)
{
    //
}
/*static void IRAM_ATTR gpio_interrupt_handler(void *args)
{
  int pinNumber = (int) args;
  int64_t current_time = esp_timer_get_time();
  if (((current_time - last_time) >= 150000))
  {
    last_time = current_time;
    xQueueSendFromISR(handlerQueue, &pinNumber, NULL);
  }
}*/
void LED_Control_Task(void *params)
{
    State_m current_state;
    while (1)
    {
        insertarMonedas();
        if(saldo >= 15 && flag_finishInsert){
            cambio(saldo,0,0,0);//func para retornar cambio
            flag_ticket = 1;
        }else{
            flag_ticket = 0;
        }
        if(flag_ticket){
            gpio_set_level(array_init[3][1], 1);//encender gpio 23 o el configurado para confirmar ticket
            vTaskDelay( 500/portTICK_PERIOD_MS);
            gpio_set_level(array_init[3][1], 0);
        }
    }
}
void cambio(uint8_t cantidad, uint8_t diez, uint8_t cinco, uint8_t un_peso)
{
    if(cantidad == 0){
        secuenciaCambio(diez,array_init[0][0]);
        vTaskDelay(500/portTICK_PERIOD_MS);
        secuenciaCambio(cinco,array_init[0][1]);
        vTaskDelay(500/portTICK_PERIOD_MS);
        secuenciaCambio(un_peso,array_init[0][2]);
        vTaskDelay(500/portTICK_PERIOD_MS);
    }
    else if (cantidad >= 10){
        cambio(cantidad - 10, diez + 1, cinco, un_peso);
    }
    else if (cantidad >= 5){
        cambio(cantidad - 5, diez, cinco + 1, un_peso);
    }
    else if (cantidad >= 1){
        cambio(cantidad - 1, diez, cinco, un_peso + 1);
    }
}
void secuenciaCambio(uint8_t numberCoins, uint8_t piNumber)
{
    for (uint8_t i = 0; i < numberCoins; i++){
        gpio_set_level(piNumber, 1);
        vTaskDelay( 500/portTICK_PERIOD_MS);
        gpio_set_level(piNumber, 0);
    }
}
int insertarMonedas()
{
    int pinNumber;
        if (xQueueReceive(handlerQueue, &pinNumber, portMAX_DELAY)){
            switch (pinNumber){
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
            if((current_time - time_inactivity) >= 200000 && (saldo > 0)){
                flag_finishInsert = 1;
                //flag_ticket = 1;
                return saldo;//aqui nose aun si es lo correcto xdd
            }else{
                flag_finishInsert = 0;
                //flag_ticket = 0;
            }
        }//help me
}
