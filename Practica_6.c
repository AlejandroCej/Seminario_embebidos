#include "sdkconfig.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/gptimer.h"
#include "esp_task_wdt.h"
#include "esp_system.h"
#include <unistd.h>


// Pines de filas
#define ROW1 42
#define ROW2 41
#define ROW3 40
#define ROW4 39

// Pines de columnas
#define COL1 38
#define COL2 37
#define COL3 36
#define COL4 35

//Pines del display
#define pin_catodo_displayUnidades 12
#define pin_catodo_displayDecenas  9 
#define pin_catodo_displayCentenas 8
#define intervaloTimer_us (2000)
#define intervaloTimer2_us (100000)

#define segmento_A 4
#define segmento_B 5
#define segmento_C 6
#define segmento_D 7
#define segmento_E 15
#define segmento_F 16
#define segmento_G 17

#define cero    0x7E
#define uno     0x30
#define dos     0x6D
#define tres    0x79
#define cuatro  0x33
#define cinco   0x5B
#define seis    0x5F
#define siete   0x70
#define ocho    0x7f
#define nueve   0x7b
#define todosApagados   0x00

// Mapeo de caracteres del teclado a valores del display
#define A 0x77  // Representación de 'A' en el display
#define B 0x7F  // Representación de 'B' (similar a '8')
#define C 0x4E  // Representación de 'C'
#define D 0x3D  // Representación de 'D'
#define E 0x4F  // Representación de 'E'
#define F 0x47  // Representación de 'F'
#define asterisco 0x01  // Representación de '-' en el display

uint8_t numerosCodifiados[16] = {
    cero, uno, dos, tres, cuatro, cinco, seis, siete, ocho, nueve, A, B, C, D, E, F
};
//PARTE DE LOS DISPLAYS

 uint8_t unidades = 0;
 uint8_t decenas  = 0;
 uint8_t centenas = 0;
 uint16_t contador = 0;

volatile int activacionDisplays = 0;

volatile char tecla = '-';  // Declaración de la bandera para el primer pin
volatile bool teclaPresionada = false;

static bool IRAM_ATTR on_timer3_alarm(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    activacionDisplays++;
    if(activacionDisplays >= 3){
        activacionDisplays = 0;
    }
    return true;
}



void decodificaSegmentos(uint8_t display){
    //Parte baja
   gpio_set_level(segmento_G, (display & 0b00000001) >> 0);  
   gpio_set_level(segmento_F, (display & 0b00000010) >> 1);  
   gpio_set_level(segmento_E, (display & 0b00000100) >> 2);  
   gpio_set_level(segmento_D, (display & 0b00001000) >> 3);  

   //Parte alta
   gpio_set_level(segmento_C, (display & 0b00010000) >> 4);  
   gpio_set_level(segmento_B, (display & 0b00100000) >> 5); 
   gpio_set_level(segmento_A, (display & 0b01000000) >> 6);  
}


void task_core_1(void *pvParameters) {   
    uint8_t valorDisplay = asterisco;  // Valor inicial del display (asterisco)
    
    while (1) {
        // Decodificar el número o carácter actual
        if (teclaPresionada) {
            teclaPresionada = false;

            // Convertir la tecla presionada al valor del display
            if (tecla >= '0' && tecla <= '9') {
                valorDisplay = numerosCodifiados[tecla - '0'];  // Convertir dígito
                printf("Tecla presionada: %c\n", tecla);
            } else if (tecla == 'A') {
                valorDisplay = A;
                printf("Tecla presionada: %c\n", tecla);
            } else if (tecla == 'B') {
                valorDisplay = B;
                printf("Tecla presionada: %c\n", tecla);
            } else if (tecla == 'C') {
                valorDisplay = C;
                printf("Tecla presionada: %c\n", tecla);
            } else if (tecla == 'D') {
                valorDisplay = D;
                printf("Tecla presionada: %c\n", tecla);
            } else if (tecla == '*') {
                valorDisplay = asterisco;  // Mostrar - para '*'
                printf("Tecla presionada: %c\n", tecla);
            } else if (tecla == '#') {
                valorDisplay = todosApagados;  // Apagar el display para '#'
                printf("Tecla presionada: %c\n", tecla);
            } else {
                valorDisplay = asterisco;  // Valor por defecto
            }
        }

        // Multiplexar el display
        if (activacionDisplays == 0) { 
            gpio_set_level(pin_catodo_displayUnidades, 1);   
            gpio_set_level(pin_catodo_displayDecenas, 0);
            gpio_set_level(pin_catodo_displayCentenas, 0);
            decodificaSegmentos(valorDisplay);        
        } else if (activacionDisplays == 1) {  
            gpio_set_level(pin_catodo_displayUnidades, 0);   
            gpio_set_level(pin_catodo_displayDecenas, 1);   
            gpio_set_level(pin_catodo_displayCentenas, 0);
            decodificaSegmentos(valorDisplay);         
        } else if (activacionDisplays == 2) {
            gpio_set_level(pin_catodo_displayUnidades, 0);   
            gpio_set_level(pin_catodo_displayDecenas, 0);   
            gpio_set_level(pin_catodo_displayCentenas, 1);
            decodificaSegmentos(valorDisplay);     
        }
    }
}

