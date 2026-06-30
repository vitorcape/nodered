#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const char* ssid        = "vitor";
const char* password    = "capellinho";
const char* mqtt_server = "7e3b86210c2447adad0a3f82c88aed28.s1.eu.hivemq.cloud";
const int   mqtt_port   = 8883;
const char* mqtt_user   = "vitor";
const char* mqtt_pass   = "Vitor123";

const int PIN_VERDE    = 4;
const int PIN_AMARELO  = 5;
const int PIN_VERMELHO = 6;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
WiFiClientSecure espClient;
PubSubClient mqtt(espClient);

String designAtual   = "alternado";
float  ultimaTemp    = 0.0;
float  ultimaUmi     = 0.0;
float  ultimaHI      = 0.0;
String ultimoNivel   = "conforto";
bool   dadosRecebidos = false;

bool     mostrandoTemp = true;
unsigned long tAlternado = 0;

// ── Helpers de tela de status ─────────────────────────────────────────────

void oledStatus(String linha1, String linha2 = "", String linha3 = "") {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Linha 1 — destaque (size 1, negrito simulado)
  display.setTextSize(1);
  display.setCursor(0, 4);
  display.print(linha1);

  if (linha2.length() > 0) {
    display.setCursor(0, 22);
    display.print(linha2);
  }

  if (linha3.length() > 0) {
    display.setCursor(0, 38);
    display.print(linha3);
  }

  // Rodapé fixo
  //display.drawLine(0, 56, 127, 56, SSD1306_WHITE);
  //display.setCursor(0, 58);
  //display.setTextSize(1);
  //display.print("ClimaMonitor v2");

  display.display();
}

void oledStatusAnimado(String titulo, String detalhe, int pontos) {
  // Exibe título + detalhe + "..." animado
  String anim = "";
  for (int i = 0; i < (pontos % 4); i++) anim += ".";

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 4);
  display.print(titulo);

  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print(detalhe);

  display.setCursor(0, 34);
  display.print("Aguardando" + anim);

  //display.drawLine(0, 56, 127, 56, SSD1306_WHITE);
  //display.setCursor(0, 58);
  //display.print("ClimaMonitor v2");

  display.display();
}

// ── Conectar WiFi com status no OLED ─────────────────────────────────────

void conectarWiFi() {
  WiFi.begin(ssid, password);
  int tentativas = 0;

  while (WiFi.status() != WL_CONNECTED) {
    oledStatusAnimado("Conectando WiFi", String(ssid), tentativas);
    delay(500);
    tentativas++;
    Serial.print(".");
  }

  Serial.println("\nWi-Fi OK: " + WiFi.localIP().toString());

  // Tela de confirmação WiFi
  oledStatus(
    "WiFi conectado!",
    String(ssid),
    WiFi.localIP().toString()
  );
  delay(1500);
}

// ── Reconexão MQTT com status no OLED ────────────────────────────────────

void reconnect() {
  int tentativas = 0;

  while (!mqtt.connected()) {
    // Verifica WiFi primeiro
    if (WiFi.status() != WL_CONNECTED) {
      oledStatus("WiFi perdido!", "Reconectando...");
      WiFi.reconnect();
      int w = 0;
      while (WiFi.status() != WL_CONNECTED) {
        delay(500); w++;
        oledStatusAnimado("Reconectando WiFi", String(ssid), w);
      }
      oledStatus("WiFi reconectado!", String(ssid), WiFi.localIP().toString());
      delay(1000);
    }

    // Tenta MQTT
    oledStatusAnimado(
      "Conectando MQTT",
      "HiveMQ Cloud",
      tentativas
    );

    Serial.print("Conectando HiveMQ...");

    if (mqtt.connect("esp32_leds", mqtt_user, mqtt_pass)) {
      Serial.println(" OK");
      mqtt.subscribe("leds/comando");
      mqtt.subscribe("oled/design");
      mqtt.subscribe("oled/dados");
      mqtt.publish("leds/status", "{\"online\":true}", true);

      // Tela de sucesso MQTT
      oledStatus(
        "MQTT conectado!",
        "HiveMQ Cloud",
        "Aguardando dados..."
      );
      delay(1500);

    } else {
      Serial.print(" falhou rc="); Serial.println(mqtt.state());
      tentativas++;

      String erro = "rc=" + String(mqtt.state());
      oledStatus("MQTT falhou", erro, "Tentando novamente...");
      delay(3000);
    }
  }
}

// ── Designs do OLED ───────────────────────────────────────────────────────

