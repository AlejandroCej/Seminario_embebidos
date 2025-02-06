#include <unistd.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_task_wdt.h"

#define pinLED 12
#define ALTO 1
#define BAJO 0

// Definimos los tiempos en microsegundos
int tiempoBajo = 7500; // 100 ms
int tiempoAlto = 833; // 100 ms

void app_main(void) {
    esp_task_wdt_deinit();
    int cuenta = 0;
    printf("Cuenta: %d\n",cuenta);
    gpio_reset_pin(pinLED);
    gpio_set_direction(pinLED, GPIO_MODE_OUTPUT);

    while (true) {
        gpio_set_level(pinLED, ALTO);
        usleep(tiempoAlto); // Convertir ms a microsegundos

        gpio_set_level(pinLED, BAJO);
        usleep(tiempoBajo); // Convertir ms a microsegundos
    }
}
