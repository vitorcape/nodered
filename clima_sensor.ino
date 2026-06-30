// ============================================================
//  clima_sensor.ino
//  Monitor Climático · Firmware ESP32
//
//  Bibliotecas necessárias (instale via Library Manager):
//    - PubSubClient   by Nick O'Leary      (MQTT)
//    - DHT sensor library  by Adafruit     (DHT11/DHT22)
//    - Adafruit Unified Sensor by Adafruit (dependência do DHT)
//    - ArduinoJson    by Benoit Blanchon   (JSON)
//
//  Fluxo:
//    Boot → WiFi → MQTT → lê DHT → publica JSON → aguarda → repete
//    Em background: reconecta WiFi/MQTT se cair
// ============================================================

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

// --- Objetos globais -----------------------------------------
DHT       dht(DHT_PIN, SENSOR_TYPE);
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

// --- Estado --------------------------------------------------
unsigned long ultimaPublicacao  = 0;
unsigned long ultimaReconexao   = 0;
unsigned long bootTime          = 0;
uint32_t      totalPublicacoes  = 0;
bool          alertaAtivo       = false;
String        ultimoAlerta      = "";

// ============================================================
//  Setup
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(500);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Serial.println("\n\n╔══════════════════════════════╗");
    Serial.println("║     ClimaMonitor · ESP32     ║");
    Serial.println("╚══════════════════════════════╝");
    Serial.printf("  Dispositivo : %s\n",    DEVICE_NAME);
    Serial.printf("  Sensor      : %s\n",    FONTE_JSON);
    Serial.printf("  Precisão    : %s\n",    SENSOR_PRECISAO);
    Serial.printf("  Pino DHT    : GPIO %d\n", DHT_PIN);
    Serial.printf("  Intervalo   : %d s\n",  PUBLISH_INTERVAL_MS / 1000);
    Serial.println("──────────────────────────────");

    dht.begin();
    delay(2000); // DHT precisa de estabilização

    conectarWiFi();
    configurarMQTT();
    conectarMQTT();

    bootTime = millis();
    Serial.println("\n[OK] Sistema pronto. Publicando...\n");
}

// ============================================================
//  Loop principal
// ============================================================
void loop() {
    // Mantém WiFi e MQTT ativos
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Conexão perdida. Reconectando...");
        conectarWiFi();
    }

    if (!mqtt.connected()) {
        unsigned long agora = millis();
        if (agora - ultimaReconexao >= RECONNECT_DELAY_MS) {
            ultimaReconexao = agora;
            conectarMQTT();
        }
    }

    mqtt.loop();

    // Publica no intervalo configurado
    if (millis() - ultimaPublicacao >= PUBLISH_INTERVAL_MS) {
        ultimaPublicacao = millis();
        lerEPublicar();
    }
}

// ============================================================
//  WiFi
// ============================================================
void conectarWiFi() {
    Serial.printf("[WiFi] Conectando a '%s'", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int tentativas = 0;
    while (WiFi.status() != WL_CONNECTED && tentativas < 30) {
        delay(500);
        Serial.print(".");
        piscarLED(1, 100);
        tentativas++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] Conectado! IP: %s  RSSI: %d dBm\n",
            WiFi.localIP().toString().c_str(), WiFi.RSSI());
        digitalWrite(LED_PIN, HIGH);
    } else {
        Serial.println("\n[WiFi] Falha. Tentará novamente no próximo ciclo.");
        digitalWrite(LED_PIN, LOW);
    }
}

// ============================================================
//  MQTT
// ============================================================
void configurarMQTT() {
    mqtt.setServer(MQTT_SERVER, MQTT_PORT);
    mqtt.setCallback(callbackMQTT);
    mqtt.setKeepAlive(60);
    mqtt.setBufferSize(512);
}

void conectarMQTT() {
    // Client ID único baseado no MAC do ESP32
    String clientId = String(DEVICE_NAME) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);

    // Last Will — broker publica automaticamente se o ESP32 desconectar
    String willPayload = "{\"status\":\"offline\",\"device\":\"" + clientId + "\"}";

    Serial.printf("[MQTT] Conectando ao broker %s:%d...\n", MQTT_SERVER, MQTT_PORT);

    bool conectado;
    if (strlen(MQTT_USER) > 0) {
        conectado = mqtt.connect(
            clientId.c_str(),
            MQTT_USER, MQTT_PASSWORD,
            MQTT_TOPIC_STATUS, 1, true,
            willPayload.c_str()
        );
    } else {
        conectado = mqtt.connect(
            clientId.c_str(),
            nullptr, nullptr,
            MQTT_TOPIC_STATUS, 1, true,
            willPayload.c_str()
        );
    }

    if (conectado) {
        Serial.printf("[MQTT] Conectado! Client ID: %s\n", clientId.c_str());

        // Publica birth message (retained)
        String birth = "{\"status\":\"online\",\"device\":\"" + String(DEVICE_NAME) + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
        mqtt.publish(MQTT_TOPIC_STATUS, birth.c_str(), true);

        // Subscreve tópico de alertas para receber feedback do Node-RED
        mqtt.subscribe(MQTT_TOPIC_ALERTA);
        Serial.printf("[MQTT] Subscrito em: %s\n", MQTT_TOPIC_ALERTA);

        // Subscreve tópico de status geral do sistema
        mqtt.subscribe("sistema/status");

        piscarLED(3, 150);
    } else {
        Serial.printf("[MQTT] Falha. Código de erro: %d\n", mqtt.state());
        // Códigos: -4=timeout, -3=conn_lost, -2=connect_failed, -1=disconnected,
        //           1=bad_protocol, 2=bad_client_id, 3=unavailable, 4=bad_credentials, 5=unauthorized
    }
}

