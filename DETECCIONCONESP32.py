from ultralytics import YOLO
import cv2
import time
from collections import deque
import requests
from datetime import datetime

# --- Cargar modelo ---
model = YOLO("best.pt")

# --- Clases ---
class_names = model.names  # {0: 'chihuahua', 1: 'pomeranía', 2: 'pug'}
print("Clases del modelo:", class_names)

# --- Inicializar cámara ---
# NOTA: Si usas una cámara externa, el índice puede ser 0 o 1
cap = cv2.VideoCapture(1)
if not cap.isOpened():
    print("❌ No se pudo abrir la cámara.")
    exit()

# --- Parámetros de Detección ---
CONF_THRESHOLD = 0.80
FRAMES_REQUIRED_BY_CLASS = {
    "chihuahua": 8,
    "pomeranía": 8,
    "pug": 8
}

history = deque(maxlen=20)
current_label = None
last_detection_time = 0
detections_count = {name: 0 for name in class_names.values()}

# --- IP de tu ESP32 ---
ESP32_IP = "192.168.4.1"
SETTIME_ENDPOINT = f"http://{ESP32_IP}/settime"

# --- Variables de Sincronización (NUEVAS) ---
last_sync_time = time.time()
SYNC_INTERVAL = 1.0  # Sincronizar cada 1.0 segundo


# ===================================================================
# 💧 FUNCIÓN: Envío de HORA para SINCRONIZACIÓN (SIN HILOS)
# ===================================================================

def enviar_hora_sincronizacion_sincrona():
    """Obtiene la hora actual y la envía al ESP32 (rápido y no bloqueante)."""
    global last_sync_time

    fecha_hora_actual = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    payload = {'fecha': fecha_hora_actual}

    try:
        # Usar timeout muy corto (0.1s) para que no bloquee el bucle principal
        requests.get(SETTIME_ENDPOINT, params=payload, timeout=0.1)

        # 🚨 DEBUG: Imprime solo si es exitoso
        # print(f"[SINC OK] Hora enviada: {fecha_hora_actual}")

    except requests.exceptions.RequestException as e:
        # Si la conexión falla, solo lo notamos en el log para no saturar
        # print(f"[SINC ERROR] Fallo al sincronizar: {e}")
        pass

    except Exception:
        pass

    # Actualizar el tiempo de la última sincronización
    last_sync_time = time.time()


# ===================================================================
# 🚀 PROGRAMA PRINCIPAL
# ===================================================================

print("✅ Detección y Sincronización iniciadas. Presiona 'q' para salir.")
print(f"✅ Sincronización de hora en bucle principal. IP de ESP32: {ESP32_IP}")

while True:

    # -------------------------------------------------------------
    # ⌚ SINCRONIZACIÓN DE HORA (NUEVA LÓGICA SIN HILOS)
    # -------------------------------------------------------------
    # Se ejecuta al menos una vez por segundo.
    if time.time() - last_sync_time >= SYNC_INTERVAL:
        enviar_hora_sincronizacion_sincrona()
    # -------------------------------------------------------------

    ret, frame = cap.read()
    if not ret:
        break

    results = model(frame, stream=True)

    annotated_frame = frame.copy()

    for r in results:
        boxes = r.boxes
        annotated_frame = r.plot()  # Vuelve a pintar el frame si hay detecciones

        if len(boxes) > 0:
            confs = boxes.conf.cpu().numpy()
            cls_ids = boxes.cls.cpu().numpy().astype(int)
            best_idx = confs.argmax()
            best_conf = confs[best_idx]
            best_class = cls_ids[best_idx]
            best_label = class_names[best_class]

            if best_conf > CONF_THRESHOLD:
                history.append(best_label)
            else:
                history.clear()

            frames_required = FRAMES_REQUIRED_BY_CLASS.get(best_label, 10)
            if len(history) >= frames_required and all(h == history[0] for h in history):
                new_label = history[0]
                if new_label != current_label:
                    current_label = new_label
                    detections_count[current_label] += 1
                    last_detection_time = time.time()
                    print(f"✅ {current_label} confirmado ({detections_count[current_label]} veces)")

                    # 🚀 LÓGICA DE ENVÍO DE COMIDA
                    try:
                        fecha_hora = datetime.now().strftime("%Y-%m-%d %H:%M:%S")  # 🕒
                        params = {"nombre": current_label, "fecha": fecha_hora}
                        # Endpoint /raza - Aumentamos timeout por ser una operación crítica
                        r = requests.get(f"http://{ESP32_IP}/raza", params=params, timeout=3)
                        print(f"📡 Enviado al ESP32 (COMIDA): {current_label} a las {fecha_hora}")
                        print("🔹 Respuesta del ESP32:", r.text)
                    except Exception as e:
                        print("⚠️ Error al enviar al ESP32 (COMIDA):", e)

            # Mostrar info en pantalla
            if current_label:
                cv2.putText(annotated_frame,
                            f"Raza detectada: {current_label}",
                            (10, 40), cv2.FONT_HERSHEY_SIMPLEX,
                            1, (0, 255, 0), 2)
            cv2.putText(annotated_frame,
                        f"Conf: {best_conf:.2f}",
                        (10, 80), cv2.FONT_HERSHEY_SIMPLEX,
                        0.8, (255, 255, 255), 2)
        else:
            history.clear()
            current_label = None

    cv2.imshow("Detección de Razas - YOLOv8", annotated_frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()

# --- Resumen final ---
print("\n📊 RESUMEN FINAL DE DETECCIONES")
for raza, cantidad in detections_count.items():
    print(f"  - {raza.capitalize()}: {cantidad} detecciones confirmadas")

print("\n👋 Programa terminado.")