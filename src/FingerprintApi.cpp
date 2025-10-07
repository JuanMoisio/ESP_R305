#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include "FingerprintApi.h"
#include "ScanRequest.h"

// helpers estáticos
static AsyncEventSource* s_fpEvents = nullptr;
static AsyncWebServer*    s_server   = nullptr;

// Ring buffer para eventos pendientes
static constexpr int MAX_PENDING = 16;
struct PendingEvent {
  char type[20];
  char data[256];
};
static PendingEvent s_queue[MAX_PENDING];
static int s_qHead = 0;
static int s_qTail = 0;

// Mutex para proteger la cola entre tasks/loop
static portMUX_TYPE s_fpMux = portMUX_INITIALIZER_UNLOCKED;

static inline bool queueEmpty() { return s_qHead == s_qTail; }
static inline bool queueFull()  { return ((s_qTail + 1) % MAX_PENDING) == s_qHead; }

static void enqueueEvent(const char* type, const char* json) {
  portENTER_CRITICAL(&s_fpMux);
  int next = (s_qTail + 1) % MAX_PENDING;
  if (next == s_qHead) {
    // cola llena: descartar el más viejo (avanzar head) para hacer sitio
    s_qHead = (s_qHead + 1) % MAX_PENDING;
  }
  strncpy(s_queue[s_qTail].type, type, sizeof(s_queue[s_qTail].type)-1);
  s_queue[s_qTail].type[sizeof(s_queue[s_qTail].type)-1] = '\0';
  strncpy(s_queue[s_qTail].data, json, sizeof(s_queue[s_qTail].data)-1);
  s_queue[s_qTail].data[sizeof(s_queue[s_qTail].data)-1] = '\0';
  s_qTail = next;
  portEXIT_CRITICAL(&s_fpMux);
}

static inline bool canSendEvents() {
  return (s_fpEvents != nullptr) && (WiFi.status() == WL_CONNECTED);
}

void initFingerprintApi(AsyncWebServer& server, AsyncEventSource& events) {
  s_server = &server;
  s_fpEvents = &events;

  server.on("/fp/command", HTTP_GET, [](AsyncWebServerRequest *req){
    String action;
    if (req->hasParam("action")) action = req->getParam("action")->value();
    String idParam = req->hasParam("id") ? req->getParam("id")->value() : "";

    if (action == "scan") {
      // instruir UI para pedir huella (SSE) y solicitar a AutoMode que inicie MATCHING
      fpApiEmitPrompt();
      requestScan();
      req->send(202, "application/json", "{\"status\":\"ok\",\"action\":\"scan\"}");
      return;
    }
    if (action == "enrollStart") {
      fpApiEmitEnrollStart();
      req->send(202, "application/json", "{\"status\":\"ok\",\"action\":\"enrollStart\"}");
      return;
    }
    if (action == "enrollAbort") {
      fpApiEmitEnrollAbort();
      req->send(202, "application/json", "{\"status\":\"ok\",\"action\":\"enrollAbort\"}");
      return;
    }
    if (action == "erase") {
      if (idParam.length() == 0) {
        req->send(400, "application/json", "{\"error\":\"missing id\"}");
        return;
      }
      fpApiEmitEraseRequest(idParam.toInt());
      req->send(202, "application/json", String("{\"status\":\"ok\",\"action\":\"erase\",\"id\":") + idParam + "}");
      return;
    }
    if (action == "status") {
      const char *body = "{\"status\":\"idle\",\"scanBar\":true}";
      req->send(200, "application/json", body);
      return;
    }
    req->send(400, "application/json", "{\"error\":\"unknown action\"}");
  });

  server.on("/fp", HTTP_GET, [](AsyncWebServerRequest *req){
    const char *html = "<html><body><h3>Fingerprint API</h3>"
                       "<p>Use /fp/command?action=scan or subscribe to SSE /fp/events</p></body></html>";
    req->send(200, "text/html", html);
  });
}

// Encolado (ya no envían inmediatamente)
void fpApiEmitPrompt() {
  enqueueEvent("prompt", "{\"event\":\"prompt\",\"msg\":\"Ponga su huella\"}");
}
void fpApiEmitResult(bool ok, int id, int score) {
  char buf[128];
  snprintf(buf, sizeof(buf), "{\"event\":\"result\",\"ok\":%s,\"id\":%d,\"score\":%d}",
           ok ? "true" : "false", id, score);
  enqueueEvent("result", buf);
}
void fpApiEmitEnrollStart() {
  enqueueEvent("enroll", "{\"event\":\"enroll\",\"stage\":\"start\"}");
}
void fpApiEmitEnrollAbort() {
  enqueueEvent("enroll", "{\"event\":\"enroll\",\"stage\":\"abort\"}");
}
void fpApiEmitEnrollResult(bool ok, int id) {
  char buf[80];
  snprintf(buf, sizeof(buf), "{\"event\":\"enroll\",\"stage\":\"result\",\"ok\":%s,\"id\":%d}",
           ok ? "true" : "false", id);
  enqueueEvent("enroll", buf);
}
void fpApiEmitEraseRequest(int id) {
  char buf[80];
  snprintf(buf, sizeof(buf), "{\"event\":\"erase\",\"stage\":\"request\",\"id\":%d}", id);
  enqueueEvent("erase", buf);
}
void fpApiEmitEraseResult(bool ok, int id) {
  char buf[80];
  snprintf(buf, sizeof(buf), "{\"event\":\"erase\",\"stage\":\"result\",\"ok\":%s,\"id\":%d}",
           ok ? "true" : "false", id);
  enqueueEvent("erase", buf);
}

// Llamar periódicamente desde loop() para enviar lo encolado de forma segura
void fpApiLoop() {
  // nada que hacer si no hay eventos en cola
  if (queueEmpty()) return;

  // si hay eventos pero no se puede enviar, imprimir aviso con rate limit
  if (!canSendEvents()) {
    static unsigned long lastWarn = 0;
    unsigned long now = millis();
    if (now - lastWarn > 5000) {
      Serial.printf("[fpapi] eventos pendientes=%d, esperando WiFi/SSE\n",
                    (s_qTail - s_qHead + MAX_PENDING) % MAX_PENDING);
      lastWarn = now;
    }
    return;
  }

  // enviar encolados
  while (true) {
    portENTER_CRITICAL(&s_fpMux);
    if (s_qHead == s_qTail) {
      portEXIT_CRITICAL(&s_fpMux);
      break;
    }
    PendingEvent ev = s_queue[s_qHead];
    s_qHead = (s_qHead + 1) % MAX_PENDING;
    portEXIT_CRITICAL(&s_fpMux);

    if (s_fpEvents) {
      s_fpEvents->send(ev.data, ev.type, millis());
      // yield to allow background tasks to run
      delay(0);
    }
  }
}