#include "driver/gpio.h" 
 #include "freertos/FreeRTOS.h" 
 #include "freertos/task.h" 
 #include "esp_log.h" 

 //Pins del led 
 #define A 21 
 #define B 47 
 #define C 48 
 #define D 35 
 #define E 36 
 #define F 37 
 #define G 38 

 #define BOTON_PIN 15 // Pin GPIO al que está conectado el botón 

 // Variable to store the toggled state 
 int cont = 0; 

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

 void app_main() { 
     configurarDisplay();
     while(cont <= 20){
        mostrarNumero(cont);
        cont++;
        if(cont >= 20){
            cont = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
     }
 }
