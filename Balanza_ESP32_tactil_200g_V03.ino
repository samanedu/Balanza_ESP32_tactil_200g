/*
Conexion del Hardware según archivo User_Setup.h 
TFT ILI9341 240x320 with touch & SD card reader ---> ESP-WR00M32 (ESP32 DEVKIT V1-DOIT)
Seccionar Placa (ESP32 Wrover Module)
(confirmar esta conexión en el archivo User_Setup.h de la biblioteca TFT_eSPI de Bodmer)
Descomentar la siguente line en el archivo User_Setup.h
#define TOUCH_CS PIN_D2     // Chip select pin (T_CS) of touch screen

#define TFT_MOSI 23 // In some display driver board, it might be written as "SDA" and so on.
#define TFT_SCLK 18
#define TFT_CS   5  // Chip select control pin
#define TFT_DC   2  // Data Command control pin
#define TFT_RST  4  // Reset pin (could connect to Arduino RESET pin)
//#define TFT_BL   22  // LED back-light

#define TOUCH_CS 15     // Chip select pin (T_CS) of touch screen
#define TFT_MISO 19 // Lo usa el TOUCH en TFT_TP_OUT (MISO)


-----------LCD
TFT_VDD           ----> ESP32 PIN 3V3
TFT_GND           ----> ESP32 PIN GND
TFT_CS            ----> ESP32 PIN D5
TFT_RST           ----> ESP32 PIN D4 
TFT_DC            ----> ESP32 PIN D2
TFT_SDI (MOSI)    ----> ESP32 PIN D23
TFT_SCK           ----> ESP32 PIN D18
TFT_BL  (LED)     ----> 3V3
TFT_SDO (MISO)    ----> NC (NO CONECTAR)
----------TOUCH
TFT_TP_CLK        ----> ESP32 PIN D18
TFT_TP_CS         ----> ESP32 PIN D15
TFT_TP_DIN        ----> ESP32 PIN D23
TFT_TP_OUT (MISO) ----> ESP32 PIN D19
TFT_TP_IRQ        ----> NC (NO CONECTAR)
---------CARD
SD_CS             ----> ESP32 PIN D25 (but you can change)
SD_D1             ----> ESP32 PIN D23
SD_D0             ----> ESP32 PIN D19
SD_CLK            ----> ESP32 PIN D18
--------------------------------------
*/


// Incluyendo las librerías necesarias
#include "FS.h"               // Sistema de archivos para guardar datos en memoria
#include <SPI.h>              // Protocolo de comunicación SPI
#include <TFT_eSPI.h>         // Librería para controlar pantallas TFT
#include "Free_Fonts.h"       // Librería para usar diferentes fuentes
#include <HX711_ADC.h>        // Librería para controlar el sensor de carga HX711
#include <Wire.h>             // Comunicación I2C (no usada en este código)
#include <Average.h>          // Librería para calcular promedios

#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>           // Librería para leer y escribir en la EEPROM
#endif

// Definición de pines para el TFT y el panel táctil
#define TFT_CS 5     // Pin CS del TFT
#define TOUCH_CS 15  // Pin CS del panel táctil

// Definiciones para el sensor de celda de carga HX711
#define HX711_dout 32 // Pin de datos (DOUT) de HX711
#define HX711_sck 33  // Pin de reloj (SCK) de HX711

// Dimensiones y posición de los botones en la pantalla
#define FRAME_X 220   // Coordenada X del botón TARA
#define FRAME_Y 170   // Coordenada Y del botón TARA
#define FRAME_W 80   // Ancho del botón TARA
#define FRAME_H 60    // Altura del botón TARA

// Archivo para almacenar los datos de calibración del panel táctil
#define CALIBRATION_FILE "/TouchCalData3"

// Si se pone en true, se repite la calibración del panel táctil
#define REPEAT_CAL false

// Función para reiniciar el ESP32
#define funcReset ESP.restart // Función de puntero que apunta al resetmen ESP32