void oledAlternado() {
  if (millis() - tAlternado < 2000) return;
  tAlternado = millis();
  mostrandoTemp = !mostrandoTemp;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  String valor = mostrandoTemp
    ? String(ultimaTemp, 1)
    : String(ultimaUmi, 0) + "%";

  display.setTextSize(3);
  int16_t bx, by; uint16_t bw, bh;
  display.getTextBounds(valor.c_str(), 0, 0, &bx, &by, &bw, &bh);
  display.setCursor((128 - bw) / 2, 8);
  display.print(valor);

  display.setTextSize(2);
  display.getTextBounds("XX", 0, 0, &bx, &by, &bw, &bh);
  display.setCursor((128 - bw) / 2, 44);
  if (mostrandoTemp) {
    display.print((char)247); display.print("C");
  } else {
    display.print("umid");
  }

  display.fillCircle(54, 61, 3, mostrandoTemp  ? SSD1306_WHITE : SSD1306_BLACK);
  display.drawCircle(54, 61, 3, SSD1306_WHITE);
  display.fillCircle(74, 61, 3, !mostrandoTemp ? SSD1306_WHITE : SSD1306_BLACK);
  display.drawCircle(74, 61, 3, SSD1306_WHITE);

  display.display();
}

void oledConforto() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(3);
  display.setCursor(0, 2);
  display.print(ultimaTemp, 1);
  display.print((char)247);
  display.print("C");

  display.setTextSize(2);
  display.setCursor(0, 30);
  display.print("Umid:");
  display.print(ultimaUmi, 0);
  display.print("%");

  display.drawLine(0, 48, 127, 48, SSD1306_WHITE);

  String nivel = "CONFORTO";
  if      (ultimaHI >= 54) nivel = "PERIGO EXT";
  else if (ultimaHI >= 41) nivel = "PERIGO";
  else if (ultimaHI >= 32) nivel = "CAUT.EXT.";
  else if (ultimaHI >= 27) nivel = "CAUTELA";

  display.setTextSize(1);
  int16_t bx, by; uint16_t bw, bh;
  display.getTextBounds(nivel.c_str(), 0, 0, &bx, &by, &bw, &bh);
  display.setCursor((128 - bw) / 2, 55);
  display.print(nivel);

  display.display();
}

void oledBarra() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 4);
  display.print(ultimaTemp, 1);
  display.print((char)247);
  display.print("C");

  display.setCursor(0, 26);
  display.print(ultimaUmi, 0);
  display.print("% umid");

  int barraW = (int)((ultimaUmi / 100.0) * 116);
  display.drawRect(4, 48, 120, 10, SSD1306_WHITE);
  display.fillRect(4, 48, barraW, 10, SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(4,  59); display.print("0");
  display.setCursor(109, 59); display.print("100");

  display.display();
}

void atualizarOled() {
  if (!dadosRecebidos) return; // não desenha dados se ainda não recebeu nada
  if      (designAtual == "alternado") oledAlternado();
  else if (designAtual == "conforto")  { oledConforto(); delay(2000); }
  else if (designAtual == "barra")     { oledBarra();    delay(2000); }
}

// ── Callback MQTT ─────────────────────────────────────────────────────────

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  StaticJsonDocument<256> doc;
  deserializeJson(doc, msg);
  String t = String(topic);

  if (t == "leds/comando") {
    String led = doc["led"] | "off";
    digitalWrite(PIN_VERDE,    led == "verde"    ? HIGH : LOW);
    digitalWrite(PIN_AMARELO,  led == "amarelo"  ? HIGH : LOW);
    digitalWrite(PIN_VERMELHO, led == "vermelho" ? HIGH : LOW);
  }

  if (t == "oled/design") {
    designAtual = String(doc["design"] | "alternado");
    tAlternado  = 0;
    if (dadosRecebidos && designAtual != "alternado") atualizarOled();
  }

  if (t == "oled/dados") {
    ultimaTemp    = doc["temperatura"] | 0.0f;
    ultimaUmi     = doc["umidade"]     | 0.0f;
    ultimaHI      = doc["heat_index"]  | 0.0f;
    ultimoNivel   = String(doc["nivel"] | "conforto");
    dadosRecebidos = true; // libera os designs
    tAlternado    = 0;
    if (designAtual != "alternado") atualizarOled();
  }
}

// ── Setup / Loop ──────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  pinMode(PIN_VERDE,    OUTPUT);
  pinMode(PIN_AMARELO,  OUTPUT);
  pinMode(PIN_VERMELHO, OUTPUT);

  // Inicializa OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED nao encontrado");
    while (true); // trava se não achar o OLED
  }

  // Tela de boot
  oledStatus("Vitor Capelli", "ESP32 #3", "Iniciando...");
  delay(1000);

  // Conecta WiFi com feedback no OLED
  conectarWiFi();

  // Configura MQTT
  espClient.setInsecure();
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(callback);

  // Conecta MQTT com feedback no OLED
  reconnect();
}

void loop() {
  if (!mqtt.connected()) reconnect();
  mqtt.loop();
  atualizarOled();
}