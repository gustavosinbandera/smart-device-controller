#include "a9g_module.h"
#include "mgos_uart.h"
#include "mgos_shadow.h"
#include "mgos_system.h"
#include "frozen.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <functional>
#include <string>


A9GModule &A9GModule::getInstance() {
  static A9GModule instance;
  return instance;
}

A9GModule::A9GModule()
  : lineLen(0),
    gpsFixValid(false),
    gpsLat(0.0),
    gpsLon(0.0),
    speedKnots(0.0),
    courseDeg(0.0),
    networkRegistered(false),
    gprsAttached(false),
    apnConfigured(false),
    httpCallback(nullptr) {
  memset(lineBuffer, 0, sizeof(lineBuffer));
  timestampUTC[0] = 0;
  imei[0] = '\0';
  phoneNumber[0] = '\0';
}

void A9GModule::initUART() {
  mgos_uart_set_dispatcher(1, uartDispatch, this);
  mgos_uart_set_rx_enabled(1, true);
}

void A9GModule::uartDispatch(int uart_no, void *arg) {
  A9GModule *self = static_cast<A9GModule *>(arg);
  char c;
  while (mgos_uart_read(uart_no, &c, 1) > 0) {
    self->handleGPSByte(c);
  }
}

void A9GModule::handleGPSByte(char c) {
  if (c == '\n') {
    lineBuffer[lineLen] = '\0';
    processLine(lineBuffer);
    lineLen = 0;
  } else if (lineLen < (int)(sizeof(lineBuffer) - 1)) {
    lineBuffer[lineLen++] = c;
  } else {
    lineLen = 0;
  }
}

void A9GModule::processLine(const char *line) {
  if (strstr(line, "$GPRMC")) {
    parseGPRMC(line);
  } else if (strstr(line, "+CNUM:")) {
    parseCNUMResponse(line);
  } else if (isdigit(line[0]) && strlen(line) > 10) {
    parseIMEIResponse(line);
  } else if (strstr(line, "+CREG:")) {
    parseCREGResponse(line);
  } else if (strstr(line, "+CGATT:")) {
    parseCGATTResponse(line);
  } else if (strstr(line, "+HTTPREAD:")) {
    // Ignorar encabezado
  } else if (httpCallback && strstr(line, "HTTP/") == nullptr && strstr(line, "+HTTPREAD:") == nullptr) {
    httpCallback(line);
    httpCallback = nullptr;
  }
}

static float nmeaToDecimal(const char *coord, const char dir) {
  if (!coord || strlen(coord) < 4) return 0.0;
  float val = atof(coord);
  int deg = (int)(val / 100);
  float min = val - deg * 100;
  float decimal = deg + (min / 60.0);
  if (dir == 'S' || dir == 'W') decimal = -decimal;
  return decimal;
}

void A9GModule::parseGPRMC(const char *line) {
  char fields[12][20] = {{0}};
  int fieldIndex = 0, charIndex = 0;

  for (int i = 0; line[i] != '\0' && fieldIndex < 12; ++i) {
    if (line[i] == ',') {
      fields[fieldIndex][charIndex] = '\0';
      fieldIndex++;
      charIndex = 0;
    } else if (charIndex < 19) {
      fields[fieldIndex][charIndex++] = line[i];
    }
  }

  gpsFixValid = (fields[2][0] == 'A');
  if (!gpsFixValid) return;

  gpsLat = nmeaToDecimal(fields[3], fields[4][0]);
  gpsLon = nmeaToDecimal(fields[5], fields[6][0]);
  speedKnots = atof(fields[7]);
  courseDeg = atof(fields[8]);

  snprintf(timestampUTC, sizeof(timestampUTC), "20%.2s-%.2s-%.2sT%.2s:%.2s:%.2sZ",
           fields[9]+4, fields[9]+2, fields[9],
           fields[1], fields[1]+2, fields[1]+4);

  reportGPS();
}

