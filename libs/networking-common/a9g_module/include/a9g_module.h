#pragma once
#include "mgos.h"
#include <functional>

using HttpCallback = std::function<void(const char *response)>;

#ifdef __cplusplus
extern "C" {
#endif

bool mgos_a9g_module_init(void);

#ifdef __cplusplus
}
#endif

class A9GModule {
 public:
  static A9GModule &getInstance();

  void initUART();
  void handleGPSByte(char c);
  void processLine(const char *line);
  void parseGPRMC(const char *line);
  void reportGPS();
  void sendATCommand(const char *cmd);
  void queryIMEI();
  void queryPhoneNumber();
  void parseIMEIResponse(const char *line);
  void parseCNUMResponse(const char *line);
  void reportDeviceInfo();
  void queryNetworkRegistration();
  void parseCREGResponse(const char *line);
  void reportNetworkStatus();
  void queryGPRSAttachment();
  void parseCGATTResponse(const char *line);
  void setAPN(const char *apn);
  void reportGPRSStatus();
  void enableGPRS();
  void setupHTTP(const char *url);
  void httpGET();
  void readHTTPResponse();
  void sendHTTPGET(const char *url);
  void sendHTTPGET(const char *url, HttpCallback cb);
  void sendHTTPPOST(const char *url, const char *body, HttpCallback cb);
  void reportAllStatus();

 private:
  A9GModule();
  static void uartDispatch(int uart_no, void *arg);

  char lineBuffer[128];
  int lineLen;

  // GPS data
  bool gpsFixValid;
  float gpsLat;
  float gpsLon;
  float speedKnots;
  float courseDeg;
  char timestampUTC[24];
  char imei[32];
  char phoneNumber[32];
  bool networkRegistered;
  bool gprsAttached;
  bool apnConfigured;

  public:
  HttpCallback httpCallback;

  
};
