/*
 * ARQUIVO: esp32_chao_fabrica.ino
 * OBJETIVO: Etapa 01 - Ler sensores do chão de fábrica e enviar via ESPNOW.
 * SENAI CIMATEC - Práticas Integradas: Camada de Serviço
 */

// Bibliotecas necessárias
#include <WiFi.h>
#include <esp_now.h>
#include <DHT.h>

// --- 1. DEFINIÇÃO DA PINAGEM (Hardware) ---
// Atuadores
#define PIN_LED_VERDE 25    // [cite: 54, 94]
#define PIN_LED_VERMELHO 26 // [cite: 55, 96]

// Sensores
#define PIN_DHT 27          // [cite: 51, 75]
#define PIN_PIR 14          // [cite: 53, 87]
#define PIN_ULTRA_TRIG 32   // [cite: 50, 63]
#define PIN_ULTRA_ECHO 33   // [cite: 50, 63]
#define PIN_LDR 34          // [cite: 52, 80] (Pino analógico)

// Configurações do Sensor DHT
#define DHTTYPE DHT11   // Mude para DHT22 se estiver usando esse modelo
DHT dht(PIN_DHT, DHTTYPE);

// Configurações do Tanque (AJUSTE CONFORME SEU TANQUE FÍSICO)
#define DISTANCIA_TANQUE_VAZIO 20.0 // (cm) Distância do sensor até o fundo
#define DISTANCIA_TANQUE_CHEIO 5.0  // (cm) Distância do sensor até o nível máximo
#define NIVEL_ALERTA_TINTA 20.0 // (em %) Nível para acionar alerta [cite: 55, 72]

// --- 2. CONFIGURAÇÃO DO ESPNOW ---
/* * ATENÇÃO: SUBSTITUA O VALOR ABAIXO pelo Endereço MAC
 * do seu SEGUNDO ESP32 (o de monitoramento).
 * [cite: 105]
 */
uint8_t mac_monitoramento[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX};

// Estrutura dos dados que serão enviados (struct) [cite: 118]
// Esta estrutura deve ser IDÊNTICA no ESP32 de monitoramento
typedef struct struct_message {
    float nivel_tinta;    // [cite: 109]
    float temperatura;    // [cite: 111]
    float umidade;        // [cite: 112]
    int luminosidade;     // [cite: 113]
    bool presenca;        // [cite: 115]
    // O timestamp não é enviado daqui, conforme edital [cite: 117]
    // (O edital menciona, mas a etapa de BD/Monitoramento é quem deve tratar)
} struct_message;

// Cria uma variável global 'dadosSensores' baseada na estrutura
struct_message dadosSensores;

// Variável para informações do "peer" (o outro ESP32)
esp_now_peer_info_t peerInfo;

// --- 3. FUNÇÃO DE LEITURA DO SENSOR ULTRASSÔNICO ---
// Função para ler o sensor ultrassônico e converter para porcentagem
float lerNivelTanque() {
    // Gera o pulso (trigger) no sensor ultrassônico
    digitalWrite(PIN_ULTRA_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_ULTRA_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_ULTRA_TRIG, LOW);

    // Lê o tempo de retorno do pulso (echo)
    long duracao = pulseIn(PIN_ULTRA_ECHO, HIGH);

    // Calcula a distância em cm [cite: 66]
    float distancia_cm = duracao * 0.034 / 2.0;

    // Converte a distância para porcentagem de nível de tinta [cite: 67]
    float pct = 100.0 * (DISTANCIA_TANQUE_VAZIO - distancia_cm) / (DISTANCIA_TANQUE_VAZIO - DISTANCIA_TANQUE_CHEIO);
    
    // Trava o valor entre 0% e 100% para evitar leituras erradas
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    
    return pct;
}

// --- 4. FUNÇÃO DE CALLBACK (Retorno de Envio ESPNOW) ---
// Esta função é chamada automaticamente após uma tentativa de envio
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Limpa os LEDs de status de envio
  digitalWrite(PIN_LED_VERMELHO, LOW); 
  
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("-> Pacote enviado com sucesso");
    digitalWrite(PIN_LED_VERDE, HIGH); // Acende o verde (nível OK)
    delay(100); // Pisca rápido para indicar envio [cite: 133]
    // O LED verde será mantido aceso ou apagado pela lógica do tanque
  } else {
    Serial.println("-> Falha no envio ESPNOW"); // 
    digitalWrite(PIN_LED_VERMELHO, HIGH); // Acende o vermelho (nível baixo)
    delay(100); // Pisca rápido para indicar falha [cite: 135]
    // O LED vermelho será mantido aceso ou apagado pela lógica do tanque
  }
}

