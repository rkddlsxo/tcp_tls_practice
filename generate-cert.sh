#!/usr/bin/env bash
# Generate a self-signed certificate for the local TLS teaching examples.
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

if ! command -v openssl >/dev/null 2>&1; then
    echo "OpenSSL is required. On Ubuntu: sudo apt install openssl" >&2
    exit 1
fi

# Preserve existing certificates and keys, including symbolic links.
for file in server-cert.pem server-key.pem; do
    if [[ -e "$file" || -L "$file" ]]; then
        echo "$file already exists. Move it before generating a new pair." >&2
        exit 1
    fi
done

umask 077
openssl req -x509 -newkey rsa:2048 -sha256 -nodes \
    -keyout server-key.pem \
    -out server-cert.pem \
    -days 30 \
    -subj "/CN=localhost" \
    -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"

echo "Created server-cert.pem and server-key.pem for localhost / 127.0.0.1."
echo "Valid for 30 days. Run the TLS examples from this directory."