// Callback: chamado quando chega mensagem em tópico subscrito
void callbackMQTT(char* topic, byte* payload, unsigned int length) {
    String topico(topic);
    String mensagem;
    for (unsigned int i = 0; i < length; i++) {
        mensagem += (char)payload[i];
    }

    Serial.printf("\n[MQTT ←] Tópico: %s\n", topico.c_str());
    Serial.printf("[MQTT ←] Payload: %s\n\n", mensagem.c_str());

    // Trata alertas de conforto vindos do Node-RED
    if (topico == MQTT_TOPIC_ALERTA) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, mensagem);
        if (!err) {
            const char* nivel = doc["nivel"];
            const char* msg   = doc["mensagem"];

            if (nivel && msg) {
                alertaAtivo  = true;
                ultimoAlerta = String("[") + nivel + "] " + msg;

                Serial.println("══════════════════════════");
                Serial.printf("  ALERTA: %s\n", ultimoAlerta.c_str());
                Serial.println("══════════════════════════");

                // Sinaliza alerta com LED
                if (strcmp(nivel, "critico") == 0) {
                    piscarLED(10, 80);  // pisca rápido para crítico
                } else {
                    piscarLED(5, 200);  // pisca devagar para atenção
                }
            }
        }
    }
}

// ============================================================
//  Leitura do sensor e publicação MQTT
// ============================================================
void lerEPublicar() {
    if (!mqtt.connected()) {
        Serial.println("[Sensor] MQTT desconectado, pulando leitura.");
        return;
    }

    // Lê sensor — DHT pode retornar NaN em leituras ruins
    float temperatura = dht.readTemperature();
    float umidade     = dht.readHumidity();

    // Valida leitura
    if (isnan(temperatura) || isnan(umidade)) {
        Serial.println("[Sensor] Leitura inválida (NaN). Verifique o sensor e o pull-up.");
        piscarLED(2, 50); // pisca rápido = erro
        return;
    }

    // Sanity check de faixa (mesma validação do PHP)
    if (temperatura < -40 || temperatura > 80 || umidade < 0 || umidade > 100) {
        Serial.printf("[Sensor] Valor fora de faixa: T=%.1f°C H=%.1f%%\n", temperatura, umidade);
        return;
    }

    // Monta JSON
    JsonDocument doc;
    doc["fonte"]       = FONTE_JSON;
    doc["temperatura"] = round(temperatura * 10) / 10.0;  // 1 casa decimal
    doc["umidade"]     = round(umidade * 10) / 10.0;

    char buffer[128];
    size_t bytes = serializeJson(doc, buffer);

    // Publica
    bool ok = mqtt.publish(MQTT_TOPIC_PUB, buffer);
    totalPublicacoes++;

    if (ok) {
        piscarLED(1, 80);
        Serial.printf("[%s] T=%.1f°C  H=%.1f%%  → %s  (#%d)\n",
            FONTE_JSON, temperatura, umidade, MQTT_TOPIC_PUB, totalPublicacoes);
        imprimirStatus();
    } else {
        Serial.println("[MQTT] Falha ao publicar. Buffer cheio ou desconectado.");
    }
}

// ============================================================
//  Utilitários
// ============================================================
void piscarLED(int vezes, int ms) {
    for (int i = 0; i < vezes; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(ms);
        digitalWrite(LED_PIN, LOW);
        delay(ms);
    }
    // Volta ao estado conectado (LED aceso = WiFi+MQTT ok)
    if (WiFi.status() == WL_CONNECTED && mqtt.connected()) {
        digitalWrite(LED_PIN, HIGH);
    }
}

void imprimirStatus() {
    unsigned long uptime = (millis() - bootTime) / 1000;
    Serial.printf("  ↳ Uptime: %lus | WiFi: %d dBm | Heap: %d bytes",
        uptime, WiFi.RSSI(), ESP.getFreeHeap());
    if (alertaAtivo) {
        Serial.printf(" | ⚠ %s", ultimoAlerta.c_str());
    }
    Serial.println();
}