// --- 5. SETUP (Executado uma vez) ---
void setup() {
  Serial.begin(115200);
  Serial.println("Inicializando ESP32 Chão de Fábrica...");
  
  // Define pinos dos LEDs como SAÍDA (OUTPUT)
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_VERMELHO, OUTPUT);
  
  // Define pinos dos Sensores como ENTRADA (INPUT)
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_ULTRA_TRIG, OUTPUT); // Trigger é saída
  pinMode(PIN_ULTRA_ECHO, INPUT);  // Echo é entrada
  // O pino ADC (LDR) não precisa de pinMode

  // Inicializa o sensor DHT
  dht.begin();

  // Inicializa o Wi-Fi no modo Station (necessário para ESPNOW) [cite: 106]
  WiFi.mode(WIFI_STA); 
  
  // Inicializa o ESPNOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao inicializar ESP-NOW");
    return;
  }

  // Registra a função de callback para sabermos se o envio falhou [cite: 124]
  esp_now_register_send_cb(OnDataSent);

  // Adiciona o ESP32 de monitoramento (peer)
  memcpy(peerInfo.peer_addr, mac_monitoramento, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Falha ao adicionar peer (ESP32 de Monitoramento)");
    return;
  }

  Serial.println("ESP32 Chão de Fábrica pronto. Iniciando leituras...");
}

// --- 6. LOOP PRINCIPAL (Executado continuamente) ---
void loop() {
  // 1. Ler todos os sensores e armazenar na struct
  dadosSensores.nivel_tinta = lerNivelTanque();
  dadosSensores.temperatura = dht.readTemperature(); // [cite: 76]
  dadosSensores.umidade = dht.readHumidity(); // [cite: 76]
  dadosSensores.luminosidade = analogRead(PIN_LDR); // [cite: 82]
  dadosSensores.presenca = digitalRead(PIN_PIR); // [cite: 88]

  // Tratamento de falha de leitura do DHT
  if (isnan(dadosSensores.temperatura) || isnan(dadosSensores.umidade)) {
    Serial.println("Falha ao ler do sensor DHT!");
  }

  // 2. Lógica dos LEDs de status (baseado no nível do tanque)
  if (dadosSensores.nivel_tinta < NIVEL_ALERTA_TINTA) {
    digitalWrite(PIN_LED_VERMELHO, HIGH); // Nível baixo [cite: 55, 72, 96]
    digitalWrite(PIN_LED_VERDE, LOW);     // [cite: 73]
    Serial.println("STATUS: Alerta! Nível de tinta baixo."); // [cite: 74, 101]
  } else {
    digitalWrite(PIN_LED_VERMELHO, LOW);
    digitalWrite(PIN_LED_VERDE, HIGH);    // Nível OK [cite: 54, 94]
    Serial.println("STATUS: Operação normal"); // [cite: 101]
  }

  // 3. Exibir todas as leituras no Monitor Serial [cite: 98]
  Serial.print("Nível do tanque: "); // [cite: 69]
  Serial.print(dadosSensores.nivel_tinta, 1); // 1 casa decimal
  Serial.println("%");
  
  Serial.print("Ambiente: "); // [cite: 77]
  Serial.print(dadosSensores.temperatura, 1);
  Serial.print(" ºC | Umidade: ");
  Serial.print(dadosSensores.umidade, 1);
  Serial.println("%");

  Serial.print("Luminosidade: "); // [cite: 84]
  Serial.println(dadosSensores.luminosidade);

  Serial.print("Presença: "); // [cite: 90]
  Serial.println(dadosSensores.presenca ? "Detectada" : "Sem presença"); // [cite: 91]

  // 4. Enviar os dados via ESPNOW
  // O edital pede envio a cada 2s [cite: 121]
  esp_err_t result = esp_now_send(mac_monitoramento, (uint8_t *) &dadosSensores, sizeof(dadosSensores));

  // 5. Aguardar 2 segundos antes da próxima leitura e envio [cite: 99, 121]
  Serial.println("---------------------------------");
  delay(2000); 
}