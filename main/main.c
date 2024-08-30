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

static uint16_t saldo = 0;
static int64_t last_time = 0;
static int64_t current_time;
static int64_t time_inactivity = 0;
static uint8_t flag_ticket = 0;
QueueHandle_t handlerQueue;
static uint8_t flag_finishInsert = 0;

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

estado_t current_state = Q0_PAYMENT_RESTART;

void gpios_init();
void insert_coins_Task(void *params); // retorna la cantidad total de numberCoinss insertadas
void paymentSequence(estado_t *, Payment_t *payment);
void secuenciaCambio(int8_t numberCoins, int8_t piNumber); // funcion para representar cambio en leds
void cambio(uint8_t, uint8_t, uint8_t, uint8_t);           // funcion que retorna cambio total y  utliza la func secuenciaCambio
void LED_Control_Task(void *params);
static void IRAM_ATTR gpio_interrupt_handler(void *args);

void app_main(void)
{
  Payment_t payment = {0};
  estado_t current_state = Q1_COIN_WAIT;
  gpios_init();
  handlerQueue = xQueueCreate(10, sizeof(int));

  // Corregimos la creación de la tarea para pasar la dirección del puntero
  xTaskCreate(insert_coins_Task, "Insert_Coins_Task", 2048, &payment, 1, &payment.pxCreatedTask);

  gpio_install_isr_service(0);
  for (int8_t i = 0; i < 4; i++)
  {
    gpio_isr_handler_add(INPUT_PINS[i], gpio_interrupt_handler, (void *)INPUT_PINS[i]);
  }
  while (1)
  {
    paymentSequence(&current_state, &payment);
    // vTaskDelay(10 / portTICK_PERIOD_MS);
  }
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
void cambio(uint8_t cantidad, uint8_t diez, uint8_t cinco, uint8_t un_peso)
{
  printf("Tu cambio es %d \n", cantidad);
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
    printf("Hola este es el edo 0\n");
    payment->last_time = 0;
    payment->saldo = 0;
    *current_state_t = Q1_COIN_WAIT;
    break;
  case Q1_COIN_WAIT:
    printf("Hola este es el edo 1\n");
    vTaskResume(payment->pxCreatedTask); // activa la tarea
    *current_state_t = Q2_PAYMENT;
    break;
  case Q2_PAYMENT:
    if ((current_time - payment->last_time) >= 2000000 && (payment->saldo > 0))
    {
      vTaskSuspend(payment->pxCreatedTask);
      *current_state_t = Q3_FULL_PAYMENT;
      printf("Tiempo de inactividad agotado\n");
      printf("Redirigiendo a pago completo con %d\n", payment->saldo);
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
    if ((payment->saldo - 15) > 0)
    {
      gpio_set_level(PIN_LEDS[3], 1);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      gpio_set_level(PIN_LEDS[3], 0);
      *current_state_t = Q4_EXCHANGE; // cambio
    }
    break;
  case Q4_EXCHANGE:
    cambio(payment->saldo, 0, 0, 0);
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
    current_time = esp_timer_get_time();
    if (xQueueReceive(handlerQueue, &pinNumber, portMAX_DELAY)) // entra si recibe algo
    {
      switch (pinNumber)
      {
      case BTN_1: // Botón 1
        saldo++;
        break;
      case BTN_2: // Botón 2
        saldo += 5;
        break;
      case BTN_3: // Botón 3
        saldo += 10;
        break;
      case BTN_4: // Botón 4
        saldo += 20;
        break;
      default:
        break;
      }
      payment->last_time = current_time; // toma el ultimo valor desde que se detecto un pulso
      payment->saldo += saldo;
    }
  }
}
/*void LED_Control_Task(void *params)
  {
    int pinNumber = 0, count = 0;
    while (1){
        if (xQueueReceive(handlerQueue, &pinNumber, portMAX_DELAY)){
            //paymentSequence((estado_t)pinNumber);
        }
    }
  }*/
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
    gpio_set_intr_type(INPUT_PINS[i], GPIO_INTR_NEGEDGE);
  }
}
