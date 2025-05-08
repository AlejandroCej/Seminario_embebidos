#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

// Pins de los segmentos de los displays
const gpio_num_t segment_pins[7] = {4, 5, 6, 7, 15, 16, 17};

// Pins de los transistores de los displays
const gpio_num_t digit_pins[6] = {36, 48, 21, 8, 9, 12};

// Codificación de los caracteres a mostrar en el display 7 segmentos
const uint8_t display_chars[6] = {
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
        io_conf.pin_bit_mask |= (1ULL << segment_pins[i]);

    // Para configurar los pines de los transistores
    for (int i = 0; i < 6; i++)
        io_conf.pin_bit_mask |= (1ULL << digit_pins[i]);

    gpio_config(&io_conf);
}

// Para decodificar el valor hexadecimal a los segmentos del display
void set_segments(uint8_t hex_value)
{
    for (int i = 0; i < 7; i++) {
        // Extract each bit from the hex value and set the corresponding segment
        gpio_set_level(segment_pins[i], (hex_value >> i) & 0x01);
    }
}

// Para cuando el transistor está apagado (HIGH)
void disable_all_digits()
{
    for (int i = 0; i < 6; i++) {
        gpio_set_level(digit_pins[i], 0);
    }
}

// Multiplexado y visualización de los caracteres en el display
void display_task(void *pvParameters)
{
    while (1) {
        for (int i = 0; i < 6; i++) {
            disable_all_digits();
            set_segments(display_chars[i]);
            gpio_set_level(digit_pins[i], 1);
            vTaskDelay(pdMS_TO_TICKS(5));     // El delay para el multiplexado
        }
    }
}

void app_main(void)
{
    init_gpio();
    xTaskCreate(display_task, "display_task", 2048, NULL, 5, NULL);
}