void A9GModule::reportGPS() {
  if (!gpsFixValid) return;

  char json[256];
  snprintf(json, sizeof(json),
    "{\"gps\":{\"fix\":true,\"timestamp_utc\":\"%s\","
    "\"latitude\":%.6f,\"longitude\":%.6f,"
    "\"speed_knots\":%.2f,\"course_deg\":%.2f}}",
    timestampUTC, gpsLat, gpsLon, speedKnots, courseDeg);

  mgos_shadow_updatef(0, "%s", json);
}

void A9GModule::sendATCommand(const char *cmd) {
  mgos_uart_write(1, cmd, strlen(cmd));
  mgos_uart_write(1, "\r\n", 2);
}

void A9GModule::queryIMEI() {
  sendATCommand("AT+GSN");
}

void A9GModule::queryPhoneNumber() {
  sendATCommand("AT+CNUM");
}

void A9GModule::parseIMEIResponse(const char *line) {
  if (strlen(line) > 10 && isdigit(line[0])) {
    strncpy(imei, line, sizeof(imei) - 1);
    imei[sizeof(imei) - 1] = '\0';
    reportDeviceInfo();
  }
}

void A9GModule::parseCNUMResponse(const char *line) {
  const char *quote = strchr(line, '"');
  if (quote) {
    const char *next = strchr(quote + 1, '"');
    if (next && (next - quote - 1) < (int)sizeof(phoneNumber)) {
      strncpy(phoneNumber, quote + 1, next - quote - 1);
      phoneNumber[next - quote - 1] = '\0';
      reportDeviceInfo();
    }
  }
}

void A9GModule::reportDeviceInfo() {
  if (imei[0] == '\0' && phoneNumber[0] == '\0') return;

  char json[128];
  snprintf(json, sizeof(json),
    "{\"device\":{\"imei\":\"%s\",\"phone_number\":\"%s\"}}",
    imei, phoneNumber);

  mgos_shadow_updatef(0, "%s", json);
}

void A9GModule::queryNetworkRegistration() {
  sendATCommand("AT+CREG?");
}

void A9GModule::parseCREGResponse(const char *line) {
  int n = -1, stat = -1;
  if (sscanf(line, "+CREG: %d,%d", &n, &stat) == 2) {
    networkRegistered = (stat == 1 || stat == 5);
    reportNetworkStatus();
  }
}

void A9GModule::reportNetworkStatus() {
  char json[64];
  snprintf(json, sizeof(json),
           "{\"network\":{\"registered\":%s}}",
           networkRegistered ? "true" : "false");

  mgos_shadow_updatef(0, "%s", json);
}

void A9GModule::queryGPRSAttachment() {
  sendATCommand("AT+CGATT?");
}

void A9GModule::parseCGATTResponse(const char *line) {
  int status = -1;
  if (sscanf(line, "+CGATT: %d", &status) == 1) {
    gprsAttached = (status == 1);
    reportGPRSStatus();
  }
}

void A9GModule::setAPN(const char *apn) {
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", apn);
  sendATCommand(cmd);
  apnConfigured = true;
}

void A9GModule::reportGPRSStatus() {
  char json[64];
  snprintf(json, sizeof(json),
           "{\"gprs\":{\"attached\":%s}}",
           gprsAttached ? "true" : "false");

  mgos_shadow_updatef(0, "%s", json);
}

void A9GModule::enableGPRS() {
  sendATCommand("AT+CGATT=1");
}

void A9GModule::setupHTTP(const char *url) {
  sendATCommand("AT+HTTPTERM");
  sendATCommand("AT+HTTPINIT");
  sendATCommand("AT+HTTPPARA=\"CID\",1");

  char cmd[128];
  snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
  sendATCommand(cmd);
}

void A9GModule::httpGET() {
  sendATCommand("AT+HTTPACTION=0");
}

void A9GModule::readHTTPResponse() {
  sendATCommand("AT+HTTPREAD");
}

// HTTP GET con callback
struct HttpGetCtx {

