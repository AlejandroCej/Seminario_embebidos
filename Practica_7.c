#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_task_wdt.h"

// Pines de los displays de 7 segmentos
#define SEG_A 4
#define SEG_B 5
#define SEG_C 6
#define SEG_D 7
#define SEG_E 15
#define SEG_F 16
#define SEG_G 17

#define CATODO_UNIDADES 8
#define CATODO_DECENAS 9
#define CATODO_CENTENAS 12

// Pines del teclado matricial
#define COL_1 35
#define COL_2 36
#define COL_3 37
#define COL_4 38
#define FIL_1 39
#define FIL_2 40
#define FIL_3 41
#define FIL_4 42

// Pin del sensor LM35
#define LM35_ADC_CHANNEL ADC_CHANNEL_2  // GPIO 2

// Variables globales
volatile float temperatura = 0.0;  // Temperatura actual
volatile bool mostrarCelsius = true;  // Modo de temperatura (Celsius o Fahrenheit)
QueueHandle_t colaTeclado;  // Cola para manejar las teclas presionadas

// Mapeo de números a segmentos del display
const uint8_t numerosCodificados[10] = {
    0b0111111,  // 0
    0b0000110,  // 1
    0b1011011,  // 2
    0b1001111,  // 3
    0b1100110,  // 4
    0b1101101,  // 5
    0b1111101,  // 6
    0b0000111,  // 7
    0b1111111,  // 8
    0b1101111   // 9
};

// Configuración de pines GPIO
void configurarGPIO() {
    // Configurar pines de segmentos como salida
    uint8_t pinesSegmentos[] = {SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G};
    for (int i = 0; i < 7; i++) {
        gpio_reset_pin(pinesSegmentos[i]);
        gpio_set_direction(pinesSegmentos[i], GPIO_MODE_OUTPUT);
    }

    // Configurar pines de catodos como salida
    uint8_t pinesCatodos[] = {CATODO_UNIDADES, CATODO_DECENAS, CATODO_CENTENAS};
    for (int i = 0; i < 3; i++) {
        gpio_reset_pin(pinesCatodos[i]);
        gpio_set_direction(pinesCatodos[i], GPIO_MODE_OUTPUT);
    }

    // Configurar pines del teclado matricial
    uint8_t columnas[] = {COL_1, COL_2, COL_3, COL_4};
    uint8_t filas[] = {FIL_1, FIL_2, FIL_3, FIL_4};

    for (int i = 0; i < 4; i++) {
        gpio_reset_pin(columnas[i]);
        gpio_set_direction(columnas[i], GPIO_MODE_OUTPUT);
        gpio_set_level(columnas[i], 1);  // Inicialmente en alto

        gpio_reset_pin(filas[i]);
        gpio_set_direction(filas[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(filas[i], GPIO_PULLUP_ONLY);
    }
}

// Leer el valor del LM35 y convertirlo a temperatura
float leerTemperatura() {
    int raw = adc1_get_raw(LM35_ADC_CHANNEL);
    float voltaje = (raw * 3.3) / 4095.0;  // Convertir a voltaje
    return voltaje * 100.0;  // LM35: 10mV/°C
}

// Decodificar y mostrar un número en los displays
void mostrarNumero(int numero) {
    int unidades = numero % 10;
    int decenas = (numero / 10) % 10;
    int centenas = (numero / 100) % 10;

    uint8_t pinesCatodos[] = {CATODO_UNIDADES, CATODO_DECENAS, CATODO_CENTENAS};
    int valores[] = {unidades, decenas, centenas};

    for (int i = 0; i < 3; i++) {
        // Encender el catodo correspondiente
        gpio_set_level(pinesCatodos[i], 1);

        // Decodificar el número y encender los segmentos
        uint8_t segmentos = numerosCodificados[valores[i]];
        gpio_set_level(SEG_A, segmentos & 0x01);
        gpio_set_level(SEG_B, segmentos & 0x02);
        gpio_set_level(SEG_C, segmentos & 0x04);
        gpio_set_level(SEG_D, segmentos & 0x08);
        gpio_set_level(SEG_E, segmentos & 0x10);
        gpio_set_level(SEG_F, segmentos & 0x20);
        gpio_set_level(SEG_G, segmentos & 0x40);

        // Esperar un breve tiempo para multiplexar
        vTaskDelay(pdMS_TO_TICKS(5));

        // Apagar el catodo
        gpio_set_level(pinesCatodos[i], 0);
    }
}

// Tarea para manejar el teclado matricial
void task_teclado(void *pvParameters) {
    uint8_t columnas[] = {COL_1, COL_2, COL_3, COL_4};
    uint8_t filas[] = {FIL_1, FIL_2, FIL_3, FIL_4};
    char mapaTeclado[4][4] = {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}
    };

    while (1) {
        for (int col = 0; col < 4; col++) {
            gpio_set_level(columnas[col], 0);  // Activar columna
            for (int fila = 0; fila < 4; fila++) {
                if (gpio_get_level(filas[fila]) == 0) {  // Detectar tecla presionada
                    char tecla = mapaTeclado[fila][col];
                    xQueueSend(colaTeclado, &tecla, portMAX_DELAY);
                    while (gpio_get_level(filas[fila]) == 0) {
                        vTaskDelay(pdMS_TO_TICKS(10));  // Esperar a que se suelte
                    }
                }
            }
            gpio_set_level(columnas[col], 1);  // Desactivar columna
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Tarea para manejar la temperatura y el display
void task_temperatura(void *pvParameters) {
    while (1) {
        temperatura = leerTemperatura();  // Leer temperatura del LM35
        int tempMostrar = (int)(mostrarCelsius ? temperatura : (temperatura * 9.0 / 5.0) + 32.0);
        mostrarNumero(tempMostrar);  // Mostrar temperatura en el display
    }
}

// Tarea para manejar las teclas presionadas
void task_manejar_teclas(void *pvParameters) {
    char tecla;
    while (1) {
        if (xQueueReceive(colaTeclado, &tecla, portMAX_DELAY)) {
            if (tecla == '1') {
                mostrarCelsius = true;  // Cambiar a Celsius
            } else if (tecla == '2') {
                mostrarCelsius = false;  // Cambiar a Fahrenheit
            }
        }
    }
}

// Configuración inicial
void app_main() {
    // Disable the task watchdog timer
    esp_task_wdt_deinit();

    // Your existing initialization code
    configurarGPIO();

    // Configurar ADC para el LM35
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(LM35_ADC_CHANNEL, ADC_ATTEN_DB_11);

    // Crear cola para el teclado
    colaTeclado = xQueueCreate(10, sizeof(char));

    // Crear tareas
    xTaskCreate(task_teclado, "Teclado", 2048, NULL, 1, NULL);
    xTaskCreate(task_temperatura, "Temperatura", 2048, NULL, 1, NULL);
    xTaskCreate(task_manejar_teclas, "ManejarTeclas", 2048, NULL, 1, NULL);
}
