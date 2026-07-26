import requests
import os
import time
from datetime import datetime

def sincronizar():
    print("--- Esperando conexion a Internet en la USCO... ---")
    intentos = 0
    while intentos < 12:  # Reintenta por 60 segundos (1 minuto)
        try:
            # Pedimos la cabecera a Google
            response = requests.get("https://www.google.com", timeout=5)
            
            if response.status_code == 200:
                date_str = response.headers.get('Date')
                # Convertimos GMT a objeto de tiempo
                gmt_time = datetime.strptime(date_str, '%a, %d %b %Y %H:%M:%S %Z')
                fecha_utc = gmt_time.strftime('%Y-%m-%d %H:%M:%S')
                
                print(f"Internet detectado. Hora UTC: {fecha_utc}")
                # Seteamos la hora (sin sudo porque el contenedor es privileged)
                os.system(f'date -s "{fecha_utc} UTC"')
                print("--- Sincronizacion exitosa ---")
                return True 
        except Exception as e:
            intentos += 1
            print(f"Intento {intentos}: WiFi no listo... esperando 5s.")
            time.sleep(5)
            
    print("Fallo critico: No se pudo sincronizar la hora tras 1 minuto.")
    return False

if __name__ == "__main__":
    sincronizar()
