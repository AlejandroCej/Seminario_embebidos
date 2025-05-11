#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"

// Segmentos de GPIO
const gpio_num_t segment_pins[7] = {4, 5, 6, 7, 15, 16, 17};

// Para los transistores
const gpio_num_t digit_pins[6] = {36, 48, 21, 8, 9, 12};

// Configuración del I2C
#define I2C_MASTER_NUM         I2C_NUM_0
#define I2C_MASTER_SDA_IO      42
#define I2C_MASTER_SCL_IO      41
#define I2C_MASTER_FREQ_HZ     100000
#define DS3231_ADDR            0x68

// Para el Botón
#define BUTTON_PIN 40

// Para el antirrebote
#define DEBOUNCE_TIME_MS 200

// Números en hexadecimal para los displays
const uint8_t digit_to_segments[10] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F  // 9
};

uint8_t display_chars[6] = {0};
volatile bool show_time = true;
int64_t last_button_press_time = 0;

// Convertir BCD a Decimal
uint8_t bcd_a_dec(uint8_t val) {
    return ((val / 16 * 10) + (val % 16));
}

// Inicializar puertos
void init_gpio() {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = 0,
        .pull_up_en = 0,
        .pin_bit_mask = 0
    };

    for (int i = 0; i < 7; i++)
        io_conf.pin_bit_mask |= (1ULL << segment_pins[i]);
    for (int i = 0; i < 6; i++)
        io_conf.pin_bit_mask |= (1ULL << digit_pins[i]);

    gpio_config(&io_conf);

    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_pullup_en(BUTTON_PIN);
}

// Inicializar I2C
void i2c_master_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

// Leer fecha y hora de I2C
void read_ds3231(uint8_t *hours, uint8_t *minutes, uint8_t *seconds,
                 uint8_t *day, uint8_t *month, uint8_t *year) {
    uint8_t data[7];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true); // Start at register 0
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 7, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    *seconds =bcd_a_dec(data[0]);
    *minutes =bcd_a_dec(data[1]);
    *hours   =bcd_a_dec(data[2]);
    *day     =bcd_a_dec(data[4]);
    *month   =bcd_a_dec(data[5] & 0x1F);
    *year    =bcd_a_dec(data[6]);
}

// Actualizar el display cada vez que pasa un segundo
void MostrarHoraFecha() {
    uint8_t h, m, s, d, mo, y;
    read_ds3231(&h, &m, &s, &d, &mo, &y);

    uint8_t digits[6];

    if (show_time) {
        digits[0] = h / 10;
        digits[1] = h % 10;
        digits[2] = m / 10;
        digits[3] = m % 10;
        digits[4] = s / 10;
        digits[5] = s % 10;
    } else {
        digits[0] = d / 10;
        digits[1] = d % 10;
        digits[2] = mo / 10;
        digits[3] = mo % 10;
        digits[4] = y / 10;
        digits[5] = y % 10;
    }

    for (int i = 0; i < 6; i++) {
        display_chars[i] = digit_to_segments[digits[i]];
    }
}

// Para el multiplexado
void ConfigurarMulti() {
    for (int i = 0; i < 6; i++) {
        gpio_set_level(digit_pins[i], 0);
    }
}

// Para mostrar números en los displays
void MostrarNumero(uint8_t value) {
    for (int i = 0; i < 7; i++) {
        gpio_set_level(segment_pins[i], (value >> i) & 0x01);
    }
}

// Multiplexar los displays
void MultiDisplays(void *pvParameters) {
    while (1) {
        for (int i = 0; i < 6; i++) {
            ConfigurarMulti();
            MostrarNumero(display_chars[i]);
            gpio_set_level(digit_pins[i], 1);
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }
}

// Chequeo de botón con protección contra rebote
void TareaBoton(void *pvParameters) {
    while (1) {
        int level = gpio_get_level(BUTTON_PIN);
        int64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds

        if (level == 0 && (current_time - last_button_press_time) > DEBOUNCE_TIME_MS) { // Button pressed
            last_button_press_time = current_time; // Update the last press time
            show_time = !show_time; // Toggle the display mode
            printf("Button pressed, show_time: %d\n", show_time);
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to avoid excessive CPU usage
    }
}

// Leer fecha y hora y actualizar el display
void LeerFechaHora(void *pvParameters) {
    while (1) {
        uint8_t h, m, s, d, mo, y;
        read_ds3231(&h, &m, &s, &d, &mo, &y);

        // Update the display buffer
        MostrarHoraFecha();

        // Print the current time or date to the serial monitor
        if (show_time) {
            printf("Current Time: %02d:%02d:%02d\n", h, m, s);
        } else {
            printf("Current Date: %02d/%02d/%02d\n", d, mo, y);
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // Delay for 500ms
    }
}

void app_main() {
    // Quitar el watchdog del task
    esp_task_wdt_deinit();
    init_gpio();
    i2c_master_init();

    MostrarHoraFecha();

    // Crear tareas
    xTaskCreatePinnedToCore(MultiDisplays, "MultiDisplays", 2048, NULL, 5, NULL, 1);
    xTaskCreate(TareaBoton, "TareaBoton", 2048, NULL, 5, NULL);
    xTaskCreate(LeerFechaHora, "LeerFechaHora", 2048, NULL, 5, NULL);

}
