#!/bin/bash

# Uso: ./create_aws_thing.sh esp32-relay-001

set -e

THING_NAME="$1"

if [ -z "$THING_NAME" ]; then
  echo "❌ Debes especificar un nombre de Thing. Ejemplo: ./create_aws_thing.sh esp32-relay-001"
  exit 1
fi

POLICY_NAME="MongooseDefaultPolicy"
CERT_DIR="certs/$THING_NAME"

echo "📦 Creando carpeta $CERT_DIR..."
mkdir -p "$CERT_DIR"

echo "✅ Creando Thing: $THING_NAME..."
aws iot create-thing --thing-name "$THING_NAME" >/dev/null

echo "🔐 Generando certificado y clave..."
aws iot create-keys-and-certificate \
  --set-as-active \
  --output json > "$CERT_DIR/certs.json"

CERT_ARN=$(jq -r '.certificateArn' "$CERT_DIR/certs.json")
echo "$CERT_ARN" > "$CERT_DIR/arn.txt"
jq -r '.certificatePem' "$CERT_DIR/certs.json" > "$CERT_DIR/cert.pem"
jq -r '.keyPair.PrivateKey' "$CERT_DIR/certs.json" > "$CERT_DIR/key.pem"

echo "🌐 Descargando Amazon CA (si no existe)..."
curl -s https://www.amazontrust.com/repository/AmazonRootCA1.pem -o "$CERT_DIR/ca.pem"

echo "🔗 Asociando certificado al Thing..."
aws iot attach-thing-principal \
  --thing-name "$THING_NAME" \
  --principal "$CERT_ARN"

echo "📜 Verificando si existe política $POLICY_NAME..."
if ! aws iot get-policy --policy-name "$POLICY_NAME" >/dev/null 2>&1; then
  echo "📝 Creando política $POLICY_NAME..."
  aws iot create-policy \
    --policy-name "$POLICY_NAME" \
    --policy-document '{
      "Version": "2012-10-17",
      "Statement": [
        {
          "Effect": "Allow",
          "Action": [ "iot:*" ],
          "Resource": "*"
        }
      ]
    }'
fi

echo "🔗 Asociando política al certificado..."
aws iot attach-policy \
  --policy-name "$POLICY_NAME" \
  --target "$CERT_ARN"

echo "📤 Subiendo certificados al dispositivo vía mos..."
mos put "$CERT_DIR/cert.pem"
mos put "$CERT_DIR/key.pem"
mos put "$CERT_DIR/ca.pem"

echo "✅ Listo. El Thing '$THING_NAME' está registrado, con certificados subidos al ESP32."



MOS_YML="mos.yml"

if [ ! -f "$MOS_YML" ]; then
  echo "❌ No se encontró $MOS_YML en el directorio actual"
  exit 1
fi

echo "🛠️  Buscando y modificando device.id en $MOS_YML..."

if grep -q '\["device.id",' "$MOS_YML"; then
  # Reemplazar el valor existente
  sed -i -E "s/(\[\"device\.id\",\s*\")[^\"]+(\"])/\1$THING_NAME\2/" "$MOS_YML"
  echo "✅ device.id actualizado a '$THING_NAME'"
else
  # Insertar al final de config_schema
  awk -v dn="$THING_NAME" '
    /^config_schema:/ { print; in_section=1; next }
    in_section && /^[^[:space:]]/ { in_section=0 }
    in_section && /^  -/ { last=$0 }
    { lines[NR]=$0 }
    END {
      for (i = 1; i <= NR; i++) {
        print lines[i]
        if (lines[i] == last) {
          print "  - [\"device.id\", \"" dn "\"]"
        }
      }
    }
  ' "$MOS_YML" > "$MOS_YML.tmp" && mv "$MOS_YML.tmp" "$MOS_YML"
  echo "✅ device.id agregado al final de config_schema"
fi
