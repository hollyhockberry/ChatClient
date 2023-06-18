// Copyright (c) 2023 Inaba (@hollyhockberry)
// This software is released under the MIT License.
// http://opensource.org/licenses/mit-license.php

#ifndef CHATCLIENT_H_
#define CHATCLIENT_H_

#include <vector>
#include <WiFiClientSecure.h>
#include <WString.h>

struct ChatUsage {
  int prompt_tokens;
  int completion_tokens;
  int total_tokens;
};

class ChatClient {
  const char* _API_KEY;
  const char* _rootCACertificate;
  std::vector<String> _System;
  std::vector<String> _History;
  int _MaxHistory = 5;
  uint16_t _TimeOut = 0;
  String _Model = "gpt-3.5-turbo";
 
 public:
  ChatClient(const char* key, const char* rootCACertificate = nullptr);

  void begin();

  void Model(const char* model);

  void ClearSystem();
  void AddSystem(const char* content);
  
  bool Chat(const char* message, String& response, ChatUsage* usage = nullptr);
  bool ChatStream(const char* message, void (*callback)(const char*) = nullptr);
  bool ChatStream(const char* message, String& response, void (*callback)(const char*) = nullptr);

  uint16_t TimeOut() const {
    return _TimeOut;
  }
  void TimeOut(uint16_t msec) {
    _TimeOut = msec;
  }

  int MaxHistory() const {
    return _MaxHistory;
  }
  void MaxHistory(int size);
  void ClearHistory();

 private:
  bool ChatStream(WiFiClientSecure& client, const char* message, String& response, void (*callback)(const char*));

  String MakePayload(const char* msg, bool isStream = false) const;
  void AddHistory(String& message, String& response);
  void PurgeHistory();
};

#endif  // CHATCLIENT_H_
