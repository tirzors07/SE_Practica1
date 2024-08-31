#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <stdio.h>
#define BTN_1 16    // $1
#define BTN_2 17    // $5
#define BTN_3 22    // $10
#define BTN_4 23    // $20
#define PIN_LED1 5  // cambio 10
#define PIN_LED2 18 // cambio 5
#define PIN_LED3 19 // cambio 1
#define PIN_LED4 21 // ticket
const static gpio_num_t INPUT_PINS[4] = {BTN_1, BTN_2, BTN_3, BTN_4};
const static gpio_num_t PIN_LEDS[4] = {PIN_LED1, PIN_LED2, PIN_LED3, PIN_LED4};
static uint16_t saldo_btn = 0;
static int64_t current_time = 0;
static int64_t last = 0;
QueueHandle_t handlerQueue;
typedef struct Payment
{
  int8_t saldo;               // saldo total
  int64_t last_time;          // tiempo anterior
  TaskHandle_t pxCreatedTask; // manejador de tarea
} Payment_t;
typedef enum
{
  Q0_PAYMENT_RESTART = 0,
  Q1_COIN_WAIT,
  Q2_PAYMENT,
  Q3_FULL_PAYMENT,
  Q4_EXCHANGE
} estado_t;
void gpios_init();
void insert_coins_Task(void *params); // retorna la cantidad total de numberCoinss insertadas
void paymentSequence(estado_t *, Payment_t *payment);
void secuenciaCambio(int8_t numberCoins, int8_t piNumber); // funcion para representar cambio en leds
void cambio(int8_t cantidad);           // funcion que retorna cambio total y  utliza la func secuenciaCambio
static void IRAM_ATTR gpio_interrupt_handler(void *args) {
  int pinNumber = (int)args;
  current_time = esp_timer_get_time();
  if ((( current_time- last) >= 150000))
  {
    last = esp_timer_get_time();
    xQueueSendFromISR(handlerQueue, &pinNumber, NULL);
  }
}
void app_main(void)
{
  Payment_t payment = {0};
  estado_t current_state = Q0_PAYMENT_RESTART;
  gpios_init();
  handlerQueue = xQueueCreate(10, sizeof(int));
  xTaskCreate(insert_coins_Task, "Insert_Coins_Task", 2048, &payment, 1, &payment.pxCreatedTask);
  gpio_install_isr_service(0);
  for (int8_t i = 0; i < 4; i++)
  {
    gpio_isr_handler_add(INPUT_PINS[i], gpio_interrupt_handler, (void *)INPUT_PINS[i]);
  }
  while (1) {
    paymentSequence(&current_state, &payment);
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
void cambio(int8_t cantidad)
{
    int8_t diez = 0, cinco = 0, un_peso = 0;
    while (cantidad > 0) {
        if (cantidad >= 10) {
            cantidad -= 10;
            diez++;
        } else if (cantidad >= 5) {
            cantidad -= 5;
            cinco++;
        } else if (cantidad >= 1) {
            cantidad -= 1;
            un_peso++;
        }
    }
    secuenciaCambio(diez, PIN_LEDS[0]);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    secuenciaCambio(cinco, PIN_LEDS[1]);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    secuenciaCambio(un_peso, PIN_LEDS[2]);
    vTaskDelay(500 / portTICK_PERIOD_MS);
}
void secuenciaCambio(int8_t numberCoins, int8_t piNumber)
{
  for (uint8_t i = 0; i < numberCoins; i++)
  {
    gpio_set_level(piNumber, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(piNumber, 0);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}
void paymentSequence(estado_t *current_state_t, Payment_t *payment)
{
  switch (*current_state_t)
  {
    case Q0_PAYMENT_RESTART: // INICIAL
      printf("Estado 0 en ejecucion\n");
      payment->last_time = 0;
      payment->saldo = 0;
      *current_state_t = Q1_COIN_WAIT;
      break;
    case Q1_COIN_WAIT:
      //vTaskResume(payment->pxCreatedTask); // activa la tarea
      if(payment->saldo != 0){
        *current_state_t = Q2_PAYMENT;
      }
      break;
    case Q2_PAYMENT:
      if ((esp_timer_get_time() - payment->last_time) >= 5000000 && (payment->saldo > 0))
      {
        //vTaskSuspend(payment->pxCreatedTask);
        printf("Tiempo de inactividad agotado\n");
        printf("Redirigiendo a pago completo con %d\n", payment->saldo);
        *current_state_t = Q3_FULL_PAYMENT;
      }
      else
      {
        *current_state_t = Q2_PAYMENT;
      }
      break;
    case Q3_FULL_PAYMENT:
      if (payment->saldo == 15)
      { // devolver ticket y reiniciar
        printf("Has pagado %d\n", payment->saldo);
        gpio_set_level(PIN_LEDS[3], 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(PIN_LEDS[3], 0);
        *current_state_t = Q0_PAYMENT_RESTART; // AL REINICIO
      }
      if (((payment->saldo) - 15) > 0)
      {
        gpio_set_level(PIN_LEDS[3], 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(PIN_LEDS[3], 0);
        *current_state_t = Q4_EXCHANGE; // cambio
        payment->saldo = payment->saldo - 15;
      }
      break;
    case Q4_EXCHANGE:
      printf("Su cambio es %d \n",payment->saldo);
      cambio(payment->saldo);
      *current_state_t = Q0_PAYMENT_RESTART;
      break;
    default:
      *current_state_t = Q0_PAYMENT_RESTART;
      break;
  }
}
void insert_coins_Task(void *params)
{
  Payment_t *payment = (Payment_t *)params;
  int pinNumber;
  while (1)
  {
    if (xQueueReceive(handlerQueue, &pinNumber, portMAX_DELAY)) // entra si recibe algo
    {
      printf("Pin: %d\n",pinNumber);
      switch (pinNumber)
      {
        case BTN_1: // Botón 1
          saldo_btn = 1;
          break;
        case BTN_2: // Botón 2
          saldo_btn = 5;
          break;
        case BTN_3: // Botón 3
          saldo_btn = 10;
          break;
        case BTN_4: // Botón 4
          saldo_btn = 20;
          break;
      }
      payment->last_time = esp_timer_get_time(); // toma el ultimo valor desde que se detecto un pulso
      payment->saldo += saldo_btn;
    }
  }
}
void gpios_init()
{
  for (int8_t i = 0; i < 4; i++)
  { // inicializar leds
    gpio_reset_pin(PIN_LEDS[i]);
    gpio_set_direction(PIN_LEDS[i], GPIO_MODE_OUTPUT);
  }
  for (int8_t i = 0; i < 4; i++)
  { // botones
    gpio_reset_pin(INPUT_PINS[i]);
    gpio_set_direction(INPUT_PINS[i], GPIO_MODE_INPUT);
    gpio_set_pull_mode(INPUT_PINS[i], GPIO_PULLUP_ONLY);
    gpio_set_intr_type(INPUT_PINS[i], GPIO_INTR_POSEDGE);
  }
}
