#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>
#include "HX711.h" 

// --------- CELDA DE CARGA HX711 ----------
#define DOUT 5
#define CLK 4

HX711 scale;

// TUS VALORES DE CALIBRACIÓN:
const long OFFSET_CELDA = 210057;     // Tu resultado: 210057
const float FACTOR_CALIBRACION = 1810.81; // Tu resultado: 1810.81

const int NUM_LECTURAS_CELDA = 15; // Número de lecturas para promedio
const float PESO_MINIMO_REPETIR = 25.0; // Límite de comida en el plato para permitir otra ración (en gramos)
const float TOLERANCIA_PESO = 3.0; // Tolerancia de +/- 5 gramos para detener el motor

// --------- CICLO DE AGUA ----------

const unsigned long TIEMPO_SUCCION = 10000;  // 3 s
const unsigned long TIEMPO_AGUA = 8000;    // 10 s

bool cicloAguaActivo = false;

bool enSuccion = false;
bool enAgua = false;

unsigned long inicioEventoAgua = 0;


const unsigned long TIEMPO_ESPERA_ENTRE_BOMBAS = 3000; // 3 segundos




// --------- EEPROM ----------
#define EEPROM_SIZE 1024
#define ADDR_COUNT 0
#define ADDR_RECORDS 4              // comida


#define MAX_REGISTROS ((EEPROM_SIZE - ADDR_RECORDS) / sizeof(Registro))



// --------- BOMBAS DE AGUA ----------
#define BOMBA1_PIN 32
#define BOMBA2_PIN 17



struct Registro {
  char raza[20];
  float cantidad;
  int vezActual;
  int totalVeces;
  char fechaHora[25];
};


bool pedirRenovarAgua = false;
String fechaAguaPendiente = "";


int totalRegistros = 0; // Comida

String ultimaFecha = "";

// --------- LCD I2C ----------
LiquidCrystal_PCF8574 lcd(0x27);

// --------- WiFi / WebServer ----------
const char* ssid = "ESP32_RAZAS";
const char* password = "12345678";
WebServer server(80);

// --------- Variables de configuración ----------
int vecesChi = 0;
float cantChi = 0;
int vecesPom = 0;
float cantPom = 0;
int vecesPug = 0;
float cantPug = 0;

// Contadores diarios
int comidasChi = 0;
int comidasPom = 0;
int comidasPug = 0;

// --------- Motor (L298N) ----------
const int pinENA = 25;
const int pinIN1 = 26;
const int pinIN2 = 27;

// --------- ULTRASÓNICO 1 (Alimentos) ----------
const int trigPin = 18;
const int echoPin = 19;

// --------- LEDs ULTRASÓNICO 1 ----------
const int ledMayor5 = 13;
const int ledMenor5 = 14;

// --------- ULTRASONICO 2 (Otro Nivel) ----------
#define TRIG2 23
#define ECHO2 35

// --------- LEDs ULTRASÓNICO 2 ----------
const int ledMayor5_2 = 15;
const int ledMenor5_2 = 33;


time_t stringToTime(String fecha) {
  struct tm t;
  memset(&t, 0, sizeof(t));

  t.tm_year = fecha.substring(0,4).toInt() - 1900;
  t.tm_mon  = fecha.substring(5,7).toInt() - 1;
  t.tm_mday = fecha.substring(8,10).toInt();
  t.tm_hour = fecha.substring(11,13).toInt();
  t.tm_min  = fecha.substring(14,16).toInt();
  t.tm_sec  = fecha.substring(17,19).toInt();

  return mktime(&t);
}
String timeToString(time_t t) {
  struct tm *tm_info = localtime(&t);
  char buffer[20];
  strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", tm_info);
  return String(buffer);
}


// ------------------------------------------------------------------
// -------------------- FUNCIONES HX711 -----------------------------
// ------------------------------------------------------------------

// Función para leer promedio de N lecturas y convertir a peso en gramos
long leerPromedioCrudo(int n) {
  long suma = 0;
  for (int i = 0; i < n; i++) {
    while (!scale.is_ready()); 
    suma += scale.read();
    delay(50); 
  }
  return suma / n;
}

