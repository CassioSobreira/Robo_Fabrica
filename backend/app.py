import os
from flask import Flask, request, jsonify
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS
from dotenv import load_dotenv

# Carrega as variáveis do arquivo .env (que está na mesma pasta)
load_dotenv()

# --- Configuração do InfluxDB (lendo do .env) ---
INFLUX_URL = os.getenv("INFLUXDB_URL", "http://localhost:8086")
INFLUX_TOKEN = os.getenv("INFLUXDB_TOKEN")
INFLUX_ORG = os.getenv("INFLUXDB_ORG")
INFLUX_BUCKET = os.getenv("INFLUXDB_BUCKET")

# --- Inicialização do Cliente InfluxDB ---

# Verifica se as variáveis foram carregadas do .env
if not all([INFLUX_TOKEN, INFLUX_ORG, INFLUX_BUCKET]):
    print("="*50)
    print("ERRO: Variáveis de ambiente InfluxDB não definidas.")
    print("Verifique se o arquivo '.env' existe na pasta 'backend/'")
    print("e se ele contém INFLUXDB_TOKEN, INFLUXDB_ORG, e INFLUXDB_BUCKET.")
    print("="*50)
    exit(1) # Para a execução se o .env estiver errado

# Inicializa o cliente
try:
    client = InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG)
    # Verifica se a conexão é válida (opcional, mas bom)
    health = client.health()
    if health.status != "pass":
       raise Exception("Falha na saúde do InfluxDB")
    print("Conexão com InfluxDB estabelecida com sucesso.")
except Exception as e:
    print(f"Erro ao conectar ao InfluxDB em {INFLUX_URL}: {e}")
    print("Verifique se o InfluxDB está rodando (docker compose up -d)")
    exit(1)

# Cria a API de escrita de forma síncrona (confirma cada escrita)
write_api = client.write_api(write_options=SYNCHRONOUS)

# --- Inicialização do Flask (nossa API) ---
app = Flask(__name__)

# --- Endpoint da API (/dados) ---
# Este é o endereço que o ESP32 de Monitoramento vai chamar
@app.route('/dados', methods=['POST'])
def receber_dados():
    """
    Endpoint para receber os dados dos sensores (enviados pelo ESP32).
    Espera um JSON no formato:
    {
        "nivel": 75.1,
        "temp": 24.5,
        "umid": 55.0,
        "luz": 680,
        "pres": 1 (ou true)
    }
    """
    try:
        # Pega o JSON enviado pelo ESP32
        data = request.get_json()

        if not data:
            return jsonify({"erro": "Nenhum dado JSON recebido"}), 400

        # Validação simples dos campos (conforme edital)
        required_fields = ['nivel', 'temp', 'umid', 'luz', 'pres']
        if not all(field in data for field in required_fields):
            return jsonify({"erro": "Dados incompletos. Faltando campos."}), 400

        print(f"Dados Recebidos do ESP32: {data}")

        # --- Escrita no InfluxDB ---
        
        # "sensores_pintura" é o nome da "tabela" (measurement) no InfluxDB
        point = Point("sensores_pintura") \
            .tag("local", "chao_fabrica") \
            .field("nivel_tinta", float(data['nivel'])) \
            .field("temperatura", float(data['temp'])) \
            .field("umidade", float(data['umid'])) \
            .field("luminosidade", int(data['luz'])) \
            .field("presenca", bool(data['pres']))

        # Escreve o ponto no bucket
        write_api.write(bucket=INFLUX_BUCKET, org=INFLUX_ORG, record=point)
        
        print(f"-> Dados registrados no InfluxDB com sucesso!")

        # Responde ao ESP32 que deu tudo certo
        return jsonify({"sucesso": "Dados registrados"}), 201

    except Exception as e:
        print(f"Erro ao processar requisição: {e}")
        return jsonify({"erro": str(e)}), 500

# --- Execução do Servidor ---
if __name__ == '__main__':
    print("Iniciando servidor Flask...")
    # 'host="0.0.0.0"' é CRUCIAL. 
    # Isso faz o servidor escutar em todas as interfaces de rede (Wi-Fi, Cabo).
    # Se usar 'localhost' ou '127.0.0.1', o ESP32 (outro dispositivo) 
    # NUNCA conseguirá se conectar.
    app.run(host='0.0.0.0', port=5000, debug=True)