// PARTE DE TECLADO MATRICIAL

volatile int tiempoRetardo = 10;

uint8_t columnas[4] = {COL1, COL2, COL3, COL4};
uint8_t filas[4] = {ROW1,ROW2,ROW3,ROW4};
volatile uint8_t columnaSeleccionada = -1;


// Mapa del teclado 4x4
char mapaTeclado[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

static const char *TAG = "Teclado4x4:";



static bool IRAM_ATTR on_timer_alarm(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    columnaSeleccionada++;
    if(columnaSeleccionada > 3) columnaSeleccionada = -1;
    
     if(columnaSeleccionada == 0){
        gpio_set_level(COL1, 0);
        gpio_set_level(COL2, 1);
        gpio_set_level(COL3, 1);
        gpio_set_level(COL4, 1);
    }
    else if(columnaSeleccionada == 1){
        gpio_set_level(COL1, 1);
        gpio_set_level(COL2, 0);
        gpio_set_level(COL3, 1);
        gpio_set_level(COL4, 1);
    }
    else if(columnaSeleccionada == 2){
        gpio_set_level(COL1, 1);
        gpio_set_level(COL2, 1);
        gpio_set_level(COL3, 0);
        gpio_set_level(COL4, 1);
    }
    else if(columnaSeleccionada == 3){
        gpio_set_level(COL1, 1);
        gpio_set_level(COL2, 1);
        gpio_set_level(COL3, 1);
        gpio_set_level(COL4, 0);
    }
    return true;
}

static void IRAM_ATTR funcionInterrupcion1(void *arg) {
    tecla = mapaTeclado[0][columnaSeleccionada];
    teclaPresionada = true;
}

// Manejador de interrupción para el segundo pin
static void IRAM_ATTR funcionInterrupcion2(void *arg) {
    tecla = mapaTeclado[1][columnaSeleccionada];
    teclaPresionada = true;
}

// Manejador de interrupción para el tercer pin
static void IRAM_ATTR funcionInterrupcion3(void *arg) {
     tecla = mapaTeclado[2][columnaSeleccionada];
    teclaPresionada = true;
}
static void IRAM_ATTR funcionInterrupcion4(void *arg) {
     tecla = mapaTeclado[3][columnaSeleccionada];
    teclaPresionada = true;
}



void configurarTeclado(){
    //Configurar como salida a las columnas.
    for(int i = 0; i<4; i++){
        //i=0,1,2,3
        gpio_reset_pin(columnas[i]);
        gpio_set_direction(columnas[i],GPIO_MODE_OUTPUT);
        gpio_set_level(columnas[i],1);
    }

    gpio_reset_pin(ROW1);
    gpio_set_direction(ROW1, GPIO_MODE_INPUT);
    gpio_set_intr_type(ROW1, GPIO_INTR_NEGEDGE);  // Interrupción en flanco de bajada

    gpio_reset_pin(ROW2);
    gpio_set_direction(ROW2, GPIO_MODE_INPUT);
    gpio_set_intr_type(ROW2, GPIO_INTR_NEGEDGE);  // Interrupción en flanco de bajada

    gpio_reset_pin(ROW3);
    gpio_set_direction(ROW3, GPIO_MODE_INPUT);
    gpio_set_intr_type(ROW3, GPIO_INTR_NEGEDGE);  // Interrupción en flanco de bajada

    gpio_reset_pin(ROW4);
    gpio_set_direction(ROW4, GPIO_MODE_INPUT);
    gpio_set_intr_type(ROW4, GPIO_INTR_NEGEDGE);  // Interrupción en flanco de bajada

    // Instalación del servicio ISR
    gpio_install_isr_service(0);
    
    // Asignar manejadores de interrupciones a cada pin
    gpio_isr_handler_add(ROW1, funcionInterrupcion1, NULL);
    gpio_isr_handler_add(ROW2, funcionInterrupcion2, NULL);
    gpio_isr_handler_add(ROW3, funcionInterrupcion3, NULL);
    gpio_isr_handler_add(ROW4, funcionInterrupcion4, NULL);

}
//Maquinas de estado FSM (Finite State Machine)
void rotaBit(uint8_t estadoActual){
   

}

int8_t leerFilas(){
    int8_t resultado = -1;
    if(gpio_get_level(ROW1) == 0)        resultado = 1;    
    else if(gpio_get_level(ROW2) == 0)   resultado = 2;   
    else if(gpio_get_level(ROW3) == 0)   resultado = 3;    
    else if(gpio_get_level(ROW4) == 0)   resultado = 4;    
    else resultado = -1;

    return resultado;
}

void configurarTimer(){
      // Configuración del timer
    gptimer_handle_t gptimer = NULL;
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,  // Fuente de reloj por defecto (APB o XTAL)
        .direction = GPTIMER_COUNT_UP,  // Contar hacia arriba
        .resolution_hz = 1000000,  // Configurar a 1 MHz para contar en microsegundos
    };
    gptimer_new_timer(&timer_config, &gptimer);  // Crear un nuevo timer con la configuración

    // Configurar la alarma del timer
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = 50000,  // 1 segundo en microsegundos
        .reload_count = 0,  // Reiniciar el contador a 0 después de alcanzar la alarma
        .flags.auto_reload_on_alarm = true,  // Recargar automáticamente la alarma
    };
    gptimer_set_alarm_action(gptimer, &alarm_config);

    // Registrar la función ISR para la alarma del timer
    gptimer_event_callbacks_t cbs = {
        .on_alarm = on_timer_alarm,  // Llamar a la función on_timer_alarm al activarse la alarma
    };
    gptimer_register_event_callbacks(gptimer, &cbs, NULL);

    // Iniciar el timer
    gptimer_enable(gptimer);  // Habilitar el timer
    gptimer_start(gptimer);   // Iniciar el timer
}

