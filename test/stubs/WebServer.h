#pragma once
#include <functional>
#include "Arduino.h"
#include "Update.h"
enum HTTPMethod { HTTP_GET, HTTP_POST };
enum HTTPUploadStatus { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
struct HTTPUpload { HTTPUploadStatus status; String filename; uint8_t* buf; size_t currentSize; size_t totalSize; };
class WebServer {
 public:
  typedef std::function<void(void)> THandlerFunction;
  WebServer(int port);
  void on(const char*, HTTPMethod, THandlerFunction);
  void on(const char*, HTTPMethod, THandlerFunction, THandlerFunction);
  void onNotFound(THandlerFunction);
  void begin();
  void handleClient();
  void send(int, const char*, const char*);
  void send(int, const char*, const String&);
  void send_P(int, const char*, const char*);
  void sendHeader(const char*, const char*, bool first = false);
  String arg(const char*);
  bool hasArg(const char*);
  HTTPUpload& upload();
};
