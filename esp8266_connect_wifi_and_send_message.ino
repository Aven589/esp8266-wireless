#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#pragma pack(push, 1)
struct EspToCarFrame {
  uint8_t head;
  float targetX;
  float targetY;
  float pidKp;
  float pidKi;
  float pidKd;
  uint8_t tail;
};

struct CarToEspFrame {
  uint8_t head;
  float carX;
  float carY;
  uint8_t tail;
};
#pragma pack(pop)

static const uint8_t FRAME_HEAD = 0xB5;
static const uint8_t FRAME_TAIL = 0x5B;

// 假设这是我的热点，你要改成你的手机热点
const char* ssid     = "iPhone";
const char* password = "1234567890";

ESP8266WebServer server(80);

float carX = 0.00f;
float carY = 0.00f;
float targetX = 1.00f;
float targetY = 1.00f;
float pidKp = 1.20f;
float pidKi = 0.10f;
float pidKd = 0.05f;

String sendStatus = "等待发送参数";
String lastCommand = "还没有发送指令";

uint8_t rxBuffer[sizeof(CarToEspFrame)] = {0};
size_t rxIndex = 0;
bool waitingHead = true;

String escapeHtml(const String& text) {
  String escaped = text;
  escaped.replace("&", "&amp;");
  escaped.replace("<", "&lt;");
  escaped.replace(">", "&gt;");
  escaped.replace("\"", "&quot;");
  escaped.replace("'", "&#39;");
  return escaped;
}

String escapeJson(const String& text) {
  String escaped = "";

  for (unsigned int i = 0; i < text.length(); i++) {
    char c = text[i];
    if (c == '\\') {
      escaped += "\\\\";
    } else if (c == '"') {
      escaped += "\\\"";
    } else if (c == '\n') {
      escaped += "\\n";
    } else if (c == '\r') {
      escaped += "\\r";
    } else if (c == '\t') {
      escaped += "\\t";
    } else {
      escaped += c;
    }
  }

  return escaped;
}

String formatFloat(float value) {
  return String(value, 3);
}

String buildPidSummary() {
  return "Kp=" + formatFloat(pidKp) + "  Ki=" + formatFloat(pidKi) + "  Kd=" + formatFloat(pidKd);
}

String buildTargetSummary() {
  return "X=" + formatFloat(targetX) + "  Y=" + formatFloat(targetY);
}

void sendParameterCommand() {
  lastCommand = "SET_TARGET " +
                String("X=") + formatFloat(targetX) + " " +
                String("Y=") + formatFloat(targetY) + " " +
                String("KP=") + formatFloat(pidKp) + " " +
                String("KI=") + formatFloat(pidKi) + " " +
                String("KD=") + formatFloat(pidKd);

  EspToCarFrame tx;
  tx.head = FRAME_HEAD;
  tx.targetX = targetX;
  tx.targetY = targetY;
  tx.pidKp = pidKp;
  tx.pidKi = pidKi;
  tx.pidKd = pidKd;
  tx.tail = FRAME_TAIL;

  Serial.write((const uint8_t*)&tx, sizeof(tx));
  Serial.flush();

  Serial.println("[WEB] 参数帧已发送");
  sendStatus = "参数发送成功";
}

void processSerialFrame(const CarToEspFrame& frame) {
  carX = frame.carX;
  carY = frame.carY;
}

void processSerialRx() {
  while (Serial.available() > 0) {
    uint8_t b = (uint8_t)Serial.read();

    if (waitingHead) {
      if (b == FRAME_HEAD) {
        rxBuffer[0] = b;
        rxIndex = 1;
        waitingHead = false;
      }
      continue;
    }

    rxBuffer[rxIndex++] = b;

    if (rxIndex >= sizeof(CarToEspFrame)) {
      waitingHead = true;
      rxIndex = 0;

      CarToEspFrame rxFrame;
      memcpy(&rxFrame, rxBuffer, sizeof(CarToEspFrame));
      if (rxFrame.head == FRAME_HEAD && rxFrame.tail == FRAME_TAIL) {
        processSerialFrame(rxFrame);
        sendStatus = "串口状态已更新";
      }
    }
  }
}

void handleApply() {
  if (server.hasArg("targetX")) {
    targetX = server.arg("targetX").toFloat();
  }
  if (server.hasArg("targetY")) {
    targetY = server.arg("targetY").toFloat();
  }
  if (server.hasArg("pidKp")) {
    pidKp = server.arg("pidKp").toFloat();
  }
  if (server.hasArg("pidKi")) {
    pidKi = server.arg("pidKi").toFloat();
  }
  if (server.hasArg("pidKd")) {
    pidKd = server.arg("pidKd").toFloat();
  }

  sendParameterCommand();
  handleRoot();
}

