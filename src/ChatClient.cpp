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

#ifndef CHATCLIENT_PURGE_CA_CERT_
ChatClient::ChatClient(const char* key) : _WiFiClient(), _API_KEY(key), _History(), _System() {
  constexpr static const char* root_ca = \
    "-----BEGIN CERTIFICATE-----\n" \
    "MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ\n" \
    "RTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJlclRydXN0MSIwIAYD\n" \
    "VQQDExlCYWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTAwMDUxMjE4NDYwMFoX\n" \
    "DTI1MDUxMjIzNTkwMFowWjELMAkGA1UEBhMCSUUxEjAQBgNVBAoTCUJhbHRpbW9y\n" \
    "ZTETMBEGA1UECxMKQ3liZXJUcnVzdDEiMCAGA1UEAxMZQmFsdGltb3JlIEN5YmVy\n" \
    "VHJ1c3QgUm9vdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKMEuyKr\n" \
    "mD1X6CZymrV51Cni4eiVgLGw41uOKymaZN+hXe2wCQVt2yguzmKiYv60iNoS6zjr\n" \
    "IZ3AQSsBUnuId9Mcj8e6uYi1agnnc+gRQKfRzMpijS3ljwumUNKoUMMo6vWrJYeK\n" \
    "mpYcqWe4PwzV9/lSEy/CG9VwcPCPwBLKBsua4dnKM3p31vjsufFoREJIE9LAwqSu\n" \
    "XmD+tqYF/LTdB1kC1FkYmGP1pWPgkAx9XbIGevOF6uvUA65ehD5f/xXtabz5OTZy\n" \
    "dc93Uk3zyZAsuT3lySNTPx8kmCFcB5kpvcY67Oduhjprl3RjM71oGDHweI12v/ye\n" \
    "jl0qhqdNkNwnGjkCAwEAAaNFMEMwHQYDVR0OBBYEFOWdWTCCR1jMrPoIVDaGezq1\n" \
    "BE3wMBIGA1UdEwEB/wQIMAYBAf8CAQMwDgYDVR0PAQH/BAQDAgEGMA0GCSqGSIb3\n" \
    "DQEBBQUAA4IBAQCFDF2O5G9RaEIFoN27TyclhAO992T9Ldcw46QQF+vaKSm2eT92\n" \
    "9hkTI7gQCvlYpNRhcL0EYWoSihfVCr3FvDB81ukMJY2GQE/szKN+OMY3EU/t3Wgx\n" \
    "jkzSswF07r51XgdIGn9w/xZchMB5hbgF/X++ZRGjD8ACtPhSNzkE1akxehi/oCr0\n" \
    "Epn3o0WC4zxe9Z2etciefC7IpJ5OCBRLbf1wbWsaY71k5h+3zvDyny67G7fyUIhz\n" \
    "ksLi4xaNmjICq44Y3ekQEe5+NauQrz4wlHrQMz2nZQ/1/I6eYs9HRCwBXbsdtTLS\n" \
    "R9I4LtD+gdwyah617jzV/OeBHRnDJELqYzmp\n" \
    "-----END CERTIFICATE-----\n";
  _WiFiClient.setCACert(root_ca);
}
#endif  // CHATCLIENT_PURGE_CA_CERT_

ChatClient::ChatClient(const char* key, const char* rootCA) : _WiFiClient(), _API_KEY(key), _History(), _System() {
  _WiFiClient.setCACert(rootCA);
}

void ChatClient::begin() {
  _System.clear();
  _History.clear();
  _MaxHistory = 5;
  _TimeOut = 0;
}

void ChatClient::Model(const char* model) {
  _Model = model;
}

bool ChatClient::Chat(const char* message, String& response, ChatUsage* usage) {
  HTTPClient http;
  if (_TimeOut > 0) {
    http.setTimeout(_TimeOut);
  }
  if (!http.begin(_WiFiClient, API_URL)) {
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + _API_KEY);
  const auto code = http.POST(MakePayload(message));
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  const auto payload = http.getString();
  http.end();

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

bool ChatClient::ChatStream(const char* message, void (*callback)(const char*)) {
  String response;
  return ChatStream(message, response, callback);
}

bool ChatClient::ChatStream(const char* message, String& response, void (*callback)(const char*)) {
  if (!_WiFiClient.connect(API_HOST, API_PORT)) {
    return false;
  }
  const auto payload = MakePayload(message, true);
  _WiFiClient.print(String("POST ") + API + " HTTP/1.1\r\n" +
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
    while (_WiFiClient.available()) {
      String line = _WiFiClient.readStringUntil('\n');
      if (!beginBody) {
        if (line.startsWith("HTTP/")) {
          const auto pos = line.indexOf(' ') + 1;
          httpResCode = line.substring(pos, line.indexOf(' ', pos)).toInt();
        } else if (line == "\r") {
          if (httpResCode != HTTP_CODE_OK) {
            _WiFiClient.stop(); 
            return false;
          }
          beginBody = true;
        }
      } else if (line.startsWith("data: ")) {
        const auto body = line.substring(6);
        if (body == "[DONE]") {
          _WiFiClient.stop(); 
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
  _WiFiClient.stop(); 
  return false;
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
