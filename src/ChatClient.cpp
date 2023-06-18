// Copyright (c) 2023 Inaba (@hollyhockberry)
// This software is released under the MIT License.
// http://opensource.org/licenses/mit-license.php

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "ChatClient.hpp"

namespace {
constexpr static const char* API_HOST = "api.openai.com";
constexpr static int API_PORT = 443;
constexpr static const char* API = "/v1/chat/completions";
constexpr static const char* API_URL = "https://api.openai.com/v1/chat/completions";
}  // namespace

ChatClient::ChatClient() : ChatClient(nullptr, nullptr) {
}

ChatClient::ChatClient(const char* key) : ChatClient(key, nullptr) {
}

ChatClient::ChatClient(const char* key, const char* rootCACertificate)
: _API_KEY(key), _rootCACertificate(rootCACertificate), _History(), _System() {
}

void ChatClient::begin() {
  _System.clear();
  _History.clear();
  _MaxHistory = 5;
  _TimeOut = 0;
}

void ChatClient::ApiKey(const char* key) {
  _API_KEY = key;
}

const char* ChatClient::ApiKey() const {
  return _API_KEY;
}

void ChatClient::Model(const char* model) {
  _Model = model;
}

bool ChatClient::Chat(const char* message, String& response, ChatUsage* usage) {
  if (_API_KEY == nullptr) {
    return false;
  }
  auto client = new WiFiClientSecure();
  auto http = new HTTPClient();

  if (_rootCACertificate) {
    client->setCACert(_rootCACertificate);
  } else {
    client->setInsecure();
  }
  if (_TimeOut > 0) {
    http->setTimeout(_TimeOut);
  }
  if (!http->begin(*client, API_URL)) {
    delete http;
    delete client;
    return false;
  }
  http->addHeader("Content-Type", "application/json");
  http->addHeader("Authorization", String("Bearer ") + _API_KEY);

  const auto code = http->POST(MakePayload(message));
  const auto payload = http->getString();
  http->end();
  delete http;
  delete client;

  if (code != HTTP_CODE_OK) {
    return false;
  }
  DynamicJsonDocument doc(1024);
  if (::deserializeJson(doc, payload.c_str())) {
    return false;
  }
  const char* content = doc["choices"][0]["message"]["content"];
  if (content == nullptr) {
    return false;
  }
  response = content;
  String m = message;
  AddHistory(m, response);

  if (usage) {
    usage->prompt_tokens = doc["usage"]["prompt_tokens"];
    usage->completion_tokens = doc["usage"]["completion_tokens"];
    usage->total_tokens = doc["usage"]["total_tokens"];
  }
  return true;
}

bool ChatClient::ChatStream(WiFiClientSecure& client, const char* message, String& response, void (*callback)(const char*)) {
  if (_API_KEY == nullptr) {
    return false;
  }
  if (_rootCACertificate) {
    client.setCACert(_rootCACertificate);
  } else {
    client.setInsecure();
  }
  if (!client.connect(API_HOST, API_PORT)) {
    return false;
  }
  const auto payload = MakePayload(message, true);
  client.print(String("POST ") + API + " HTTP/1.1\r\n" +
               "Host: " + API_HOST + "\r\n" +
               "Authorization: Bearer " + _API_KEY + "\r\n" +
               "Content-Type: application/json\r\n" +
               "Content-Length: " + payload.length() + "\r\n\r\n" +
               payload);

  int httpResCode = -1;
  bool beginBody = false;
  response = "";
  auto period = ::millis();
  while(_TimeOut <= 0 || (::millis() - period) < _TimeOut) {
    ::delay(100);
    while (client.available()) {
      String line = client.readStringUntil('\n');
      if (!beginBody) {
        if (line.startsWith("HTTP/")) {
          const auto pos = line.indexOf(' ') + 1;
          httpResCode = line.substring(pos, line.indexOf(' ', pos)).toInt();
        } else if (line == "\r") {
          if (httpResCode != HTTP_CODE_OK) {
            client.stop(); 
            return false;
          }
          beginBody = true;
        }
      } else if (line.startsWith("data: ")) {
        const auto body = line.substring(6);
        if (body == "[DONE]") {
          client.stop(); 
          String m = message;
          AddHistory(m, response);
          return true;
        }
        DynamicJsonDocument doc(1024);
        auto error = ::deserializeJson(doc, body.c_str());
        const char* data = doc["choices"][0]["delta"]["content"];
        if (data) {
          response += data;
          if (callback) {
            callback(data);
          }
        }
      }
      period = ::millis();
    }
  }
  client.stop(); 
  return false;
}

bool ChatClient::ChatStream(const char* message, void (*callback)(const char*)) {
  String response;
  return ChatStream(message, response, callback);
}

bool ChatClient::ChatStream(const char* message, String& response, void (*callback)(const char*)) {
  auto client = new WiFiClientSecure();
  const auto ret = ChatStream(*client, message, response, callback);
  delete client;
  return ret;
}

namespace {
void AddMessage(JsonArray& array, const char* role, const char* content) {
  auto message = array.createNestedObject();
  message["role"] = role;
  message["content"] = content;
}
}  // namespace

String ChatClient::MakePayload(const char* msg, bool isStream) const {
  static StaticJsonDocument<4096> doc;
  doc["model"] = _Model;
  if (isStream) {
    doc["stream"] = true;    
  }
  auto messages = doc.createNestedArray("messages");
  bool user = true;
  for (const auto& s : _System) {
    AddMessage(messages, "system", s.c_str());
  }
  for (const auto& h : _History) {
    AddMessage(messages, user ? "user" : "assistant", h.c_str());
    user = !user;
  }
  AddMessage(messages, "user", msg);

  String payload;
  ::serializeJson(doc, payload);
  return payload;
}

void ChatClient::MaxHistory(int size) {
  if (_MaxHistory != size) {
    _MaxHistory = size;
    PurgeHistory();
  }
}

void ChatClient::ClearHistory() {
  _History.clear();
}

void ChatClient::AddHistory(String& message, String& response) {
  _History.push_back(message);
  _History.push_back(response);
  PurgeHistory();
}

void ChatClient::PurgeHistory() {
  auto overflow = static_cast<int>(_History.size()) - static_cast<int>(_MaxHistory * 2);
  if (overflow > 0) {
    _History.erase(_History.begin(), _History.begin() + overflow);
  }
}

void ChatClient::ClearSystem() {
  _System.clear();
}

void ChatClient::AddSystem(const char* content) {
  _System.push_back(content);
}