float leerPesoPromedio(int n) {
  long lecturaPromedio = leerPromedioCrudo(n);
  float peso = (float)(lecturaPromedio - OFFSET_CELDA) / FACTOR_CALIBRACION;

  // Descontar 100g del plato
  peso -= 100.0;

  // Evitar valores negativos cuando el plato está vacío
  if (peso < 0) peso = 0;

  return peso;
}


bool hayAgua = false;   // GLOBAL

void renovarAgua(String fechaHora) {
  Serial.println("💧 Renovando agua...");

  // 🟢 SI YA HABÍA AGUA → quitar primero
  if (hayAgua) {
    Serial.println("🌀 Quitando agua vieja");
    digitalWrite(BOMBA2_PIN, HIGH);
    delay(TIEMPO_SUCCION);
    digitalWrite(BOMBA2_PIN, LOW);

    Serial.println("⏳ Esperando que se estabilice...");
    delay(TIEMPO_ESPERA_ENTRE_BOMBAS);
  } else {
    Serial.println("🆕 Primera vez: no se quita agua");
  }

  // 🚰 Siempre poner agua nueva
  Serial.println("🚰 Poniendo agua nueva");
  digitalWrite(BOMBA1_PIN, HIGH);  // ✅ AGUA ON
  delay(TIEMPO_AGUA);
  digitalWrite(BOMBA1_PIN, LOW);   // ✅ AGUA OFF

  // 💾 Guardar historial
  guardarRegistro("AGUA", 0.0, 0, 0, fechaHora);

  hayAgua = true;

  Serial.println("✅ Agua renovada correctamente");

  // 🔒 Seguridad final
  digitalWrite(BOMBA1_PIN, LOW);
  digitalWrite(BOMBA2_PIN, LOW);
}



// --------- FUNCION LCD ----------
void mostrarLCD(String l1, String l2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(l1.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(l2.substring(0, 16));
}

// --------- MEDIR DISTANCIA 1 ----------
float medirDistancia() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duracion = pulseIn(echoPin, HIGH, 30000);
  float distancia = (duracion * 0.0343) / 2;
  
  return distancia;
  
}

// --------- MEDIR DISTANCIA 2 ----------
float medirDistancia2() {
  digitalWrite(TRIG2, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG2, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG2, LOW);

  long duracion = pulseIn(ECHO2, HIGH, 30000);
  float distancia2 = (duracion * 0.0343) / 2;

  return distancia2;
  
}

// --------- Guardar registro de Comida ----------
void guardarRegistro(String raza, float cantidad, int vezActual, int totalVeces, String fechaHora) {
  Serial.println("💾 Guardando registro de COMIDA en EEPROM...");

  Registro nuevo;
  raza.toCharArray(nuevo.raza, sizeof(nuevo.raza));
  nuevo.cantidad = cantidad;
  nuevo.vezActual = vezActual;
  nuevo.totalVeces = totalVeces;
  fechaHora.toCharArray(nuevo.fechaHora, sizeof(nuevo.fechaHora));

  int addr = ADDR_RECORDS + totalRegistros * sizeof(Registro);
  if (addr + sizeof(Registro) > EEPROM_SIZE) return;

  EEPROM.put(addr, nuevo);
  totalRegistros++;
  EEPROM.put(ADDR_COUNT, totalRegistros);
  EEPROM.commit();

  Serial.print("  [TEST] Guardado OK para "); Serial.println(nuevo.raza);
}



