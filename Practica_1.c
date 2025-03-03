#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"
#include <unistd.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

// Definición de pines
#define pinPWM 14     // Pin PWM para el servo
#define BUTTON_PIN_INC 0   // Pin para el botón que incrementa el ángulo
#define BUTTON_PIN_DEC 2   // Pin para el botón que disminuye el ángulo
#define BUTTON_PIN_SPEED 15 // Pin para el botón que ajusta la velocidad
#define BUTTON_90_DEGREES 16

// Definir la etiqueta para el log
static const char* TAG = "BOTONES";

// Definición de tiempos en microsegundos para 0 y 180 grados
int tiempoCeroGrados = 500;    // Tiempo en us para 0 grados
int tiempo180Grados = 2500;    // Tiempo en us para 180 grados

// Función para inicializar el servomotor
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

// Función principal
void app_main(void) {
    init_servo();
    int angulo = 90; // Ángulo inicial en grados
    int delayNormal = 1000;   // Retardo normal en milisegundos
    int delayRapido = 50;    // Retardo rápido en milisegundos

    // Configurar los botones como entrada con pull-up
    gpio_set_direction(BUTTON_PIN_INC, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PIN_INC, GPIO_PULLUP_ONLY); // Configuración pull-up para el botón de incremento

    gpio_set_direction(BUTTON_PIN_DEC, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PIN_DEC, GPIO_PULLUP_ONLY); // Configuración pull-up para el botón de decremento

    gpio_set_direction(BUTTON_PIN_SPEED, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PIN_SPEED, GPIO_PULLUP_ONLY); // Configuración pull-up para el botón de velocidad

    gpio_set_direction(BUTTON_90_DEGREES, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_90_DEGREES, GPIO_PULLUP_ONLY);

    int button_state_inc;
    int button_state_dec;
    int button_state_speed;
    int button_state_90deg;
    int last_button_state_inc = 1; // Inicialmente el estado es alto debido al pull-up
    int last_button_state_dec = 1; // Inicialmente el estado es alto debido al pull-up
    int last_button_state_90deg = 1; 

    while (true) {
        // Leer el estado de los botones
        button_state_inc = gpio_get_level(BUTTON_PIN_INC);
        button_state_dec = gpio_get_level(BUTTON_PIN_DEC);
        button_state_speed = gpio_get_level(BUTTON_PIN_SPEED);
        button_state_90deg = gpio_get_level(BUTTON_90_DEGREES);

        // Determinar el retardo en función de si el botón de velocidad está presionado
        int delay = (button_state_speed == 0) ? delayRapido : delayNormal;

        // Incrementar ángulo si se presiona el botón de incremento
        if (button_state_inc == 0 && last_button_state_inc == 1) {
            ESP_LOGI(TAG, "Botón de incremento presionado");

            // Incrementar el ángulo en 10 grados
            angulo += 10;

            // Limitar el ángulo a un máximo de 180 grados
            if (angulo > 180) {
                angulo = 180;
            }

            // Ajusta el ciclo de trabajo para mover el servo
            uint32_t tiempo = grados_a_us(angulo);
            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, tiempo);
        }

        // Decrementar ángulo si se presiona el botón de decremento
        if (button_state_dec == 0 && last_button_state_dec == 1) {
            ESP_LOGI(TAG, "Botón de decremento presionado");

            // Disminuir el ángulo en 10 grados
            angulo -= 10;

            // Limitar el ángulo a un mínimo de 0 grados
            if (angulo < 0) {
                angulo = 0;
            }

            // Ajusta el ciclo de trabajo para mover el servo
            uint32_t tiempo = grados_a_us(angulo);
            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, tiempo);
        }

        if (button_state_90deg == 0 && last_button_state_90deg == 1) {
            ESP_LOGI(TAG, "Regresando a 90 grados");

            // Disminuir el ángulo en 10 grados
            angulo = 90; 

            // Ajusta el ciclo de trabajo para mover el servo
            uint32_t tiempo = grados_a_us(angulo);
            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, tiempo);
        }

        // Guardar el estado actual de los botones
        last_button_state_inc = button_state_inc;
        last_button_state_dec = button_state_dec;

        // Esperar según el retardo determinado por el botón de velocidad
        vTaskDelay(delay / portTICK_PERIOD_MS);
    }
}
