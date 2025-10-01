#include <WebApi.h>
#include <ESPAsyncWebServer.h>
#include <Arduino.h>
#include "Types.h" 

WebApi::WebApi(FingerprintService& fp) : server_(80), fp_(fp) {}

void WebApi::routes() {
  server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *r){
    String j = String("{\"ok\":true,\"wifi\":\"") + (WiFi.isConnected()?"connected":"disconnected")
             + "\",\"ip\":\"" + WiFi.localIP().toString() + "\",\"templates\":" + fp_.count() + "}";
    r->send(200, "application/json", j);
  });

  server_.on("/api/info", HTTP_GET, [this](AsyncWebServerRequest *r){
    r->send(200, "application/json", fp_.infoJson());
  });

  server_.on("/api/count", HTTP_GET, [this](AsyncWebServerRequest *r){
    int c = fp_.count();
    if (c < 0) { r->send(500, "application/json", "{\"ok\":false}"); return; }
    r->send(200, "application/json", String("{\"ok\":true,\"count\":") + c + "}");
  });

  server_.on("/api/empty", HTTP_POST, [this](AsyncWebServerRequest *r){
    bool ok = fp_.empty();
    r->send(ok ? 200 : 500, "application/json", String("{\"ok\":") + (ok?"true":"false") + "}");
  });

  server_.on("/api/enroll", HTTP_POST, [this](AsyncWebServerRequest *r){
    if (!r->hasParam("id", true) && !r->hasParam("id")) {
      r->send(400,"application/json","{\"ok\":false,\"error\":\"id requerido\"}");
      return;
    }
    uint16_t id = (r->hasParam("id", true) ? r->getParam("id", true)->value()
                                           : r->getParam("id")->value()).toInt();
    String e;
    bool ok = fp_.enroll(id, e);
    if (ok) r->send(200, "application/json", String("{\"ok\":true,\"id\":") + id + "}");
    else    r->send(409, "application/json", String("{\"ok\":false,\"error\":\"") + e + "\"}");
  });

  server_.on("/api/match", HTTP_POST, [this](AsyncWebServerRequest *r){
  FPMatch m = fp_.match();   // <-- tipo correcto

  if (m.ok) {
    r->send(200, "application/json",
            String("{\"ok\":true,\"id\":") + m.id + ",\"score\":" + m.score + "}");
  } else {
    // ajustá el nombre del campo si en tu struct es m.error en vez de m.err
    r->send(404, "application/json",
            String("{\"ok\":false,\"error\":\"") + m.err + "\"}");
  }
});


  server_.on("/api/id", HTTP_DELETE, [this](AsyncWebServerRequest *r){
    if (!r->hasParam("id")) {
      r->send(400,"application/json","{\"ok\":false,\"error\":\"id requerido\"}");
      return;
    }
    uint16_t id = r->getParam("id")->value().toInt();
    bool ok = fp_.remove(id);
    r->send(ok?200:404, "application/json",
            String("{\"ok\":") + (ok?"true":"false") + ",\"id\":" + id + "}");
  });

  // CORS preflight para todas
  const char* uris[] = {"/api/status","/api/info","/api/count","/api/empty","/api/enroll","/api/match","/api/id"};
  for (auto uri : uris) {
    server_.on(uri, HTTP_OPTIONS, [this](AsyncWebServerRequest *r){ cors(r); });
  }
}

void WebApi::cors(AsyncWebServerRequest *req){
  auto *res = req->beginResponse(204);
  res->addHeader("Access-Control-Allow-Origin", "*");
  res->addHeader("Access-Control-Allow-Methods", "GET,POST,DELETE,OPTIONS");
  res->addHeader("Access-Control-Allow-Headers", "Content-Type");
  req->send(res);
}

void WebApi::begin(uint16_t port) {
  // si usás el AsyncWebServer ya construido con el puerto en el ctor,
  // solo llamá a routes() y server_.begin()
  routes();
  server_.begin();
}