// --------- Motor con control de peso ----------
void alimentarMotor(float gramos) {
  
  // 1. Verificar peso actual en el plato (ANTES de dispensar)
  float pesoActual = leerPesoPromedio(NUM_LECTURAS_CELDA);
  Serial.print("⚖️ Peso inicial en plato: ");
  Serial.print(pesoActual, 1);
  Serial.println(" g");
  
  // Lógica de "Plato Lleno": Si hay 25g o más en el plato, NO se dispensa.
  if (pesoActual >= PESO_MINIMO_REPETIR) {
    Serial.print("⚠️ Hay ");
    Serial.print(pesoActual, 1);
    Serial.println(" g en el plato. Plato lleno (Límite: <" + String(PESO_MINIMO_REPETIR, 0) + "g). No se dispensa.");
    mostrarLCD("Plato lleno", String(pesoActual, 1) + "g");
    return; // Detiene la función sin dispensar
  }

  // Si llega aquí, es seguro dispensar.
  Serial.println("⚙️ Motor: Iniciando dispensación controlada por peso...");
  mostrarLCD("Dispensando...", String(gramos, 1) + "g");
  
  // Peso objetivo a alcanzar (con la nueva ración)
  float pesoObjetivo = pesoActual + gramos; 
  Serial.print("🎯 Peso objetivo: ");
  Serial.print(pesoObjetivo, 1);
  Serial.println(" g (+/- " + String(TOLERANCIA_PESO, 1) + "g)");
  
  // 2. Encender motor
  
  digitalWrite(pinIN1, HIGH);
  digitalWrite(pinIN2, LOW);
  digitalWrite(pinENA, HIGH);
  // Velocidad constante

  // 3. Bucle de dispensación controlado por peso
  unsigned long inicioGiro = millis();
  float pesoActualizado = pesoActual;
  
  // Continuar dispensando MIENTRAS el peso esté FUERA del rango de tolerancia.
  // Rango de tolerancia: [pesoObjetivo - TOLERANCIA, pesoObjetivo + TOLERANCIA]
 while (pesoActualizado < (pesoObjetivo - TOLERANCIA_PESO)) {

  if (millis() - inicioGiro > 80000) {
    Serial.println("❌ Límite de tiempo excedido (10s)");
    break;
  }

  pesoActualizado = leerPesoPromedio(1);

  Serial.print("📈 Peso en tiempo real: ");
  Serial.print(pesoActualizado, 1);
  Serial.println(" g");

  // 🛑 Seguridad extra: si se pasó por vibración o salto
  if (pesoActualizado >= pesoObjetivo) {
    Serial.println("🛑 Sobrepasó el peso objetivo. Motor detenido inmediatamente.");
    break;
  }

  delay(150);
}


  // 4. Detener motor
  digitalWrite(pinENA, LOW);
  digitalWrite(pinIN1, LOW);
  digitalWrite(pinIN2, LOW);
  Serial.println("⚙️ Motor OFF (por peso)");

  // 5. Lectura final
  pesoActualizado = leerPesoPromedio(NUM_LECTURAS_CELDA); // Lectura más precisa (promedio)
  mostrarLCD("Dispensado OK", String(pesoActualizado, 1) + "g final");
}


// --------- Reset diario ----------
void verificarCambioDia(String fechaHora) {
  if (fechaHora.length() < 10) return;
  String fechaDia = fechaHora.substring(0, 10);

  if (ultimaFecha == "") {
    ultimaFecha = fechaDia;
    return;
  }

  if (fechaDia != ultimaFecha) {
    ultimaFecha = fechaDia;
    comidasChi = comidasPom = comidasPug = 0;
    Serial.println("🌅 [TEST OK] Nuevo día → contadores de comida reiniciados a Cero.");
  }
}

// --------- ENDPOINT /config ----------
void handleConfig() {
  Serial.println("⚙️ Configuracion recibida desde /config");
  if (server.hasArg("chi")) {
    cantChi = server.arg("chi").toFloat();
    vecesChi = server.arg("veceschi").toInt();
  }
  if (server.hasArg("pom")) {
    cantPom = server.arg("pom").toFloat();
    vecesPom = server.arg("vecespom").toInt();
  }
  if (server.hasArg("pug")) {
    cantPug = server.arg("pug").toFloat();
    vecesPug = server.arg("vecespug").toInt();
  }
  Serial.println("📦 Configuración actual:");
  Serial.printf("   Chihuahua: %.1f g, %d veces\n", cantChi, vecesChi);
  Serial.printf("   Pomerania: %.1f g, %d veces\n", cantPom, vecesPom);
  Serial.printf("   Pug: %.1f g, %d veces\n", cantPug, vecesPug);

  mostrarLCD("Config recibida", "");
  server.send(200, "text/plain", "Configuracion recibida");
}

