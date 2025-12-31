# HUELLITASFELICES
CÓDIGOS
README – Sistema automático de alimentación y suministro de agua con ESP32
1. Descripción general

Este proyecto implementa un sistema automático de dispensado de alimento y renovación de agua para mascotas, basado en un microcontrolador ESP32.
El sistema controla la cantidad de alimento dispensado mediante una celda de carga (HX711) y gestiona el suministro de agua a través de un ciclo de renovación automático, garantizando condiciones adecuadas de higiene y seguridad.

2. Funcionamiento del sistema

El sistema opera en modo de espera (standby) hasta recibir una petición de alimentación.
Una vez detectada la raza, se valida la cantidad de alimento y el número de raciones permitidas por día. Si la ración es válida, el alimento se dispensa de forma controlada por peso.
Posteriormente, se ejecuta un ciclo de renovación de agua que retira el agua residual y suministra agua nueva.

3. Componentes principales

ESP32

Celda de carga con módulo HX711

Motor DC con puente H L298N

Bombas de agua (entrada y salida)

Sensores ultrasónicos de nivel

Pantalla LCD I2C

Memoria EEPROM

4. Lógica de control

Control por peso: evita sobrealimentación y repeticiones innecesarias si el plato no está vacío.

Límites diarios: restringe el número de raciones por raza.

Renovación de agua: se ejecuta automáticamente después de un evento de alimentación.

Seguridad: incluye tiempos máximos de operación y apagado forzado de motores y bombas.

5. Interfaz y comunicación

El ESP32 crea una red WiFi local que permite la configuración y el control del sistema mediante un servidor web, a través de distintos endpoints.
La pantalla LCD muestra mensajes de estado durante la operación del sistema.

6. Almacenamiento de datos

Los eventos de alimentación y renovación de agua se almacenan en la EEPROM, permitiendo llevar un historial básico de funcionamiento del sistema.

7. Estructura del código

El código está organizado en módulos funcionales, incluyendo:

Inicialización de hardware

Lectura de sensores

Control de motor y bombas

Gestión de registros en EEPROM

Atención de peticiones web

8. Consideraciones finales

Este proyecto fue desarrollado con fines académicos y puede ser ampliado o modificado para adaptarse a diferentes tipos de mascotas o requerimientos de alimentación
