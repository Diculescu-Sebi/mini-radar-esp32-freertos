#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Servo.h>

// =====================================================
// WIFI + TCP
// =====================================================

const char* WIFI_SSID = "DIGI-SRI";
const char* WIFI_PASSWORD = "DianaJmekera";

WiFiServer server(5000);
WiFiClient client;


// =====================================================
// PINI
// =====================================================

const int TRIG_PIN = 12;
const int ECHO_PIN = 13;
const int SERVO_PIN = 18;


// =====================================================
// SERVO
// =====================================================

Servo servo;

const int SERVO_LEFT  = 1545;
const int SERVO_RIGHT = 1455;
const int SERVO_STOP  = 1500;

// Calibrarea care a functionat
const int LEFT_TIME  = 1050;   // ~90°
const int RIGHT_TIME = 1000;   // ~90°


// =====================================================
// QUEUES
// =====================================================

QueueHandle_t dataQueue;
QueueHandle_t commandQueue;


// =====================================================
// DATE SENZOR
// =====================================================

struct SensorData {
  float distance;
  float angle;
  unsigned long timestamp;
};


// =====================================================
// COMENZI
// =====================================================

enum Command {
  CMD_START,
  CMD_STOP
};


// =====================================================
// REAL-TIME TASK
// Servo + HC-SR04
// =====================================================

void RealTimeTask(void *parameter) {

  SensorData data;
  Command command;

  bool running = false;

  // Unghiul radarului
  float angle = -90.0;

  // false = -90 -> +90
  // true  = +90 -> -90
  bool goingBack = false;

  unsigned long scanStartTime = 0;

  while (true) {

    // =================================================
    // PRIMIM COMENZI
    // =================================================

    if (xQueueReceive(commandQueue, &command, 0) == pdTRUE) {

      if (command == CMD_START) {

        running = true;

        angle = -90.0;
        goingBack = false;
        scanStartTime = millis();

        Serial.println("RealTimeTask: START");
      }

      else if (command == CMD_STOP) {

        running = false;

        servo.writeMicroseconds(SERVO_STOP);

        Serial.println("RealTimeTask: STOP");
      }
    }


    // =================================================
    // RADAR PORNIT
    // =================================================

    if (running) {

      unsigned long elapsed = millis() - scanStartTime;


      // =================================================
      // -90° -> +90°
      // =================================================

      if (!goingBack) {

        servo.writeMicroseconds(SERVO_LEFT);

        angle = -90.0 +
                (elapsed * 180.0 / (LEFT_TIME * 2.0));

        if (angle >= 90.0) {

          angle = 90.0;

          servo.writeMicroseconds(SERVO_STOP);

          delay(100);

          goingBack = true;

          scanStartTime = millis();
        }
      }


      // =================================================
      // +90° -> -90°
      // =================================================

      else {

        servo.writeMicroseconds(SERVO_RIGHT);

        elapsed = millis() - scanStartTime;

        angle = 90.0 -
                (elapsed * 180.0 / (RIGHT_TIME * 2.0));

        if (angle <= -90.0) {

          angle = -90.0;

          servo.writeMicroseconds(SERVO_STOP);

          delay(100);

          goingBack = false;

          scanStartTime = millis();
        }
      }


      // =================================================
      // HC-SR04
      // =================================================

      digitalWrite(TRIG_PIN, LOW);
      delayMicroseconds(2);

      digitalWrite(TRIG_PIN, HIGH);
      delayMicroseconds(10);

      digitalWrite(TRIG_PIN, LOW);

      long duration =
        pulseIn(ECHO_PIN, HIGH, 30000);


      if (duration == 0) {

        data.distance = -1;

      } else {

        data.distance =
          duration * 0.0343 / 2.0;
      }


      data.angle = angle;
      data.timestamp = millis();


      // =================================================
      // TRIMITEM DATELE IN QUEUE
      // =================================================

      xQueueSend(
        dataQueue,
        &data,
        0
      );


      // Aproximativ 10 masuratori/secunda
      vTaskDelay(
        pdMS_TO_TICKS(100)
      );
    }

    else {

      servo.writeMicroseconds(SERVO_STOP);

      vTaskDelay(
        pdMS_TO_TICKS(20)
      );
    }
  }
}


// =====================================================
// COMMUNICATION TASK
// =====================================================

