HUELLITAS FELICES
CÓDIGOS
README – Sistema automático de alimentación y suministro de agua con ESP32
1. Descripción general

Este proyecto implementa un sistema automático de dispensado de alimento y renovación de agua para mascotas, basado en un microcontrolador ESP32.
El sistema controla la cantidad de alimento dispensado mediante una celda de carga (HX711) y gestiona el suministro de agua a través de un ciclo de renovación automático, garantizando condiciones adecuadas de higiene, seguridad y bienestar para la mascota.

Adicionalmente, el sistema incorpora un módulo de predicción de fallas, orientado a la detección temprana de anomalías en el motor del dispensador, mejorando la confiabilidad y la seguridad operativa del dispositivo.

2. Funcionamiento del sistema

El sistema opera en modo de espera (standby) hasta recibir una petición de alimentación.
Una vez detectada la raza, se valida la cantidad de alimento y el número de raciones permitidas por día. Si la ración es válida, el alimento se dispensa de forma controlada por peso mediante la celda de carga.

Posteriormente, se ejecuta un ciclo automático de renovación de agua, el cual retira el agua residual del plato y suministra agua limpia.
Durante la operación, el sistema supervisa el estado del motor para identificar posibles condiciones anómalas de funcionamiento.

3. Componentes principales

ESP32

Celda de carga con módulo HX711

Motor DC con puente H L298N

Bombas de agua (entrada y salida)

Sensores ultrasónicos de nivel

Pantalla LCD I2C

Memoria EEPROM

Sensores para monitoreo del estado del motor (corriente, vibración y temperatura)

4. Lógica de control

Control por peso: evita la sobrealimentación y bloquea nuevas dispensaciones si el plato no se encuentra vacío.

Límites diarios: restringe el número de raciones según la raza detectada.

Renovación de agua: se ejecuta automáticamente después de cada evento de alimentación.

Seguridad: incluye tiempos máximos de operación y apagado forzado de motores y bombas ante fallos.

Predicción de fallas: analiza variables del motor para detectar condiciones como sobrecarga, desbalance o cortocircuito.

5. Interfaz y comunicación

El ESP32 crea una red Wi-Fi local que permite la configuración y el monitoreo del sistema mediante un servidor web, utilizando distintos endpoints para el envío y consulta de datos.
La pantalla LCD muestra mensajes de estado, alertas y confirmaciones durante la operación del sistema.

6. Almacenamiento de datos

Los eventos de alimentación, renovación de agua y estados relevantes del sistema se almacenan en la memoria EEPROM, permitiendo llevar un historial básico del funcionamiento incluso después de reinicios del sistema.

7. Sistema de predicción de fallas

El sistema incorpora un módulo de predicción de fallas que monitorea el comportamiento del motor del dispensador.
A partir de la medición de variables como corriente eléctrica, vibración y temperatura, se identifica el estado de operación del motor y se detectan posibles anomalías.

Cuando se reconoce una condición anormal, el sistema genera una alerta y puede detener la operación del motor para evitar daños mecánicos o eléctricos, contribuyendo a un funcionamiento más seguro y confiable del dispositivo.

8. Estructura del código

El código está organizado en módulos funcionales, incluyendo:

Inicialización de hardware

Lectura de sensores

Control de motor y bombas

Gestión de registros en EEPROM

Atención de peticiones web

Módulo de análisis y predicción de fallas

9. Consideraciones finales

Este proyecto fue desarrollado con fines académicos y está orientado a demostrar la integración de control embebido, sensado, comunicación IoT y técnicas básicas de predicción de fallas.
El sistema puede ser ampliado o adaptado para diferentes tipos de mascotas, capacidades de dispensado o niveles de automatización.