  std::string url;
  HttpCallback cb;
};

static void _http_get_step4(void *arg) {
  auto *ctx = static_cast<HttpGetCtx *>(arg);
  A9GModule::getInstance().httpCallback = ctx->cb;
  A9GModule::getInstance().sendATCommand("AT+HTTPREAD");
  delete ctx;
}

static void _http_get_step3(void *arg) {
  A9GModule::getInstance().sendATCommand("AT+HTTPACTION=0");
  mgos_set_timer(2000, 0, _http_get_step4, arg);
}

static void _http_get_step2(void *arg) {
  auto *ctx = static_cast<HttpGetCtx *>(arg);
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", ctx->url.c_str());
  A9GModule::getInstance().sendATCommand(cmd);
  mgos_set_timer(1000, 0, _http_get_step3, arg);
}

void A9GModule::sendHTTPGET(const char *url, HttpCallback cb) {
  auto *ctx = new HttpGetCtx{url, cb};
  setAPN("internet.comcel.com.co");
  sendATCommand("AT+CGATT=1");
  sendATCommand("AT+HTTPTERM");
  sendATCommand("AT+HTTPINIT");
  sendATCommand("AT+HTTPPARA=\"CID\",1");
  mgos_set_timer(1000, 0, _http_get_step2, ctx);
}

// HTTP POST con callback
struct HttpPostCtx {
  std::string url;
  std::string body;
  HttpCallback cb;
};

static void _http_post_step5(void *arg) {
  auto *ctx = static_cast<HttpPostCtx *>(arg);
  A9GModule::getInstance().httpCallback = ctx->cb;
  A9GModule::getInstance().sendATCommand("AT+HTTPREAD");
  delete ctx;
}

static void _http_post_step4(void *arg) {
  A9GModule::getInstance().sendATCommand("AT+HTTPACTION=1");
  mgos_set_timer(3000, 0, _http_post_step5, arg);
}

static void _http_post_step3(void *arg) {
  auto *ctx = static_cast<HttpPostCtx *>(arg);
  A9GModule::getInstance().sendATCommand(ctx->body.c_str());
  mgos_set_timer(1000, 0, _http_post_step4, arg);
}

static void _http_post_step2(void *arg) {
  auto *ctx = static_cast<HttpPostCtx *>(arg);
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "AT+HTTPDATA=%d,10000", (int)ctx->body.size());
  A9GModule::getInstance().sendATCommand(cmd);
  mgos_set_timer(1000, 0, _http_post_step3, arg);
}

static void _http_post_step1(void *arg) {
  auto *ctx = static_cast<HttpPostCtx *>(arg);
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", ctx->url.c_str());
  A9GModule::getInstance().sendATCommand(cmd);
  mgos_set_timer(1000, 0, _http_post_step2, arg);
}

void A9GModule::sendHTTPPOST(const char *url, const char *body, HttpCallback cb) {
  auto *ctx = new HttpPostCtx{url, body, cb};
  setAPN("internet.comcel.com.co");
  sendATCommand("AT+CGATT=1");
  sendATCommand("AT+HTTPTERM");
  sendATCommand("AT+HTTPINIT");
  sendATCommand("AT+HTTPPARA=\"CID\",1");
  sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  mgos_set_timer(1000, 0, _http_post_step1, ctx);
}

void A9GModule::reportAllStatus() {
  reportGPS();
  reportDeviceInfo();
  reportNetworkStatus();
  reportGPRSStatus();
}


extern "C" {
bool mgos_a9g_module_init(void) {
   A9GModule::getInstance().initUART();
//   A9GModule::getInstance().sendHTTPGET("http://example.com", [](const char *resp) {
//     LOG(LL_INFO, ("HTTP GET Response: %s", resp));
//   });

    mgos_set_timer(60000, MGOS_TIMER_REPEAT, [](void *) {
    A9GModule::getInstance().reportAllStatus();
    }, nullptr);

    return true;
}
}