// --------- ENDPOINT /raza ----------
void handleRaza() {
  if (!server.hasArg("nombre") || !server.hasArg("fecha")) {
    server.send(400, "text/plain", "Faltan parametros");
    return;
  }
  
  String raza = server.arg("nombre");
  String fecha = server.arg("fecha");
  
  Serial.println("🐶 Petición /raza recibida:");
  Serial.print("  Raza: "); Serial.println(raza);
  Serial.print("  Fecha: "); Serial.println(fecha);
  
  verificarCambioDia(fecha);
  
  float cantidad = 0;
  int actual = 0, total = 0;
  bool registrar = false;
  String mensajeLCD;
  
  if (raza == "chihuahua") {
    total = vecesChi;
    cantidad = cantChi;
    if (comidasChi < vecesChi) { comidasChi++; actual = comidasChi; registrar = true; }
    mensajeLCD = "Chihuahua";
  }
  else if (raza == "pomerania") {
    total = vecesPom;
    cantidad = cantPom;
    if (comidasPom < vecesPom) { comidasPom++; actual = comidasPom; registrar = true; }
    mensajeLCD = "Pomerania";
  }
  else if (raza == "pug") {
    total = vecesPug;
    cantidad = cantPug;
    if (comidasPug < vecesPug) { comidasPug++; actual = comidasPug; registrar = true; }
    mensajeLCD = "Pug";
  }
  else {
    server.send(200, "text/plain", "Raza no registrada");
    return;
  }
  
  Serial.print("  Cantidad asignada: "); Serial.println(cantidad);
  Serial.print("  Veces hoy: "); Serial.print(actual); Serial.print("/"); Serial.println(total);
  mostrarLCD(mensajeLCD, String(cantidad) + "g (" + actual + "/" + total + ")");
  
  // Si la ración es permitida hoy
  if (registrar) {
    Serial.println("📥 [TEST OK] Ración permitida. Guardando registro y activando motor...");
    // alimentarMotor contiene la lógica de "plato lleno" (peso >= 25g)
    guardarRegistro(raza, cantidad, actual, total, fecha);
    alimentarMotor(cantidad);
    pedirRenovarAgua = true;
    fechaAguaPendiente = fecha;


  } else {
    Serial.println("⚠️ [TEST OK] Ración excedida (" + String(actual-1) + "/" + String(total) + "). No se dispensa.");
    mostrarLCD("Racion excedida", "Max " + String(total) + " veces");
  }
  server.send(200, "text/plain", String(cantidad) + " (" + actual + "/" + total + ")");
}

void handleHistorial() {
  Serial.println("📜 Enviando historial WEB (últimos 10)");

  String out = "--- HISTORIAL (ULTIMOS 10 EVENTOS) ---\n";

  int registrosValidos = totalRegistros;
  if (registrosValidos < 0 || registrosValidos > MAX_REGISTROS)
    registrosValidos = 0;

  int inicio = max(0, registrosValidos - 10);

  for (int i = inicio; i < registrosValidos; i++) {

    Registro r;
    int addr = ADDR_RECORDS + i * sizeof(Registro);
    EEPROM.get(addr, r);

    // Seguridad strings
    r.raza[sizeof(r.raza) - 1] = '\0';
    r.fechaHora[sizeof(r.fechaHora) - 1] = '\0';

    out += String(i - inicio + 1) + ") ";

    // 🔹 EVENTO AGUA
    if (String(r.raza) == "AGUA") {
      out += "💧 AGUA | Renovada | ";
      out += r.fechaHora;
    }
    // 🔹 EVENTO COMIDA
    else {
      out += r.raza;
      out += " | " + String(r.cantidad, 1) + " g | ";
      out += "(" + String(r.vezActual) + "/" + String(r.totalVeces) + ") | ";
      out += r.fechaHora;
    }

    out += "\n";
  }

  server.send(200, "text/plain", out);
}