// Variables globales para la lógica de la báscula
float peso = 0.00;  // Peso leído por el sensor
float TARA = 0.00;  // Valor de la tara
float cifra = 0.00; // Valor del peso neto (peso - TARA)
bool CorrigeTara = false; // Indica si se debe corregir la tara
float calibrationValue = 0; // Valor de calibración del sensor de carga
const int calVal_eepromAdress = 0; // Dirección en la EEPROM para guardar el valor de calibración

// Inicialización de objetos globales
TFT_eSPI tft = TFT_eSPI();          // Objeto de la pantalla TFT
HX711_ADC LoadCell(HX711_dout, HX711_sck); // Objeto de la celda de carga
Average<float> ave(100);  // Objeto para calcular el promedio del peso

//------------------------------------------------------------------------------------------
// Configuración inicial del hardware y el sistema
void setup() {
  EEPROM.begin(512); // Inicializa la EEPROM con 512 bytes de espacio
  EEPROM.get(calVal_eepromAdress, calibrationValue); // Leer el valor guardado en la dirección 0 de la EEPROM y guardarlo en "calibrationValue"
  if (calibrationValue == 0) calibrationValue = 6654.77; // Valor por defecto si no hay calibración guardada

    //Serial.begin(115200);  // Inicializar el puerto serie para depuración
    tft.init();            // Inicializar la pantalla TFT
    tft.setRotation(3);    // Configurar la rotación de la pantalla (3 = horizontal invertida)
    touch_calibrate();     // Calibrar la pantalla táctil

    // Configuración inicial de la pantalla
    tft.fillScreen(TFT_WHITE); // Llenar la pantalla de blanco
    tft.setTextSize(2);        // Tamaño de texto
    tft.setTextColor(TFT_BLUE, TFT_WHITE); // Color del texto (azul) y fondo (blanco)
    tft.setTextDatum(MC_DATUM); // Centrar el texto
    
    tft.drawRoundRect(FRAME_X, FRAME_Y, FRAME_W, FRAME_H, 10, TFT_GREEN); // Dibujar botón "TARA"
    tft.drawString("TARA", FRAME_X + (FRAME_W / 2), FRAME_Y + (FRAME_H / 2)); // Etiqueta del botón "TARA"
    
    tft.drawRoundRect(FRAME_X - 100, FRAME_Y, FRAME_W, FRAME_H, 10, TFT_RED); // Dibujar botón "RESET"
    tft.drawString("RESET", FRAME_X - 100 + (FRAME_W / 2), FRAME_Y + (FRAME_H / 2)); // Etiqueta del botón "RESET"
    
    tft.drawRoundRect(FRAME_X - 200, FRAME_Y, FRAME_W, FRAME_H, 10, TFT_BLACK); // Dibujar botón "CALL"
    tft.drawString("CALL", FRAME_X - 200 + (FRAME_W / 2), FRAME_Y + (FRAME_H / 2)); // Etiqueta del botón "CALL"
    
    
    
    // Configuración del sensor de carga HX711
    LoadCell.begin();        // Iniciar comunicación con HX711
    unsigned long stabilizingtime = 2000; // Tiempo de estabilización de 2 segundos
    boolean _tare = true; // Realizar tara automáticamente
    LoadCell.start(stabilizingtime, _tare);    // Comienza la lectura con tiempo de estabilización
    
    // Verificar si hubo un error de timeout en la tara o la señal 
    if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    while (1);
  } else {
    LoadCell.setCalFactor(calibrationValue); // Establece el Factor de calibración para el sensor de carga
    
  }
    
} // cierra Setup

