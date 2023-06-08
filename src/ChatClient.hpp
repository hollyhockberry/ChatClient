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
  WiFiClientSecure _WiFiClient;
  const String _API_KEY;
  std::vector<String> _History;
  int _MaxHistory = 5;
  uint16_t _TimeOut = 0;
 
 public:
  ChatClient(const char* key, const char* rootCA);
  ChatClient(const char* key);

  void begin();

  bool Chat(const char* message, String& response, ChatUsage* usage = nullptr);

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
  String MakePayload(const char* msg) const;
  void AddHistory(String& message, String& response);
  void PurgeHistory();
};

#endif  // CHATCLIENT_H_
