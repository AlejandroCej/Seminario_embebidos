#include "driver/gpio.h" 
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h" 
#include "esp_log.h"
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "sdkconfig.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"

//Pins del led 
#define A 21 
#define B 47 
#define C 48 
#define D 35 
#define E 36 
#define F 37 
#define G 38 

#define BOTON4 4
#define BOTON5 5
#define BOTON6 6 // Pin GPIO al que está conectado el botón

#define pinPWM 14     // Pin PWM para el servo
// Definición de tiempos en microsegundos para 0 y 180 grados
int tiempoCeroGrados = 500;    // Tiempo en us para 0 grados
int tiempo180Grados = 2500;    // Tiempo en us para 180 grados

volatile uint8_t bandera1 = 0;  // Declaración de la bandera para el primer pin
volatile uint8_t bandera2 = 0;  // Declaración de la bandera para el segundo pin
volatile uint8_t bandera3 = 0;  // Declaración de la bandera para el tercer pin

static const char *TAG = "INTERRUPCIONES";  // Tag para identificar los logs


// Manejador de interrupción para el primer pin
static void IRAM_ATTR funcionInterrupcion1(void *arg) {
    bandera1 = 1;
}

// Manejador de interrupción para el segundo pin
static void IRAM_ATTR funcionInterrupcion2(void *arg) {
    bandera2 = 1;
}

// Manejador de interrupción para el tercer pin
static void IRAM_ATTR funcionInterrupcion3(void *arg) {
    bandera3 = 1;
}

void configurarDisplay() { 
     gpio_set_direction(A, GPIO_MODE_OUTPUT); 
     gpio_set_direction(B, GPIO_MODE_OUTPUT); 
     gpio_set_direction(C, GPIO_MODE_OUTPUT); 
     gpio_set_direction(D, GPIO_MODE_OUTPUT); 
     gpio_set_direction(E, GPIO_MODE_OUTPUT); 
     gpio_set_direction(F, GPIO_MODE_OUTPUT); 
     gpio_set_direction(G, GPIO_MODE_OUTPUT); 
} 

void mostrarNumero(int numero) { 
    uint8_t segmentos[20] = { 
         0x7E, // 0 
         0x30, // 1 
         0x6D, // 2 
         0x79, // 3 
         0x33, // 4 
         0x5B, // 5 
         0x1F, // 6 
         0x70, // 7 
         0x7F, // 8 
         0x73, // 9 
         0x77, //A 
         0x7F, //B 
         0x4E, //C 
         0x7E, //D 
         0x4F, //E 
         0x47, //F 
         0x5F, //G 
         0x37, //H 
         0x30, //I 
         0x38, //J  
         }; 

     gpio_set_level(A, (segmentos[numero] >> 6) & 1); 
     gpio_set_level(B, (segmentos[numero] >> 5) & 1); 
     gpio_set_level(C, (segmentos[numero] >> 4) & 1); 
     gpio_set_level(D, (segmentos[numero] >> 3) & 1); 
     gpio_set_level(E, (segmentos[numero] >> 2) & 1); 
     gpio_set_level(F, (segmentos[numero] >> 1) & 1); 
     gpio_set_level(G, (segmentos[numero] >> 0) & 1); 
} 

void init_servo() {
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, pinPWM); // Configura GPIO 4 como salida PWM

    mcpwm_config_t pwm_config;
    pwm_config.frequency = 50;    // Frecuencia de 50 Hz (para servos)
    pwm_config.cmpr_a = 0;        // Ciclo de trabajo inicial del canal A
    pwm_config.cmpr_b = 0;        // No usamos el canal B
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config); // Inicializa el MCPWM con esta configuración
}
// Función que convierte grados a tiempo en alto (us) para el servo
uint32_t grados_a_us(int grados) {
    return (tiempoCeroGrados + ((tiempo180Grados - tiempoCeroGrados) * grados / 180)); // Convierte de 0 a 180 grados en el rango definido
}
void app_main() {
    // Configuración de los pines de entrada
    gpio_reset_pin(BOTON4);
    gpio_set_direction(BOTON4, GPIO_MODE_INPUT);
    gpio_set_intr_type(BOTON4, GPIO_INTR_NEGEDGE);  // Interrupción en flanco de bajada
    
    gpio_reset_pin(BOTON5);
    gpio_set_direction(BOTON5, GPIO_MODE_INPUT);
    gpio_set_intr_type(BOTON5, GPIO_INTR_NEGEDGE);  // Interrupción en flanco de bajada
    
    gpio_reset_pin(BOTON6);
    gpio_set_direction(BOTON6, GPIO_MODE_INPUT);
    gpio_set_intr_type(BOTON6, GPIO_INTR_NEGEDGE);  // Interrupción en flanco de bajada
    
    // Instalación del servicio ISR
    gpio_install_isr_service(0);
    
    // Asignar manejadores de interrupciones a cada pin
    gpio_isr_handler_add(BOTON4, funcionInterrupcion1, NULL);
    gpio_isr_handler_add(BOTON5, funcionInterrupcion2, NULL);
    gpio_isr_handler_add(BOTON6, funcionInterrupcion3, NULL);
    int cuentaInt0 = 0;
    int cuentaInt1 = 0;
    int cuentaInt2 = 0;

    configurarDisplay();

    init_servo();
    int angulo = 0;

    while (true) {
        if (bandera1) {  // Revisamos la bandera del primer pin
            mostrarNumero(0);
            ESP_LOGI(TAG, "Interrupción detectada en pin %d, se ha ejecutado %d, la funcion de interrupcion", BOTON4, cuentaInt0);
            
            // Disminuir el ángulo en 10 grados
            angulo = 0; 
            // Ajusta el ciclo de trabajo para mover el servo
            uint32_t tiempo = grados_a_us(angulo);
            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, tiempo);

            bandera1 = 0;  // Reiniciar la bandera
            cuentaInt0++;
        }
        if (bandera2) {  // Revisamos la bandera del segundo pin
            mostrarNumero(9);
            ESP_LOGI(TAG, "Interrupción detectada en pin %d, se ha ejecutado %d, la funcion de interrupcion", BOTON5, cuentaInt1);
                        
            // Disminuir el ángulo en 10 grados
            angulo = 90; 
            // Ajusta el ciclo de trabajo para mover el servo
            uint32_t tiempo = grados_a_us(angulo);
            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, tiempo);

            bandera2 = 0;  // Reiniciar la bandera
            cuentaInt1++;
        }
        if (bandera3) {  // Revisamos la bandera del tercer pin
            mostrarNumero(10);
            ESP_LOGI(TAG, "Interrupción detectada en pin %d, se ha ejecutado %d, la funcion de interrupcion", BOTON6, cuentaInt2);
                        
            // Disminuir el ángulo en 10 grados
            angulo = 180; 
            // Ajusta el ciclo de trabajo para mover el servo
            uint32_t tiempo = grados_a_us(angulo);
            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, tiempo);

            bandera3 = 0;  // Reiniciar la bandera
            cuentaInt2++;
        }
        usleep(500 * 1000);  // Retardo de 500ms
    }
}
