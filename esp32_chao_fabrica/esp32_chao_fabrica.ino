/*
 * ARQUIVO: esp32_chao_fabrica.ino
 * OBJETIVO: Etapa 01 - Ler sensores do chão de fábrica e enviar via ESPNOW.
 * SENAI CIMATEC - Práticas Integradas: Camada de Serviço
 *
 * VERSÃO COM CORREÇÃO DE CALLBACK DE ENVIO (OnDataSent) para S3
 */

// Bibliotecas necessárias
#include <WiFi.h>
#include <esp_now.h>
#include <DHT.h>
#include <esp_wifi.h> // <-- 1. ADICIONADO PARA CORRIGIR O CALLBACK

// --- 1. DEFINIÇÃO DA PINAGEM (Hardware S3 - Placa 1) ---
#define PIN_LED_VERDE 1     //
#define PIN_LED_VERMELHO 2  //
#define PIN_DHT 8           //
#define PIN_PIR 4           //
#define PIN_ULTRA_TRIG 5    //
#define PIN_ULTRA_ECHO 6    //
#define PIN_LDR 7           // (Pino analógico)

// Configurações do Sensor DHT
#define DHTTYPE DHT11   // Mude para DHT22 se estiver usando esse modelo
DHT dht(PIN_DHT, DHTTYPE);

// Configurações do Tanque
#define DISTANCIA_TANQUE_VAZIO 20.0 
#define DISTANCIA_TANQUE_CHEIO 5.0  
#define NIVEL_ALERTA_TINTA 20.0 

// --- 2. CONFIGURAÇÃO DO ESPNOW ---
// MAC Address do ESP32 de Monitoramento (Placa 2)
uint8_t mac_monitoramento[] = {0x30, 0xED, 0xA0, 0xB7, 0x24, 0x08}; //

// Estrutura dos dados que serão enviados
typedef struct struct_message {
    float nivel_tinta;    
    float temperatura;    
    float umidade;        
    int luminosidade;     
    bool presenca;        
} struct_message;

struct_message dadosSensores;
esp_now_peer_info_t peerInfo;

// --- 3. FUNÇÃO DE LEITURA DO SENSOR ULTRASSÔNICO ---
float lerNivelTanque() {
    digitalWrite(PIN_ULTRA_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_ULTRA_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_ULTRA_TRIG, LOW);
    long duracao = pulseIn(PIN_ULTRA_ECHO, HIGH);
    float distancia_cm = duracao * 0.034 / 2.0;
    float pct = 100.0 * (DISTANCIA_TANQUE_VAZIO - distancia_cm) / (DISTANCIA_TANQUE_VAZIO - DISTANCIA_TANQUE_CHEIO);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

// --- 4. FUNÇÃO DE CALLBACK (Retorno de Envio ESPNOW) ---
// **** 2. CORREÇÃO: Assinatura da função atualizada para S3/Core 3.0+ ****
// Mudamos de (const uint8_t *mac_addr, ...) para (const wifi_tx_info_t *info, ...)
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  digitalWrite(PIN_LED_VERMELHO, LOW); 
  
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("-> Pacote enviado com sucesso (Canal 6)");
    digitalWrite(PIN_LED_VERDE, HIGH); 
    delay(100); 
  } else {
    Serial.println("-> Falha no envio ESPNOW"); 
    digitalWrite(PIN_LED_VERMELHO, HIGH); 
    delay(100); 
  }
}

// --- 5. SETUP (Executado uma vez) ---
void setup() {
  Serial.begin(115200);
  Serial.println("Inicializando ESP32 Chão de Fábrica...");
  
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_VERMELHO, OUTPUT);
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_ULTRA_TRIG, OUTPUT); 
  pinMode(PIN_ULTRA_ECHO, INPUT);  
  
  dht.begin();
  WiFi.mode(WIFI_STA); 
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao inicializar ESP-NOW");
    return;
  }

  // Registra a função de callback de envio (agora correta)
  esp_now_register_send_cb(OnDataSent);

  // Adiciona o ESP32 de monitoramento (peer)
  memcpy(peerInfo.peer_addr, mac_monitoramento, 6);
  // Força o Canal 6 para evitar interferência
  peerInfo.channel = 6; 
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Falha ao adicionar peer (ESP32 de Monitoramento)");
    return;
  }

  Serial.println("ESP32 Chão de Fábrica pronto. Enviando no Canal 6...");
}

// --- 6. LOOP PRINCIPAL (Executado continuamente) ---
void loop() {
  dadosSensores.nivel_tinta = lerNivelTanque();
  dadosSensores.temperatura = dht.readTemperature(); 
  dadosSensores.umidade = dht.readHumidity(); 
  dadosSensores.luminosidade = analogRead(PIN_LDR); 
  dadosSensores.presenca = digitalRead(PIN_PIR); 

  if (isnan(dadosSensores.temperatura) || isnan(dadosSensores.umidade)) {
    Serial.println("Falha ao ler do sensor DHT!");
  }

  if (dadosSensores.nivel_tinta < NIVEL_ALERTA_TINTA) {
    digitalWrite(PIN_LED_VERMELHO, HIGH); 
    digitalWrite(PIN_LED_VERDE, LOW);     
    Serial.println("STATUS: Alerta! Nível de tinta baixo."); 
  } else {
    digitalWrite(PIN_LED_VERMELHO, LOW);
    digitalWrite(PIN_LED_VERDE, HIGH);    
    Serial.println("STATUS: Operação normal"); 
  }

  Serial.print("Nível do tanque: "); 
  Serial.print(dadosSensores.nivel_tinta, 1); 
  Serial.println("%");
  Serial.print("Ambiente: "); 
  Serial.print(dadosSensores.temperatura, 1);
  Serial.print(" ºC | Umidade: ");
  Serial.print(dadosSensores.umidade, 1);
  Serial.println("%");
  Serial.print("Luminosidade: "); 
  Serial.println(dadosSensores.luminosidade);
  Serial.print("Presença: "); 
  Serial.println(dadosSensores.presenca ? "Detectada" : "Sem presença"); 

  esp_err_t result = esp_now_send(mac_monitoramento, (uint8_t *) &dadosSensores, sizeof(dadosSensores));

  Serial.println("---------------------------------");
  delay(2000); 
}