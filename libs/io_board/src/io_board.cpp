#include "io_board.h"
#include "frozen.h"
#include "mgos_adc.h"
#include "mgos_aws_shadow.h"
#include "mgos_mqtt.h"


bool shadow_update_suppressed = false;

IoBoard &IoBoard::getInstance() {
  static IoBoard instance;
  return instance;
}

IoBoard::IoBoard() {
  for (int i = 0; i < IOBOARD_NUM_RELAYS; ++i) relays[i] = false;
  for (int i = 0; i < IOBOARD_NUM_INPUTS; ++i) {
    inputs[i] = false;
    debounceTimers[i] = MGOS_INVALID_TIMER_ID;
  }
  analogValue = 0;
  buzzer = false;
}

void IoBoard::init() {
  for (int i = 0; i < IOBOARD_NUM_RELAYS; ++i) {
    mgos_gpio_set_mode(relayPins[i], MGOS_GPIO_MODE_OUTPUT);
    mgos_gpio_write(relayPins[i], 0);
  }

  for (int i = 0; i < IOBOARD_NUM_INPUTS; ++i) {
    mgos_gpio_set_mode(inputPins[i], MGOS_GPIO_MODE_INPUT);
    mgos_gpio_set_pull(inputPins[i], MGOS_GPIO_PULL_UP);
    inputs[i] = mgos_gpio_read(inputPins[i]);

    mgos_gpio_set_int_handler(inputPins[i], MGOS_GPIO_INT_EDGE_ANY,
      [](int pin, void *arg) {
        IoBoard &board = IoBoard::getInstance();
        for (int j = 0; j < IOBOARD_NUM_INPUTS; ++j) {
          if (board.inputPins[j] == pin) {
            if (board.debounceTimers[j] != MGOS_INVALID_TIMER_ID) return;

            board.debounceTimers[j] = mgos_set_timer(50, 0, [](void *arg2) {
              int index = (intptr_t)arg2;
              IoBoard &b = IoBoard::getInstance();
              bool new_val = mgos_gpio_read(b.inputPins[index]);
              if (new_val != b.inputs[index]) {
                b.inputs[index] = new_val;
                b.reportState();
              }
              b.debounceTimers[index] = MGOS_INVALID_TIMER_ID;
            }, (void *)(intptr_t)j);

            break;
          }
        }
      }, NULL);

    mgos_gpio_enable_int(inputPins[i]);
  }

  mgos_gpio_set_mode(buzzerPin, MGOS_GPIO_MODE_OUTPUT);
  mgos_gpio_write(buzzerPin, 0);

  mgos_event_add_group_handler(MGOS_SHADOW_BASE, shadowHandlerStatic, this);

  // Reporte completo cada minuto
  mgos_set_timer(60000, MGOS_TIMER_REPEAT, [](void *) {
    IoBoard::getInstance().reportState();
  }, nullptr);
}

void IoBoard::shadowHandlerStatic(int ev, void *ev_data, void *userdata) {
  ((IoBoard *)userdata)->handleShadowEvent(ev, ev_data);
}

void IoBoard::handleShadowEvent(int ev, void *ev_data) {
  struct mg_str *delta;
  switch (ev) {
    case MGOS_SHADOW_CONNECTED:
      reportState();
      break;
    case MGOS_SHADOW_UPDATE_DELTA:
      delta = (struct mg_str *)ev_data;
      applyDelta(delta);
      reportState();
      break;
  }
}

void IoBoard::applyDelta(struct mg_str *delta) {
  bool val;

  /* ---------- buzzer ---------- */
  if (json_scanf(delta->p, delta->len, "{outputs:{buzzer:%B}}", &val) == 1) {
    setBuzzer(val);
  }


  /* ---------- relays ---------- */
  for (int i = 0; i < IOBOARD_NUM_RELAYS; ++i) {
    char fmt[40];
    snprintf(fmt, sizeof(fmt),
             "{outputs:{relays:{relay%d:%%B}}}", i + 1);
    if (json_scanf(delta->p, delta->len, fmt, &val) == 1) {
      relays[i] = val;
      shadow_update_suppressed = true;
      setRelay(i, val);
      shadow_update_suppressed = false;
    }
  }
}


