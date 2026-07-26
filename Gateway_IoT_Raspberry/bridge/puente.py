import serial
import paho.mqtt.client as mqtt
import json
import time
import os

# Configuración de Hardware y Red
PORT = '/dev/ttyS0' # UART de la Raspberry Pi 3 B+
BAUD = 9600
MQTT_BROKER = "mosquitto"
TOPIC = "casa/consumo/monitoreo"
ENERGY_FILE = "last_energy.txt" # Para que no se pierdan los kWh acumulados

# Estructura de tus 10 datos (Fase 1 y Fase 2)
KEYS = [
    "f1_vrms", "f1_irms", "f1_p", "f1_q", "f1_s",
    "f2_vrms", "f2_irms", "f2_p", "f2_q", "f2_s"
]

# Cargar último valor de energía si existe (Persistencia)
acumulado_kWh = 0.0
if os.path.exists(ENERGY_FILE):
    try:
        with open(ENERGY_FILE, "r") as f:
            acumulado_kWh = float(f.read())
    except: pass

client = mqtt.Client()
while True:
    try:
        client.connect(MQTT_BROKER, 1883, 60)
        break
    except:
        time.sleep(2)

last_time = time.time()
print("Sistema Real Activo: Procesando 10 variables + Suma P + kWh")

with serial.Serial(PORT, BAUD, timeout=1) as ser:
    while True:
        if ser.in_waiting > 0:
            linea = ser.readline().decode('utf-8').strip()
            datos_lista = linea.split(',')
            
            if len(datos_lista) == 10:
                current_time = time.time()
                dt = current_time - last_time # Delta de tiempo en segundos
                
                try:
                    # 1. Crear el diccionario base
                    payload = {KEYS[i]: float(datos_lista[i]) for i in range(10)}
                    
                    # 2. CALCULAR POTENCIA TOTAL (Suma de P de ambas fases)
                    p_total_w = payload["f1_p"] + payload["f2_p"]
                    payload["p_total_w"] = round(p_total_w, 2)
                    
                    # 3. INTEGRACIÓN DE ENERGÍA (kWh)
                    # Fórmula: (Potencia en W * tiempo en seg) / 3,600,000
                    delta_kWh = (p_total_w * dt) / 3600000
                    acumulado_kWh += delta_kWh
                    payload["consumo_total_kWh"] = round(acumulado_kWh, 6)
                    
                    # 4. Guardar en archivo para persistencia (en cada ciclo)
                    with open(ENERGY_FILE, "w") as f:
                        f.write(str(acumulado_kWh))
                    
                    # 5. Publicar a la base de datos (vía MQTT)
                    client.publish(TOPIC, json.dumps(payload))
                    print(f"Enviado -> P_Total: {p_total_w}W | kWh: {payload['consumo_total_kWh']}")
                    
                    last_time = current_time
                except ValueError:
                    print("Error: Se recibieron datos no numéricos")
