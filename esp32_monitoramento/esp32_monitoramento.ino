/*
 * ARQUIVO: esp32_monitoramento.ino
 * OBJETIVO: Etapa 02 - Receber dados via ESPNOW e exibir na Matriz de LEDs.
 * SENAI CIMATEC - Práticas Integradas: Camada de Serviço
 */

// Bibliotecas de Conectividade
#include <WiFi.h>
#include <esp_now.h>
// #include <HTTPClient.h> // <-- Usaremos na Etapa 03

// Bibliotecas da Matriz de LEDs
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// --- 1. DEFINIÇÃO DA PINAGEM (Hardware) ---
[cite_start]// LEDs de Status de Comunicação [cite: 151, 152]
#define PIN_LED_VERDE_STATUS 25 // Aceso = Comunicação OK
#define PIN_LED_VERMELHO_STATUS 26 // Aceso = Falha de Comunicação

// Pinos da Matriz de LEDs (Hardware SPI)
#define HARDWARE_TYPE MD_MAX72xx::FC16_HW // Tipo de módulo
#define MAX_DEVICES 4 // Número de matrizes 8x8 no seu módulo
#define CLK_PIN 18    // Clock
#define DATA_PIN 23   // Data (MOSI)
#define CS_PIN 5      // Chip Select

// Inicializa o controle da Matriz de LEDs
MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// --- 2. CONFIGURAÇÃO DO ESPNOW ---
// Estrutura dos dados (DEVE SER IDÊNTICA à do ESP32 Chão de Fábrica)
typedef struct struct_message {
    float nivel_tinta;
    float temperatura;
    float umidade;
    int luminosidade;
    bool presenca;
} struct_message;

// Variável global para armazenar os dados mais recentes
struct_message dadosRecebidos;

// --- 3. CONFIGURAÇÃO WI-FI E BACKEND (Para Etapa 03) ---
/*
// --- INÍCIO ETAPA 03 (Deixe comentado por enquanto) ---
const char* ssid = "NOME_DA_REDE_WIFI";
const char* password = "SENHA_DA_REDE_WIFI";
String api_url = "http://SEU_IP_PYTHON/dados";
// --- FIM ETAPA 03 ---
*/

// --- 4. VARIÁVEIS DE CONTROLE DE ESTADO ---
unsigned long lastRxTime = 0; // Última vez que recebemos dados
unsigned long lastDisplayChange = 0; // Última vez que mudamos a tela
const long timeoutInterval = 5000; [cite_start]// 5 segundos para timeout [cite: 176, 197]
const long displayInterval = 2000; [cite_start]// 2 segundos por tela [cite: 150, 163, 170]
int displayState = 0; // Qual tela mostrar (0=Nível, 1=Temp, etc.)
bool comunicacaoOK = false; // Flag de status da comunicação

// --- 5. FUNÇÃO DE CALLBACK (Recepção de Dados ESPNOW) ---
[cite_start]// Esta função é chamada automaticamente quando um pacote ESPNOW chega [cite: 189]
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  [cite_start]// Copia os dados recebidos para a nossa variável global [cite: 194]
  memcpy(&dadosRecebidos, incomingData, sizeof(dadosRecebidos));
  
  comunicacaoOK = true; // Marca a comunicação como OK
  lastRxTime = millis(); [cite_start]// Reseta o timer do timeout [cite: 195]

  Serial.println("Pacote ESPNOW Recebido:"); [cite_start]// [cite: 206]
  Serial.printf("  Nível: %.1f%%, Temp: %.1fC, Umid: %.1f%%, Luz: %d, Pres: %d\n",
                dadosRecebidos.nivel_tinta, dadosRecebidos.temperatura,
                dadosRecebidos.umidade, dadosRecebidos.luminosidade,
                dadosRecebidos.presenca);
  
  /*
  // --- INÍCIO ETAPA 03 (Deixe comentado por enquanto) ---
  [cite_start]// Quando o Wi-Fi estiver conectado, envia os dados para o Backend [cite: 231]
  if(WiFi.status() == WL_CONNECTED) {
    enviarDadosParaBackend();
  }
  // --- FIM ETAPA 03 ---
  */
}

/*
// --- 6. FUNÇÃO DE ENVIO PARA O BACKEND (Etapa 03 - Deixe comentado) ---
void enviarDadosParaBackend() {
  HTTPClient http;
  http.begin(api_url);
  http.addHeader("Content-Type", "application/json");

  // Cria o JSON
  char jsonBuffer[256];
  sprintf(jsonBuffer, 
    "{\"nivel\":%.1f, \"temp\":%.1f, \"umid\":%.1f, \"luz\":%d, \"pres\":%d}",
    dadosRecebidos.nivel_tinta, dadosRecebidos.temperatura,
    dadosRecebidos.umidade, dadosRecebidos.luminosidade,
    dadosRecebidos.presenca
  );

  Serial.print("Enviando dados para Backend: ");
  Serial.println(jsonBuffer);
  
  int httpResponseCode = http.POST(jsonBuffer);
  
  if (httpResponseCode > 0) {
    Serial.printf("Backend respondeu com código: %d\n", httpResponseCode);
  } else {
    Serial.printf("Falha ao enviar dados para Backend, erro: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  http.end();
}
*/