void app_main() {
    configurarTeclado();
    configurarTimer();
    esp_task_wdt_deinit();

    gpio_reset_pin(pin_catodo_displayUnidades);
    gpio_reset_pin(pin_catodo_displayDecenas);
    gpio_reset_pin(pin_catodo_displayCentenas);
    gpio_reset_pin(segmento_A);
    gpio_reset_pin(segmento_B);
    gpio_reset_pin(segmento_C);
    gpio_reset_pin(segmento_D);
    gpio_reset_pin(segmento_E);
    gpio_reset_pin(segmento_F);
    gpio_reset_pin(segmento_G);

    gpio_set_direction(pin_catodo_displayUnidades, GPIO_MODE_OUTPUT);
    gpio_set_direction(pin_catodo_displayDecenas, GPIO_MODE_OUTPUT);
    gpio_set_direction(pin_catodo_displayCentenas, GPIO_MODE_OUTPUT);
    gpio_set_direction(segmento_A, GPIO_MODE_OUTPUT);
    gpio_set_direction(segmento_B, GPIO_MODE_OUTPUT);
    gpio_set_direction(segmento_C, GPIO_MODE_OUTPUT);
    gpio_set_direction(segmento_D, GPIO_MODE_OUTPUT);
    gpio_set_direction(segmento_E, GPIO_MODE_OUTPUT);
    gpio_set_direction(segmento_F, GPIO_MODE_OUTPUT);
    gpio_set_direction(segmento_G, GPIO_MODE_OUTPUT);


    printf("Iniciando programa en ESP32-S3 con FreeRTOS\n");

     // Configuración del timer
     gptimer_handle_t gptimer = NULL;
     gptimer_config_t timer_config = {
         .clk_src = GPTIMER_CLK_SRC_DEFAULT,  // Fuente de reloj por defecto (APB o XTAL)
         .direction = GPTIMER_COUNT_UP,  // Contar hacia arriba
         .resolution_hz = 1000000,  // Configurar a 1 MHz para contar en microsegundos
     };
     ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));  // Crear un nuevo timer con la configuración
 
     // Configurar la alarma del timer
     gptimer_alarm_config_t alarm_config = {
         .alarm_count = intervaloTimer_us,  // 100 microsegundos
         .flags.auto_reload_on_alarm = true,  // Recargar automáticamente la alarma
     };
     ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
 
     // Registrar la función ISR para la alarma del timer
     gptimer_event_callbacks_t cbs = {
         .on_alarm = on_timer3_alarm,  // Llamar a la función on_timer_alarm al activarse la alarma
     };
     ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));
 
     // Iniciar el timer
     ESP_ERROR_CHECK(gptimer_enable(gptimer));  // Habilitar el timer
     ESP_ERROR_CHECK(gptimer_start(gptimer));   // Iniciar el timer


      // Configuración del segundo timer
    gptimer_handle_t gptimer2 = NULL;
    gptimer_config_t timer2_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,  // 1 MHz para contar en microsegundos
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer2_config, &gptimer2));  // Crear el segundo timer

    // Configurar la alarma del segundo timer
    gptimer_alarm_config_t alarm2_config = {
        .alarm_count = intervaloTimer2_us,  // 1 segundo en microsegundos
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer2, &alarm2_config));

    // Iniciar el segundo timer
    ESP_ERROR_CHECK(gptimer_enable(gptimer2));
    ESP_ERROR_CHECK(gptimer_start(gptimer2));
    
    xTaskCreatePinnedToCore(
        task_core_1,   // Función de la tarea
        "TaskCore1",   // Nombre de la tarea
        2048,          // Tamaño del stack en bytes
        NULL,          // Parámetro de entrada
        1,             // Prioridad de la tarea
        NULL,          // Handle de la tarea
        1              // Núcleo en el que se ejecutará
    );


    while (true) {
        if (teclaPresionada) {
            ESP_LOGI(TAG, "Tecla presionada: %c", tecla);
            teclaPresionada = false;
        }
        vTaskDelay(pdMS_TO_TICKS(tiempoRetardo));        
    }
}
