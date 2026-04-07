#pragma once

// (Phase 4) OTA firmware signing public key (ECDSA P-256).
// The server signs firmware with the corresponding private key.
// The ESP32 verifies the signature before installing any OTA update.
// This is a PUBLIC key — it is safe to embed in firmware.

static const char OTA_SIGNING_PUB_KEY[] PROGMEM = R"EOF(
-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE4E1jxV7+3FZBjtahGkddQ+563PD1
0g8zOKTARW2GYkKpTHB4dpdLcqzB1JQamlu2lw64wC94645vV64UpRCYdg==
-----END PUBLIC KEY-----
)EOF";
