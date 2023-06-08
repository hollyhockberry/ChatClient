// Copyright (c) 2023 Inaba (@hollyhockberry)
// This software is released under the MIT License.
// http://opensource.org/licenses/mit-license.php

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "ChatClient.hpp"

namespace {
constexpr static const char* API_URL = "https://api.openai.com/v1/chat/completions";
}  // namespace

ChatClient::ChatClient(const char* key) : _WiFiClient(), _API_KEY(key), _History() {
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

ChatClient::ChatClient(const char* key, const char* rootCA) : _WiFiClient(), _API_KEY(key), _History() {
  _WiFiClient.setCACert(rootCA);
}

void ChatClient::begin() {
  _History.clear();
  _MaxHistory = 5;
  _TimeOut = 0;
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

String ChatClient::MakePayload(const char* msg) const {
  StaticJsonDocument<1024> doc;
  doc["model"] = "gpt-3.5-turbo";
  auto messages = doc["messages"];
  bool user = true;
  for (const auto& h : _History) {
    auto message = messages.createNestedObject();
    message["role"] = user ? "user" : "assistant";
    message["content"] = h;
    user = !user;
  }
  auto message = messages.createNestedObject();
  message["role"] = "user";
  message["content"] = msg;

  String payload;
  ::serializeJson(doc, payload);

  Serial.println(payload);
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
