# OTA Flashing Guide – Mongoose OS (ESP32 de prueba)

Este documento resume los comandos utilizados para realizar actualizaciones OTA al dispositivo ESP32 de prueba utilizando Mongoose OS, desde distintas interfaces disponibles: **USB**, **Wi-Fi (WebSocket)** y **MQTT**.

---

## 🧱 Requisitos previos

- Firmware empaquetado en `.zip` generado con:

  ```bash
  mos build --platform esp32
  mos ota-pkg
  ```

- Archivo final: `fw.zip`, alojado en un bucket S3 público, por ejemplo:

  ```
  https://domoticore-fw-ota.s3.amazonaws.com/firmware_board_relay/fw.zip
  ```

- Dispositivo con licencia activa (verificada vía `Sys.GetInfo`).

---

## 📦 Comando OTA por USB (modo directo)

```bash
mos --port /dev/ttyACM0 ota https://domoticore-fw-ota.s3.amazonaws.com/firmware_board_relay/fw.zip
```

---

## 🌐 Comando OTA por red local (WebSocket/IP)

Verifica que el dispositivo tenga IP Wi-Fi:

```bash
mos call Sys.GetInfo
```

Ejemplo IP: `192.168.80.27`

```bash
mos --port ws://192.168.80.27/rpc ota https://domoticore-fw-ota.s3.amazonaws.com/firmware_board_relay/fw.zip
```

---

## ☁️ Comando OTA por MQTT (AWS IoT Core)

Asegúrate de tener estos archivos:

- `cert.pem`
- `key.pem`
- `ca.pem`

Y que el dispositivo esté conectado a `mqtt.server` (ej. AWS IoT Core).

```bash
mos --port mqtt://a2frld6tpgk2u3-ats.iot.us-east-1.amazonaws.com:8883 \
  --cert cert.pem \
  --key key.pem \
  --ca-cert ca.pem \
  ota https://domoticore-fw-ota.s3.amazonaws.com/firmware_board_relay/fw.zip
```

> Requiere que el ESP32 esté conectado a Internet y autenticado correctamente.

---

## 🛠️ Verificar firmware actual

```bash
mos call Sys.GetInfo
```

Busca el campo `"fw_version"` o `"fw_id"` para confirmar que el OTA se aplicó correctamente.

---

## ✅ Tips finales

- Usa `mos console` para ver logs en vivo del proceso OTA.
- Si `OTA.Update` no aparece en `RPC.List`, puedes usar `mos ota` sin depender de RPCs del firmware.
- Asegúrate de tener suficiente RAM y espacio libre en el dispositivo.