//------------------------------------------------------------------------------------------
// Bucle principal del programa
void loop() {
  // Leer y promediar 100 lecturas del sensor de carga
   for (int i = 0; i < 100; i++){
     LoadCell.update();            // Actualiza la lectura del sensor
     ave.push(LoadCell.getData()); // Añade el valor de la lectura al objeto de promedio   
    }
    peso = ave.mean(); // Obtiene el promedio del peso
       
    // Manejo de la pantalla táctil
    uint16_t x, y; // Variables para almacenar coordenadas del toque
    if (tft.getTouch(&x, &y)) {  // Verificar si hay un toque en la pantalla
        
        // Verificar si el toque está dentro del área del botón "TARA"
        if ((x > FRAME_X) && (x < (FRAME_X + FRAME_W)) && (y > FRAME_Y) && (y <= (FRAME_Y + FRAME_H))) { 
            CorrigeTara = true;  // Indicar que se debe corregir la tara
            tft.fillRoundRect(40, 60, 180, 80, 10, TFT_RED);  // Mostrar una indicación visual
            delay(2000);         // Pausa de 2 segundos para mostrar la animación
            tft.fillRoundRect(40, 60, 180, 80, 10, TFT_WHITE); // Borrar la indicación visual

            // Si es necesario corregir la tara
         if(CorrigeTara=true) {
           for (int i = 0; i < 100; i++) {
             LoadCell.update();            // Actualiza la lectura del sensor
             ave.push(LoadCell.getData()); // Añade la lectura al objeto de promedio
            }
            TARA=ave.mean();     // Ajustar la tara
            CorrigeTara = false; // Resetear la bandera de corrección de tara
         } 
        } 
        

        // Verificar si el toque está dentro del área del botón "RESET"
        if ((x > FRAME_X - 130) && (x < (FRAME_X - 130 + FRAME_W)) && (y > FRAME_Y) && (y <= (FRAME_Y + FRAME_H))) {
          funcReset(); // Llamar a la función de reset
        } 

        // Verificar si el toque está dentro del área del botón "CALL"
        if ((x > FRAME_X - 200) && (x < (FRAME_X - 200 + FRAME_W)) && (y > FRAME_Y) && (y <= (FRAME_Y + FRAME_H))) {
          while (!LoadCell.update()); //bucle que se ejecuta repetidamente hasta que el método LoadCell.update() devuelve true
          masa_calibrate(); //función para calibrar la masa
        } 
    } 

    // Calcular el peso neto (peso menos la tara)
    cifra = peso - TARA;

    // Mostrar el valor en la pantalla TFT
    tft.setTextFont(FONT7);
    tft.setTextSize(0);
    tft.setTextColor(TFT_BLUE, TFT_WHITE); // Color del texto (azul) y fondo (blanco)
 
    tft.setCursor(40, 80);  // Posicionar el cursor en la pantalla
    tft.print(cifra, 2);    // Imprimir el valor del peso neto con 2 decimales
    tft.print("      ");    // Borrar cualquier dígito sobrante

    // Imprimir el peso neto en el monitor serie
    //Serial.print("PESO: ");
    //Serial.println(cifra, 2);
} // cierra el loop

// ------------------------------------------------------------------------------------------
// Función para calibrar la masa
void masa_calibrate() {
  
  while (!LoadCell.update()); // Esperar hasta obtener una lectura

  tft.setTextFont(FONT2);
  tft.setTextSize(2);
  tft.setCursor(10, 10);  // Posicionar el cursor en la pantalla
  tft.print("Despeje platillo"); // Mensaje para retirar el peso del platillo
  delay(5000); // Esperar 5 segundos

  LoadCell.tareNoDelay(); // Realiza la tara sin bloquear la ejecución
    
  tft.setCursor(10, 10);  // Posicionar el cursor en la pantalla
  tft.print("Coloque masa de 200 g"); // Mensaje para colocar una masa de 200g
  delay(5000); // Esperar 5 segundos

  LoadCell.refreshDataSet(); //actualizar o limpiar el conjunto de datos almacenados por el sensor de carga
  float newCalibrationValue = LoadCell.getNewCalibration(200); // Calcular nuevo valor de calibración
  
  // Verificar si el valor de calibración es aceptable
  if (newCalibrationValue > 6624.00 && newCalibrationValue < 6674.00 ){
   EEPROM.put(calVal_eepromAdress, newCalibrationValue); // Guardar el nuevo valor en la EEPROM
   EEPROM.commit(); // Aplicar cambios en la EEPROM
   delay(100);
   tft.setCursor(10, 10);
   tft.print("Retire la masa del platillo"); // Mensaje para retirar la masa
   delay(5000);
   funcReset(); // // Reiniciar el ESP32
  }
  else {
   tft.setCursor(10, 10);  // Posicionar el cursor en la pantalla
   tft.print("ERROR en la calibracion"); // Mensaje de error en calibración
   delay(5000); 
  }
}










