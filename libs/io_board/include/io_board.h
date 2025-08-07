#pragma once
#include "mgos.h"
#include "mgos_shadow.h"
#include <string>

#define IOBOARD_NUM_RELAYS 7
#define IOBOARD_NUM_INPUTS 6


class IoBoard {
 public:
  static IoBoard &getInstance();

  void init();
  void reportState();
  void reportRelayState(int index);
  void handleShadowEvent(int ev, void *ev_data);
  static void shadowHandlerStatic(int ev, void *ev_data, void *userdata);

  void setRelay(int index, bool on);
  bool getRelay(int index) const;
  bool getInput(int index) const;
  void toggleRelay(int index);
  void turnOffAllRelays();
  void pulseRelay(int index, int duration_ms);

  void syncRelayShadowViaMQTT(int index, bool state);

  void applyDelta(struct mg_str *delta);
  void reportRelays();
  void reportInputs(struct json_out *out);
  void reportAnalog(struct json_out *out);
  void reportBuzzer(struct json_out *out);
  void reportBuzzerState();
  void setBuzzer(bool on);
  bool getBuzzer() const;

  const int relayPins[IOBOARD_NUM_RELAYS] = {13, 12, 14, 27, 26, 15, 2};
  const int inputPins[IOBOARD_NUM_INPUTS] = {25, 33, 32, 35, 39, 36};
  const int analogPin = 34;
  const int buzzerPin = 4;

 private:
  IoBoard();
  

  bool relays[IOBOARD_NUM_RELAYS];
  bool inputs[IOBOARD_NUM_INPUTS];
  int analogValue;
  bool buzzer;

  mgos_timer_id debounceTimers[IOBOARD_NUM_INPUTS];
};
