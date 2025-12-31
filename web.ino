#include <Wire.h>
#include <Adafruit_INA219.h>
#include <MPU6050.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>
#include "WiFi.h"
#include <WebServer.h>
#include "esp_task_wdt.h"
// ===================== OBJETOS =====================
MPU6050 mpu;
Adafruit_INA219 ina219;


#define LED_NORMAL 26   // escoge un GPIO libre
#define LED_FALLO  25


// DS18B20
#define TEMP_PIN 32
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);


#define V_MIN 1.0
#define I_MIN 20.0

bool mpu_ok = true;
bool ina_ok = true;

// ======= WEB SERVER =======
WebServer server(80);

// ======= VARIABLES PARA LA WEB =======
float voltaje=0, corriente=0, potencia=0, tempC=0, accMag=0;
int estado_motor=0, clase=0;

// ======= FUNCIONES SEGURAS =======
bool leerMPU(int16_t &ax, int16_t &ay, int16_t &az,
             int16_t &gx, int16_t &gy, int16_t &gz) {

  mpu.getAcceleration(&ax, &ay, &az);
  mpu.getRotation(&gx, &gy, &gz);
  return true;
}

bool leerINA(float &v, float &i) {
  v = ina219.getBusVoltage_V();
  i = ina219.getCurrent_mA();
  return !(isnan(v) || isnan(i));
}

// ======= PREDICT =======
int predict(float x[], float accMag) {
  if (x[10] < 1.0) return 4;          // apagado total
  if (x[4] == 0 && abs(x[3]) < 5.0) return 2;
  if (x[3] > 400.0) return 1;
  if (x[4] == 1 &&
      x[3] >= 20.0 && x[3] <= 120.0 &&
      x[10] >= 4.5 && x[10] <= 5.2 &&
      x[9] < 35.0) {
    return 0;
  }
  return 3;
}

// ======= NOMBRES DE CLASES =======
const char* claseNombre(int c){
  switch(c){
    case 0: return "Normal / Operación nominal";
    case 1: return "Motor bloqueado / Stall";
    case 2: return "Falla eléctrica intermitente";
    case 3: return "Bloqueo parcial / carga irregular";
    case 4: return "Motor apagado o cortocircuitado";
    default: return "Desconocido";
  }
}

// ======= PÁGINA WEB =======
void handleRoot(){
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Estado Motor</title>";
  html += "<style>";
  html += "body{font-family:Arial;text-align:center;background-color:#f0f0f0;color:#333;}";
  html += "h2{color:#0066cc;}";
  html += ".dato{font-size:1.2em;margin:5px;padding:5px;}";
  html += ".clase{font-size:1.5em;font-weight:bold;padding:10px;border-radius:10px;display:inline-block;}";
  html += ".c0{background-color:#4CAF50;color:white;}"; // Verde
  html += ".c1{background-color:#f44336;color:white;}"; // Rojo
  html += ".c2{background-color:#FF9800;color:white;}"; // Naranja
  html += ".c3{background-color:#FFEB3B;color:black;}"; // Amarillo
  html += ".c4{background-color:#9E9E9E;color:white;}"; // Gris
  html += "</style></head><body>";
  
  html += "<h2>📊 Estado del Motor</h2>";
  html += "<div class='dato'>Voltaje: " + String(voltaje,2) + " V</div>";
  html += "<div class='dato'>Corriente: " + String(corriente,1) + " mA</div>";
  html += "<div class='dato'>Potencia: " + String(potencia,1) + " mW</div>";
  html += "<div class='dato'>Temperatura: " + String(tempC,1) + " °C</div>";
  html += "<div class='dato'>Vibración: " + String(accMag,0) + "</div>";
  html += "<div class='dato'>Estado motor: " + String(estado_motor) + "</div>";
  
  // Clase con color y emoji
  String emoji="";
  switch(clase){
    case 0: emoji="✅"; break;
    case 1: emoji="🛑"; break;
    case 2: emoji="⚡"; break;
    case 3: emoji="⚠️"; break;
    case 4: emoji="🔌"; break;
  }
  html += "<div class='clase c" + String(clase) + "'>"+emoji+" Clase " + String(clase) + " - " + claseNombre(clase) + "</div>";
  
  html += "<p>Actualización automática cada 1s</p>";
  html += "<script>setTimeout(()=>{location.reload();},2000);</script>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

bool i2cAlive() {
  Wire.beginTransmission(0x68); // MPU6050
  return (Wire.endTransmission() == 0);
}



// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(21,22);
  Wire.setClock(50000);     // FIX: I2C estable
  Wire.setTimeOut(50);

  pinMode(LED_FALLO, OUTPUT);
  pinMode(LED_NORMAL, OUTPUT);

  // Apagados (activo LOW)
  digitalWrite(LED_FALLO, HIGH);
  digitalWrite(LED_NORMAL, HIGH);


  // MPU
  mpu.initialize();
  if(!mpu.testConnection()) {
    Serial.println("Error MPU6050");
    mpu_ok=false;
  }

  // INA219
  if(!ina219.begin()) {
    Serial.println("Error INA219");
    ina_ok=false;
  }

  // DS18B20
  sensors.begin();
  sensors.setResolution(9);
  sensors.setWaitForConversion(false);

  // ---- CREAR AP ----
  WiFi.softAP("ESP32-AP", "12345678");
  Serial.println("Access Point activo: ESP32-AP");

  // ---- INICIAR SERVIDOR ----
  server.on("/", handleRoot);
  server.begin();
  esp_task_wdt_config_t wdt_config = {
  .timeout_ms = 5000,
  .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
  .trigger_panic = true
};

  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);   // task loop()




}