void handleStatus() {
  String json = "{";
  json += "\"carX\":" + formatFloat(carX) + ",";
  json += "\"carY\":" + formatFloat(carY) + ",";
  json += "\"targetSummary\":\"" + escapeJson(buildTargetSummary()) + "\",";
  json += "\"pidSummary\":\"" + escapeJson(buildPidSummary()) + "\",";
  json += "\"sendStatus\":\"" + escapeJson(sendStatus) + "\",";
  json += "\"lastCommand\":\"" + escapeJson(lastCommand) + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void handleRoot() {
  String html = "<html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>小车无线调参</title>";
  html += "<style>body{font-family:Arial,Helvetica,sans-serif;margin:0;background:linear-gradient(180deg,#e0f2fe 0%,#f8fafc 100%);color:#0f172a;}";
  html += ".wrap{max-width:760px;margin:20px auto;padding:16px;}";
  html += ".card{background:#ffffff;border-radius:18px;padding:20px;margin-bottom:16px;box-shadow:0 12px 30px rgba(15,23,42,0.10);}";
  html += "h1,h2{margin:0 0 12px;}";
  html += ".coord{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:12px;}";
  html += ".metric{background:#eff6ff;border-radius:14px;padding:16px;}";
  html += ".metric-title{font-size:14px;color:#475569;}";
  html += ".metric-value{font-size:32px;font-weight:bold;color:#1d4ed8;margin-top:8px;}";
  html += ".meta{font-size:16px;line-height:1.8;word-break:break-word;}";
  html += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;}";
  html += "label{display:block;font-size:15px;font-weight:bold;margin:10px 0 8px;}";
  html += "input,button{width:100%;font-size:18px;border-radius:12px;box-sizing:border-box;}";
  html += "input{padding:12px;border:1px solid #cbd5e1;background:#fff;}";
  html += "button{padding:14px;border:none;color:#fff;font-weight:bold;cursor:pointer;margin-top:14px;}";
  html += ".primary{background:#2563eb;}";
  html += ".hint{font-size:14px;color:#64748b;margin-top:8px;}";
  html += "@media (max-width:640px){.coord,.grid{grid-template-columns:1fr;}}";
  html += "</style></head><body>";
  html += "<div class='wrap'>";
  html += "<div class='card'><h1>小车无线调参界面</h1>";
  html += "<div class='coord'>";
  html += "<div class='metric'><div class='metric-title'>当前 X 坐标</div><div id='carX' class='metric-value'>" + formatFloat(carX) + "</div></div>";
  html += "<div class='metric'><div class='metric-title'>当前 Y 坐标</div><div id='carY' class='metric-value'>" + formatFloat(carY) + "</div></div>";
  html += "</div></div>";
  html += "<div class='card'><h2>目标点设置</h2>";
  html += "<form method='POST' action='/apply'>";
  html += "<div class='grid'>";
  html += "<div><label for='targetX'>目标 X</label><input id='targetX' name='targetX' type='number' step='0.001' value='" + formatFloat(targetX) + "'></div>";
  html += "<div><label for='targetY'>目标 Y</label><input id='targetY' name='targetY' type='number' step='0.001' value='" + formatFloat(targetY) + "'></div>";
  html += "</div>";
  html += "<div class='hint'>输入浮点数目标点，发送后通过串口下发给小车。</div>";
  html += "<h2 style='margin-top:20px;'>PID 调参</h2>";
  html += "<div class='grid'>";
  html += "<div><label for='pidKp'>Kp</label><input id='pidKp' name='pidKp' type='number' step='0.001' value='" + formatFloat(pidKp) + "'></div>";
  html += "<div><label for='pidKi'>Ki</label><input id='pidKi' name='pidKi' type='number' step='0.001' value='" + formatFloat(pidKi) + "'></div>";
  html += "</div>";
  html += "<div><label for='pidKd'>Kd</label><input id='pidKd' name='pidKd' type='number' step='0.001' value='" + formatFloat(pidKd) + "'></div>";
  html += "<button class='primary' type='submit'>发送参数</button>";
  html += "</form></div>";
  html += "<div class='card'><h2>当前参数状态</h2>";
  html += "<div id='targetSummary' class='meta'>目标点：" + escapeHtml(buildTargetSummary()) + "</div>";
  html += "<div id='pidSummary' class='meta'>PID：" + escapeHtml(buildPidSummary()) + "</div>";
  html += "<div id='sendStatus' class='meta'>状态：" + escapeHtml(sendStatus) + "</div>";
  html += "<div id='lastCommand' class='meta'>最近发送：" + escapeHtml(lastCommand) + "</div>";
  html += "</div>";
  html += "</div>";
  html += "<script>";
  html += "function escapeHtml(text){return String(text||'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/\"/g,'&quot;').replace(/'/g,'&#39;');}";
  html += "function updateStatus(){fetch('/status').then(function(response){return response.json();}).then(function(data){document.getElementById('carX').innerHTML=data.carX;document.getElementById('carY').innerHTML=data.carY;document.getElementById('targetSummary').innerHTML='目标点：'+escapeHtml(data.targetSummary);document.getElementById('pidSummary').innerHTML='PID：'+escapeHtml(data.pidSummary);document.getElementById('sendStatus').innerHTML='状态：'+escapeHtml(data.sendStatus);document.getElementById('lastCommand').innerHTML='最近发送：'+escapeHtml(data.lastCommand);}).catch(function(){});}";
  html += "setInterval(updateStatus,1000);";
  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi OK");
  Serial.println("IP: " + WiFi.localIP().toString());

  server.on("/", handleRoot);
  server.on("/apply", HTTP_POST, handleApply);
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();
}

void loop() {
  server.handleClient();
  processSerialRx();
}
