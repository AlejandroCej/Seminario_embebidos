#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_task_wdt.h"


// Pins de los segmentos de los displays
const gpio_num_t DisplayPins[7] = {4, 5, 6, 7, 15, 16, 17};

// Pins de los transistores de los displays
const gpio_num_t TransPins[6] = {36, 48, 21, 8, 9, 12};

// Codificación de los caracteres a mostrar en el display 7 segmentos
const uint8_t Letras[6] = {
    0x1E, // J
    0x79, // E
    0x6D, // S
    0x3E, // U
    0x6D, // S
    0x66  // 4
};

void init_gpio()
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = 0,
        .pull_up_en = 0,
        .pin_bit_mask = 0
    };

    // Para configurar los pines de los segmentos
    for (int i = 0; i < 7; i++)
        io_conf.pin_bit_mask |= (1ULL << DisplayPins[i]);

    // Para configurar los pines de los transistores
    for (int i = 0; i < 6; i++)
        io_conf.pin_bit_mask |= (1ULL << TransPins[i]);

    gpio_config(&io_conf);
}

// Para decodificar el valor hexadecimal a los segmentos del display
void DecodificaSegmentos(uint8_t hex_value)
{
    for (int i = 0; i < 7; i++) {
        // Extract each bit from the hex value and set the corresponding segment
        gpio_set_level(DisplayPins[i], (hex_value >> i) & 0x01);
    }
}

// Para el multiplexado de los displays
// Se apagan todos los transistores
void MultiPins()
{
    for (int i = 0; i < 6; i++) {
        gpio_set_level(TransPins[i], 0);
    }
}

// Visualización de los caracteres en el display
void MostrarNumero(void *pvParameters)
{
    while (1) {
        for (int i = 0; i < 6; i++) {
            MultiPins();
            DecodificaSegmentos(Letras[i]);
            gpio_set_level(TransPins[i], 1);
            vTaskDelay(pdMS_TO_TICKS(5));     // El delay para el multiplexado
        }
    }
}

void app_main(void)
{
    esp_task_wdt_deinit(); // Delete the watchdog for this task
    init_gpio();
    xTaskCreate(MostrarNumero, "MostrarNumero", 2048, NULL, 5, NULL);
}