// ===================== LOOP =====================
void loop() {
  int16_t ax=0, ay=0, az=0, gx=0, gy=0, gz=0;
  if (!i2cAlive()) {
  Wire.end();
  delay(5);
  Wire.begin(21,22);
  return; // ← CLAVE: sales del loop y evitas cuelgue
}

  if(mpu_ok){
    leerMPU(ax,ay,az,gx,gy,gz);
    delayMicroseconds(300);   // FIX
  }

  if(ina_ok){
  if(!leerINA(voltaje,corriente)){
    voltaje = 0;
    corriente = 0;
  }
}

  static unsigned long tTemp = 0;
  if(millis() - tTemp > 1000){
    tTemp = millis();
    sensors.requestTemperatures();
    float t = sensors.getTempCByIndex(0);
    if(t > -50 && t < 125) tempC = t;   // filtro anti-basura
  }


  potencia = voltaje*corriente;
  estado_motor = (voltaje>V_MIN && corriente>I_MIN)?1:0;

  accMag = sqrt((float)ax*ax + (float)ay*ay + (float)az*az);

  float x[11] = {(float)ax,(float)ay,(float)az,
                 corriente,(float)estado_motor,
                 (float)gx,(float)gy,(float)gz,
                 potencia,tempC,voltaje};

  clase = predict(x,accMag);

  static unsigned long tLed=0;
  static bool estadoLed=false;
  if(clase!=0 && clase!=2){
    if(millis()-tLed>300){
      tLed=millis();
      estadoLed=!estadoLed;
      digitalWrite(LED_FALLO,estadoLed);
    }
  } else digitalWrite(LED_FALLO,LOW);

  // ===== LÓGICA DE LEDs =====
// LEDs activos en LOW (VCC → resistencia → GPIO)
// ===== LED VERDE: ESTADO DEL SISTEMA =====
// Activo en LOW

  static unsigned long tBlink = 0;
  static bool ledOn = false;

  if (clase == 0) {
    // Normal → encendido fijo
    digitalWrite(LED_FALLO, LOW);
  }
  else if (clase == 4) {
    // Motor apagado → apagado
    digitalWrite(LED_FALLO, HIGH);
  }
  else {
    // Cualquier falla → parpadeo 1 Hz
    if (millis() - tBlink > 500) {
      tBlink = millis();
      ledOn = !ledOn;
      digitalWrite(LED_FALLO, ledOn ? LOW : HIGH);
    }
  }




  static unsigned long tWeb = 0;
  if (millis() - tWeb > 500) {
    tWeb = millis();
    server.handleClient();
  }

  Serial.println("Loop vivo");
  yield();
  delay(20);   // FIX: antes 1000, ahora no rompe WiFi
  esp_task_wdt_reset();



}