void IoBoard::reportRelays() {
  char buf[512];
  struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));

  json_printf(&out, "{state:{reported:{outputs:{relays:{");
  for (int i = 0; i < IOBOARD_NUM_RELAYS; ++i) {
    json_printf(&out,
                "\"relay%d\":%B%s",
                i + 1,
                relays[i],
                (i < IOBOARD_NUM_RELAYS - 1) ? "," : "");
  }
  json_printf(&out, "}}},");


  // 🔁 Ahora escribimos desired con nulls para limpiar
  json_printf(&out, "desired:{outputs:{relays:{");
  for (int i = 0; i < IOBOARD_NUM_RELAYS; ++i) {
    json_printf(&out,
                "\"relay%d\":null%s",
                i + 1,
                (i < IOBOARD_NUM_RELAYS - 1) ? "," : "");
  }
  json_printf(&out, "}}}}}");

  LOG(LL_INFO, ("[SHADOW] Reporting and clearing desired: %s", buf));
  char topic[128];
  snprintf(topic, sizeof(topic), "$aws/things/%s/shadow/update", mgos_sys_config_get_device_id());

  mgos_mqtt_pub(topic, buf, strlen(buf), 1, false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IoBoard::reportInputs(struct json_out *out) {
  for (int i = 0; i < IOBOARD_NUM_INPUTS; ++i) {
    json_printf(out, ",\"input%d\":%B", i + 1, inputs[i]);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IoBoard::reportAnalog(struct json_out *out) {
  analogValue = mgos_adc_read(analogPin);
  json_printf(out, ",\"analog\":%d", analogValue);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IoBoard::reportBuzzer(struct json_out *out) {
  json_printf(out, ",\"buzzer\":%B", buzzer);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IoBoard::reportState() {
  char buf[2048];                               // Amplía si tu JSON crece
  struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));

  analogValue = mgos_adc_read(analogPin);      // Una sola lectura ADC

  /* ---------- ROOT ---------- */
  json_printf(&out, "{outputs:{relays:{");

  /* ------ RELAYS ------ */
  for (int i = 0; i < IOBOARD_NUM_RELAYS; i++) {
    char key[10];
    snprintf(key, sizeof(key), "relay%d", i + 1);
    json_printf(&out, "%Q:%B%s",
                key,                          // %Q pone comillas correctas
                relays[i],
                (i < IOBOARD_NUM_RELAYS - 1 ? "," : ""));
  }

  /* ------ BUZZER ------ */
  json_printf(&out, "},buzzer:%B},", buzzer);  // clave estática sin comillas

  /* ---------- INPUTS ---------- */
  json_printf(&out, "inputs:{digital:{");
  for (int i = 0; i < IOBOARD_NUM_INPUTS; i++) {
    char key[10];
    snprintf(key, sizeof(key), "input%d", i + 1);
    json_printf(&out, "%Q:%B%s",
                key,
                inputs[i],
                (i < IOBOARD_NUM_INPUTS - 1 ? "," : ""));
  }

  /* ------ ANALOG ------ */
  json_printf(&out, "},analog:{adc1:%d}}}", analogValue);

  /* ---------- PUBLICAR ---------- */
  mgos_shadow_updatef(0, "%s", buf);           // mgos añade {"state":{"reported":…}}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* ---------- envía sólo la parte del buzzer ---------- */
void IoBoard::reportBuzzerState() {
  char msg[64];
  snprintf(msg, sizeof(msg),
           "{\"outputs\":{\"buzzer\":%s}}",
           buzzer ? "true" : "false");
  mgos_shadow_updatef(0, "%s", msg);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* ---------- setter ---------- */
void IoBoard::setBuzzer(bool on) {
  buzzer = on;
  mgos_gpio_write(buzzerPin, buzzer);
  reportBuzzerState();          // sincroniza la Shadow
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* (opcional) getter */
bool IoBoard::getBuzzer() const { return buzzer; }


#include "mgos_aws_shadow.h"
#include "mgos.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IoBoard::reportRelayState(int index) {
  if (index < 0 || index >= IOBOARD_NUM_RELAYS) return;

  int relay_id = index + 1;
  bool state = relays[index];

  // Publicamos sólo reported, sin tocar desired
  mgos_aws_shadow_updatef(
    0,
    "{outputs:{relays:{\"relay%d\":%B}}}",
    relay_id, state
  );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IoBoard::syncRelayShadowViaMQTT(int index, bool state) {
  char topic[128];
  snprintf(topic, sizeof(topic), "$aws/things/%s/shadow/update", mgos_sys_config_get_device_id());

  char payload[256];
  snprintf(payload, sizeof(payload),
    "{"
      "\"state\":{"
        "\"reported\":{"
          "\"outputs\":{\"relays\":{\"relay%d\":%s}}"
        "},"
        "\"desired\":{"
          "\"outputs\":{\"relays\":{\"relay%d\":null}}"
        "}"
      "}"
    "}",
    index + 1, state ? "true" : "false",
    index + 1
  );

  LOG(LL_INFO, ("[MQTT] Publishing shadow update: %s", payload));

  bool ok = mgos_mqtt_pub(topic, payload, strlen(payload), 1 /* QoS */, false);
  if (!ok) {
    LOG(LL_ERROR, ("[MQTT] Failed to publish shadow update"));
  }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void IoBoard::setRelay(int index, bool on) {
  if (index < 0 || index >= IOBOARD_NUM_RELAYS) return;

  relays[index] = on;
  mgos_gpio_write(relayPins[index], on);

  if (!shadow_update_suppressed) {
    // Publicamos reported + limpiamos desired en una sola operación
    syncRelayShadowViaMQTT(index, on);
  } else {
    // Si vino desde AWS, solo reportamos el estado sin tocar desired
    reportRelayState(index);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool IoBoard::getRelay(int index) const {
  return (index >= 0 && index < IOBOARD_NUM_RELAYS) ? relays[index] : false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool IoBoard::getInput(int index) const {
  return (index >= 0 && index < IOBOARD_NUM_INPUTS) ? inputs[index] : false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IoBoard::toggleRelay(int index) {
  if (index >= 0 && index < IOBOARD_NUM_RELAYS) {
    bool newState = !relays[index];
    setRelay(index, newState);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IoBoard::turnOffAllRelays() {
  for (int i = 0; i < IOBOARD_NUM_RELAYS; ++i) {
    relays[i] = false;
    mgos_gpio_write(relayPins[i], 0);
  }

  // Publica todos los relés en una sola llamada
  reportRelays();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void IoBoard::pulseRelay(int index, int duration_ms) {
  if (index < 0 || index >= IOBOARD_NUM_RELAYS) return;

  // Encendido usando lógica centralizada
  setRelay(index, true);

  // Usamos mgos_set_timer para apagar luego de duración
  mgos_set_timer(duration_ms, 0,
    [](void *arg) {
      int i = static_cast<int>(reinterpret_cast<intptr_t>(arg));
      IoBoard::getInstance().setRelay(i, false);  // Reutiliza setRelay
    },
    reinterpret_cast<void *>(static_cast<intptr_t>(index))
  );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" {
int mgos_io_board_init(void) {
  IoBoard::getInstance().init();
  return true;
}
}
