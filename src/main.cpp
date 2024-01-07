#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebSocketsClient.h>
#include <FS.h>
#include <ArduinoJson.h>

const char *ssid = "YourSSID";
const char *password = "YourWiFiPass";
const char *token = "YourDiscordBotToken";

unsigned long heartbeatInterval = 0;
unsigned long lastHeartbeat = 0;

unsigned long lastSequenceNumber = 0;
bool awaitingHeartbeatAck = false;

bool isSentIdentify = false;
bool isReady = false;
String sessionID;

bool switchOn = false;
short switchTimer = 0;
long logTimer = 0;

String channelID = "887901480470851587";
String botID = "887848332972683266";

WebSocketsClient webSocket;

void sendMessage(String content)
{
  HTTPClient http;
  String url = "https://discord.com/api/v9/channels/" + channelID + "/messages";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bot " + String(token));

  String payload = "{\"content\":\"" + content + "\"}";
  int httpResponseCode = http.POST(payload);
  http.end();
}

void sendEmbed(String title, String description) {
  HTTPClient http;
  String url = "https://discord.com/api/v9/channels/" + channelID + "/messages";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bot " + String(token));

  String payload = "{";
  payload += "\"content\": \"\",";
  payload += "\"embeds\": [";
  payload += "{";

  payload += "\"title\": \"";
  payload += title;
  payload += "\",";

  payload += "\"description\": \"";
  payload += description;
  payload += "\",";

  payload += "\"color\": 5620992";
  payload += "}";
  payload += "]";
  payload += "}";
  Serial.println(payload);
  int httpResponseCode = http.POST(payload);
  http.end();
}

void sendIdentify()
{
  Serial.println("sendIdentify");
  if (!isReady)
  {
    String msg = "{\"op\": 2, \"d\": {\"token\": \"";
    msg += token;
    msg += "\", \"intents\": 4608, \"properties\": {\"$os\": \"esp32\", \"$browser\": \"esp32\", \"$device\": \"esp32\"}}}";
    webSocket.sendTXT(msg);
    sendMessage(":white_check_mark: Restarted");
  }
}

void sendResume()
{
  String msg = "{\"op\": 6, \"d\": {\"token\": \"";
  msg += token;
  msg += "\", \"session_id\": \"";
  msg += sessionID;
  msg += "\", \"seq\": ";
  msg += lastSequenceNumber;
  msg += "}}";
  webSocket.sendTXT(msg);
}

void sendHeartbeat()
{
  String msg = "{\"op\": 1, \"d\": ";
  msg += String(lastSequenceNumber);
  msg += "}";
  webSocket.sendTXT(msg);
  awaitingHeartbeatAck = true;
  lastHeartbeat = millis();
}

void reconnect()
{
  if (isReady)
  {
    sendResume();
  }
  else
  {
    webSocket.beginSSL("gateway.discord.gg", 443, "/");
  }
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  String msg = "";
  String op1 = "";
  DynamicJsonDocument doc(8192);

  switch (type)
  {
  case WStype_DISCONNECTED:
    sendMessage(":no_entry: BOT Disconnected!");
    reconnect();
    break;
  case WStype_CONNECTED:
    msg = "{\"op\": 2, \"d\": {\"token\": \"";
    msg += token;
    msg += "\", \"intents\": 4608, \"properties\": {\"$os\": \"esp32\", \"$browser\": \"esp32\", \"$device\": \"esp32\"}}}";
    webSocket.sendTXT(msg);
    sendMessage(":white_check_mark: Wi-Fi Connected!");
    break;

  case WStype_TEXT:
    Serial.println("text");
    deserializeJson(doc, payload);

    if (doc["op"].as<int>() == 10)
    {
      heartbeatInterval = doc["d"]["heartbeat_interval"].as<unsigned long>();
      delay(heartbeatInterval * random(0.0, 1.0));
      sendHeartbeat();
    }
    else if (doc["op"].as<int>() == 11)
    {
      Serial.println("heartbeat ack");
      awaitingHeartbeatAck = false;
      if (!isSentIdentify)
        sendIdentify();
    }
    else if (doc["op"].as<int>() == 1)
    {
      sendHeartbeat();
    }
    else if (doc["op"].as<int>() == 0)
    {
      String eventType = doc["t"].as<String>();
      if (eventType == "READY")
      {
        isReady = true;
        sessionID = doc["d"]["session_id"].as<String>();
      }
      else if (eventType = "MESSAGE_CREATE")
      {
        String content = doc["d"]["content"].as<String>();
        if (content != NULL)
        {
          String msg_authorID = doc["d"]["author"]["id"].as<String>();
          String msg_channelID = doc["d"]["channel_id"].as<String>();
          if (msg_authorID != botID && msg_channelID == channelID)
          {
            if(content == "set on"){
              sendEmbed(":white_check_mark: スイッチを入れました", "短押し");
              switchOn = true;
              logTimer = millis();
              switchTimer = 1000;
              digitalWrite(14, LOW);
            } else if (content == "set long") {
              sendEmbed(":white_check_mark: スイッチを入れました", "長押し");
              switchOn = true;
              logTimer = millis();
              switchTimer = 8000;
              digitalWrite(14, LOW);
            } else if (content == "reset") {
              esp_restart();
            } else {
              sendMessage(":woozy_face: そんなコマンドはないよ");
            }
          }
        }
      }
    }
    break;
  }
}

void setup()
{
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  switchOn = false;
  logTimer = millis();

  pinMode(14, OUTPUT);
  digitalWrite(14, LOW);
  delay(5000);
  digitalWrite(14, HIGH);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
  }
  webSocket.beginSSL("gateway.discord.gg", 443, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(1000);
}

void loop()
{
  webSocket.loop();
  if (heartbeatInterval > 0 && millis() - lastHeartbeat > heartbeatInterval)
  {
    if (awaitingHeartbeatAck)
    {
      reconnect();
    }
    else
    {
      sendHeartbeat();
    }
  } if (switchOn && (millis() - logTimer) > switchTimer) {
    switchOn = false;
    switchTimer = 0;
    digitalWrite(14, HIGH);
  }
}