void CommunicationTask(void *parameter) {

  SensorData receivedData;
  Command command;

  while (true) {

    // =================================================
    // CONECTARE PC
    // =================================================

    if (!client || !client.connected()) {

      client = server.available();

      if (client) {

        client.setTimeout(100);

        Serial.println(
          "PC conectat prin TCP!"
        );

        client.println(
          "RADAR READY"
        );
      }
    }


    // =================================================
    // COMENZI DE LA PC
    // =================================================

    if (
      client &&
      client.connected() &&
      client.available()
    ) {

      String message =
        client.readStringUntil('\n');

      message.trim();
      message.toUpperCase();


      Serial.print(
        "Comanda primita: "
      );

      Serial.println(message);


      if (message == "START") {

        command = CMD_START;

        xQueueSend(
          commandQueue,
          &command,
          portMAX_DELAY
        );

        client.println(
          "OK START"
        );
      }


      else if (message == "STOP") {

        command = CMD_STOP;

        xQueueSend(
          commandQueue,
          &command,
          portMAX_DELAY
        );

        client.println(
          "OK STOP"
        );
      }


      else {

        client.println(
          "ERROR UNKNOWN COMMAND"
        );
      }
    }


    // =================================================
    // DATE DIN REAL-TIME TASK
    // =================================================

    if (
      xQueueReceive(
        dataQueue,
        &receivedData,
        0
      ) == pdTRUE
    ) {

      Serial.print("Distance: ");

      if (receivedData.distance < 0) {

        Serial.print("N/A");

      } else {

        Serial.print(
          receivedData.distance,
          1
        );

        Serial.print(" cm");
      }


      Serial.print(" | Angle: ");

      Serial.print(
        receivedData.angle,
        1
      );

      Serial.print(" deg | Time: ");

      Serial.print(
        receivedData.timestamp
      );

      Serial.println(" ms");


      // =================================================
      // TRIMITERE TCP
      // =================================================

      if (
        client &&
        client.connected()
      ) {

        client.print("DIST=");

        client.print(
          receivedData.distance,
          1
        );

        client.print(",ANGLE=");

        client.print(
          receivedData.angle,
          1
        );

        client.print(",TIME=");

        client.println(
          receivedData.timestamp
        );
      }
    }


    vTaskDelay(
      pdMS_TO_TICKS(10)
    );
  }
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println(
    "======================================"
  );
  Serial.println(
    " MINI RADAR - ESP32 + FREERTOS + TCP"
  );
  Serial.println(
    "======================================"
  );


  // =================================================
  // HC-SR04
  // =================================================

  pinMode(
    TRIG_PIN,
    OUTPUT
  );

  pinMode(
    ECHO_PIN,
    INPUT
  );

  digitalWrite(
    TRIG_PIN,
    LOW
  );


  // =================================================
  // SERVO
  // =================================================

  servo.setPeriodHertz(50);

  servo.attach(
    SERVO_PIN,
    500,
    2500
  );

  servo.writeMicroseconds(
    SERVO_STOP
  );


  // =================================================
  // QUEUES
  // =================================================

  dataQueue =
    xQueueCreate(
      10,
      sizeof(SensorData)
    );

  commandQueue =
    xQueueCreate(
      5,
      sizeof(Command)
    );


  if (
    dataQueue == NULL ||
    commandQueue == NULL
  ) {

    Serial.println(
      "EROARE: Queue!"
    );

    while (true) {
      delay(1000);
    }
  }


  // =================================================
  // WIFI
  // =================================================

  Serial.print(
    "Conectare la Wi-Fi"
  );

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );


  while (
    WiFi.status() != WL_CONNECTED
  ) {

    delay(500);

    Serial.print(".");
  }


  Serial.println();

  Serial.println(
    "Wi-Fi conectat!"
  );

  Serial.print(
    "IP ESP32: "
  );

  Serial.println(
    WiFi.localIP()
  );


  // =================================================
  // TCP SERVER
  // =================================================

  server.begin();

  Serial.println(
    "TCP server pornit!"
  );

  Serial.println(
    "Port: 5000"
  );


  // =================================================
  // FREE RTOS TASKS
  // =================================================

  xTaskCreate(
    RealTimeTask,
    "RealTimeTask",
    4096,
    NULL,
    2,
    NULL
  );

  xTaskCreate(
    CommunicationTask,
    "CommunicationTask",
    4096,
    NULL,
    1,
    NULL
  );
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  vTaskDelay(
    pdMS_TO_TICKS(1000)
  );
}