// --------- ROOT ----------
void handleRoot() {
  Serial.println("📡 Cliente visitó /");
  server.send(200, "text/plain", "Servidor ESP32 activo");
}

// ------------------------------------------------------------------
// ---------------------------- SETUP -------------------------------
// ------------------------------------------------------------------

void setup() {
   // 🔒 SEGURIDAD RELÉS
  pinMode(BOMBA1_PIN, OUTPUT);
  pinMode(BOMBA2_PIN, OUTPUT);

  digitalWrite(BOMBA1_PIN, LOW); // OFF
  digitalWrite(BOMBA2_PIN, LOW); // OFF

  delay(200); // estabiliza
  pinMode(25, OUTPUT);  // ENA
  pinMode(26, OUTPUT);  // IN1
  pinMode(27, OUTPUT);  // IN2

  digitalWrite(25, LOW); // motor OFF
  digitalWrite(26, LOW);
  digitalWrite(27, LOW);

  

  Serial.begin(115200);

  Serial.println("🚀 ESP32 iniciando...");
  delay(2000); 
  Wire.begin(21,22);     // I2C ESP32
  delay(100);            // tiempo para LCD

  lcd.begin(16, 2);
  lcd.setBacklight(255);

  lcd.setCursor(0,0);
  lcd.print("LCD OK");

  EEPROM.begin(EEPROM_SIZE);

  // Cargar contador único
  EEPROM.get(ADDR_COUNT, totalRegistros);
  if (totalRegistros < 0 || totalRegistros > MAX_REGISTROS)
    totalRegistros = 0;

  Serial.print("📚 Eventos cargados en EEPROM: ");
  Serial.println(totalRegistros);


 

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  pinMode(ledMayor5, OUTPUT);
  pinMode(ledMenor5, OUTPUT);
  digitalWrite(ledMayor5, HIGH);
  digitalWrite(ledMenor5, HIGH);

  pinMode(TRIG2, OUTPUT);
  pinMode(ECHO2, INPUT);

  pinMode(ledMayor5_2, OUTPUT);
  pinMode(ledMenor5_2, OUTPUT);
  digitalWrite(ledMayor5_2, HIGH);
  digitalWrite(ledMenor5_2, HIGH);


  // Inicialización de la Celda de Carga
  scale.begin(DOUT, CLK);
  Serial.println("⚖️ HX711 Celda de Carga lista.");
  
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();

  Serial.println("🌐 Servidor WiFi creado");
  Serial.print("  SSID: "); Serial.println(ssid);
  Serial.print("  IP: "); Serial.println(IP);

  server.on("/", handleRoot);
  server.on("/config", handleConfig);
  server.on("/raza", handleRaza);
  server.on("/historial", handleHistorial);
  server.begin();

  Serial.println("🟢 Servidor listo");
  mostrarLCD("Servidor listo", IP.toString());



}

// ------------------------------------------------------------------
// ---------------------------- LOOP --------------------------------
// ------------------------------------------------------------------

void loop() {
  server.handleClient();

  // ---- Ultrasonico 1 ----
  float d = medirDistancia();
  if (d > 8.0) {
    digitalWrite(ledMayor5, LOW);
    digitalWrite(ledMenor5, HIGH);
  } else {
    digitalWrite(ledMayor5, HIGH);
    digitalWrite(ledMenor5, LOW);
  }

  // ---- Ultrasonico 2 ----
  float d2 = medirDistancia2();
  if (d2 > 15.0) {
    digitalWrite(ledMayor5_2, LOW);
    digitalWrite(ledMenor5_2, HIGH);
  } else {
    digitalWrite(ledMayor5_2, HIGH);
    digitalWrite(ledMenor5_2, LOW);
  }

  // 💧 Renovación de agua (evento)
  if (pedirRenovarAgua) {
    renovarAgua(fechaAguaPendiente);
    pedirRenovarAgua = false;  // 🔒 evita repetir
  }
}
