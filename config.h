// ============================================================
//  config.h — Configuração do dispositivo
//
//  Para ESP32 #1 (DHT22): deixe #define DEVICE_DHT22
//  Para ESP32 #2 (DHT11): comente a linha e descomente #define DEVICE_DHT11
// ============================================================
#pragma once

// --- Selecione o dispositivo ---------------------------------
#define DEVICE_DHT22
// #define DEVICE_DHT11

// --- Pino do sensor ------------------------------------------
// GPIO conectado ao DATA do DHT
// Sugestão: use resistor pull-up de 10kΩ entre DATA e 3.3V
#define DHT_PIN 4

// --- WiFi ----------------------------------------------------
#define WIFI_SSID     "AP204"
#define WIFI_PASSWORD "espereta"

// --- MQTT (IP do seu servidor Windows com Mosquitto) ---------
// Abra o CMD e rode "ipconfig" para encontrar o IPv4 local
#define MQTT_SERVER   "192.168.0.5"   // ← altere para o IP do seu servidor
#define MQTT_PORT     1883
#define MQTT_USER     ""                // deixe vazio se allow_anonymous true
#define MQTT_PASSWORD ""

// --- Intervalo de publicação ---------------------------------
#define PUBLISH_INTERVAL_MS 30000       // 30 segundos
#define RECONNECT_DELAY_MS  5000        // espera entre tentativas

// --- LED de status -------------------------------------------
// GPIO 2 = LED built-in na maioria dos ESP32 DevKit
#define LED_PIN 2

// ============================================================
//  Derivados — não altere abaixo
// ============================================================
#ifdef DEVICE_DHT22
    #include <DHT.h>
    #define SENSOR_TYPE       DHT22
    #define DEVICE_NAME       "ESP32-DHT22"
    #define MQTT_TOPIC_PUB    "sensores/dht22/dados"
    #define MQTT_TOPIC_STATUS "sistema/status/dht22"
    #define MQTT_TOPIC_ALERTA "alertas/conforto"
    #define FONTE_JSON        "dht22"
    #define SENSOR_PRECISAO   "±0.5°C / ±2-5%UR"

#elif defined(DEVICE_DHT11)
    #include <DHT.h>
    #define SENSOR_TYPE       DHT11
    #define DEVICE_NAME       "ESP32-DHT11"
    #define MQTT_TOPIC_PUB    "sensores/dht11/dados"
    #define MQTT_TOPIC_STATUS "sistema/status/dht11"
    #define MQTT_TOPIC_ALERTA "alertas/conforto"
    #define FONTE_JSON        "dht11"
    #define SENSOR_PRECISAO   "±2°C / ±5%UR"

#else
    #error "Defina DEVICE_DHT22 ou DEVICE_DHT11 no config.h"
#endif