// --- 7. FUNÇÕES DA MATRIZ DE LEDS ---
void setupMatriz() {
  P.begin();
  P.setIntensity(2); // Intensidade (0-15)
  P.displayClear();
  P.displayText("Iniciando...", PA_CENTER, P.getSpeed(), 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
}

// Função principal que atualiza o que é mostrado
void atualizarDisplay() {
  // P.displayAnimate() deve ser chamado o mais rápido possível no loop
  if (!P.displayAnimate()) return; // Se não terminou a animação anterior, sai

  // Se a comunicação caiu (timeout)
  if (!comunicacaoOK) {
    P.displayText("SEM DADOS", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT); [cite_start]// [cite: 199]
    return;
  }

  // Se a comunicação está OK, verifica se deu o tempo de trocar de tela
  [cite_start]if (millis() - lastDisplayChange >= displayInterval) { // [cite: 170]
    lastDisplayChange = millis();
    displayState = (displayState + 1) % 5; // Avança para próxima tela (0 a 4)

    char buffer[20]; // Buffer para formatar o texto

    switch (displayState) {
      [cite_start]case 0: // Nível do Tanque [cite: 165]
        sprintf(buffer, "NVL %d%%", (int)dadosRecebidos.nivel_tinta);
        break;
      [cite_start]case 1: // Temperatura [cite: 166]
        sprintf(buffer, "TMP %dC", (int)dadosRecebidos.temperatura);
        break;
      [cite_start]case 2: // Umidade [cite: 167]
        sprintf(buffer, "UMD %d%%", (int)dadosRecebidos.umidade);
        break;
      [cite_start]case 3: // Luminosidade [cite: 168]
        sprintf(buffer, "LUX %d", dadosRecebidos.luminosidade);
        break;
      [cite_start]case 4: // Presença [cite: 169]
        sprintf(buffer, "PRS %s", dadosRecebidos.presenca ? "ON" : "OFF");
        break;
    }
    
    [cite_start]// Envia o texto formatado para a matriz [cite: 172]
    P.displayText(buffer, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  }
}

// --- 8. FUNÇÃO DE SETUP ---
void setup() {
  Serial.begin(115200);
  Serial.println("Inicializando ESP32 de Monitoramento...");

  // Configura os LEDs de Status
  pinMode(PIN_LED_VERDE_STATUS, OUTPUT);
  pinMode(PIN_LED_VERMELHO_STATUS, OUTPUT);
  digitalWrite(PIN_LED_VERMELHO_STATUS, HIGH); [cite_start]// Começa em falha [cite: 152]
  digitalWrite(PIN_LED_VERDE_STATUS, LOW);
  
  // Inicializa a Matriz de LEDs
  setupMatriz();

  [cite_start]// Coloca o ESP32 em modo Wi-Fi Station (necessário para ESPNOW e Wi-Fi) [cite: 188]
  WiFi.mode(WIFI_STA);
  
  /*
  // --- INÍCIO ETAPA 03 (Deixe comentado por enquanto) ---
  // Conecta ao Wi-Fi para o Backend
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  // --- FIM ETAPA 03 ---
  */

  // Mostra o MAC Address deste ESP32 no Serial
  // Você DEVE usar este MAC no código do "ESP32 Chão de Fábrica"
  Serial.print("MAC Address deste ESP32: ");
  Serial.println(WiFi.macAddress());

  // Inicializa o ESPNOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao inicializar ESP-NOW");
    P.displayText("Falha E-NOW", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    return;
  }

  // Registra a função de callback para QUANDO RECEBER dados
  [cite_start]esp_now_register_recv_cb(OnDataRecv); [cite: 189]
  
  Serial.println("ESP-NOW inicializado. Aguardando pacotes...");
}

// --- 9. FUNÇÃO DE LOOP ---
void loop() {
  
  // 1. Verifica se deu timeout na comunicação
  if (comunicacaoOK && (millis() - lastRxTime > timeoutInterval)) {
    comunicacaoOK = false; [cite_start]// Marca que a comunicação falhou [cite: 197]
    Serial.println("Timeout de comunicação ESPNOW!"); [cite_start]// [cite: 182]
  }

  // 2. Atualiza os LEDs de status
  if (comunicacaoOK) {
    digitalWrite(PIN_LED_VERDE_STATUS, HIGH); [cite_start]// [cite: 175]
    digitalWrite(PIN_LED_VERMELHO_STATUS, LOW);
  } else {
    digitalWrite(PIN_LED_VERDE_STATUS, LOW);
    digitalWrite(PIN_LED_VERMELHO_STATUS, HIGH); [cite_start]// [cite: 176]
  }

  // 3. Atualiza o que está sendo mostrado na Matriz de LEDs
  atualizarDisplay();
}