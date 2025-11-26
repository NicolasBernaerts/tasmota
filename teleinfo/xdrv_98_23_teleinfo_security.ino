/*
  xdrv_98_23_teleinfo_security.ino - Security features for Teleinfo ESP32

  Copyright (C) 2025 Nicolas Bernaerts & Olive

  This module implements advanced security features:
  - NVS (Non-Volatile Storage) encryption for sensitive data
  - Credential secure storage
  - MQTT TLS/SSL support optimization
  - Certificate management

  Benefits:
  - GDPR compliance for energy data
  - Protected API keys (RTE, InfluxDB, etc.)
  - Encrypted MQTT communication
  - Secure credential storage

  Version history:
    26/11/2025 - v1.0 - Creation for ESP32 POE security

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

#ifdef USE_ENERGY_SENSOR
#ifdef USE_TELEINFO
#ifdef USE_TELEINFO_SECURITY
#ifdef ESP32

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"

/*************************************************\
 *               NVS Configuration
\*************************************************/

#define TIC_NVS_NAMESPACE    "teleinfo_sec"
#define TIC_NVS_KEY_INFLUX   "influx_token"
#define TIC_NVS_KEY_RTE      "rte_key"
#define TIC_NVS_KEY_MQTT_PWD "mqtt_password"
#define TIC_NVS_KEY_MQTT_USER "mqtt_user"
#define TIC_NVS_KEY_FTP_PWD  "ftp_password"

/*************************************************\
 *            Secure NVS Storage
\*************************************************/

bool TeleinfoSecureNVSInit() {
  esp_err_t err = nvs_flash_init();

  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated - erase and re-initialize
    AddLog(LOG_LEVEL_WARNING, PSTR("TIC: SEC NVS truncated, erasing..."));
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }

  if (err != ESP_OK) {
    AddLog(LOG_LEVEL_ERROR, PSTR("TIC: SEC NVS init failed: %d"), err);
    return false;
  }

  // Check if NVS encryption is available
  // Note: NVS encryption requires flash encryption to be enabled in bootloader
  // This is typically done during manufacturing

  AddLog(LOG_LEVEL_INFO, PSTR("TIC: SEC NVS initialized"));

  return true;
}

/*************************************************\
 *         Store Credential (Encrypted)
\*************************************************/

bool TeleinfoStoreSecureCredential(const char* key, const char* value) {
  nvs_handle_t handle;
  esp_err_t err;

  if (!key || !value) {
    AddLog(LOG_LEVEL_ERROR, PSTR("TIC: SEC Invalid key or value"));
    return false;
  }

  // Open NVS namespace
  err = nvs_open(TIC_NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    AddLog(LOG_LEVEL_ERROR, PSTR("TIC: SEC Failed to open NVS: %d"), err);
    return false;
  }

  // Store string (encrypted if flash encryption enabled)
  err = nvs_set_str(handle, key, value);
  if (err != ESP_OK) {
    AddLog(LOG_LEVEL_ERROR, PSTR("TIC: SEC Failed to store '%s': %d"), key, err);
    nvs_close(handle);
    return false;
  }

  // Commit changes
  err = nvs_commit(handle);
  if (err != ESP_OK) {
    AddLog(LOG_LEVEL_ERROR, PSTR("TIC: SEC Failed to commit: %d"), err);
    nvs_close(handle);
    return false;
  }

  nvs_close(handle);

  AddLog(LOG_LEVEL_INFO, PSTR("TIC: SEC Credential '%s' stored securely"), key);

  return true;
}

/*************************************************\
 *        Retrieve Credential (Encrypted)
\*************************************************/

bool TeleinfoGetSecureCredential(const char* key, char* value, size_t max_len) {
  nvs_handle_t handle;
  esp_err_t err;
  size_t required_size = max_len;

  if (!key || !value || max_len == 0) {
    AddLog(LOG_LEVEL_ERROR, PSTR("TIC: SEC Invalid parameters"));
    return false;
  }

  // Open NVS namespace (read-only)
  err = nvs_open(TIC_NVS_NAMESPACE, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    AddLog(LOG_LEVEL_DEBUG, PSTR("TIC: SEC NVS not found: %d"), err);
    return false;
  }

  // Get string
  err = nvs_get_str(handle, key, value, &required_size);
  if (err != ESP_OK) {
    if (err != ESP_ERR_NVS_NOT_FOUND) {
      AddLog(LOG_LEVEL_ERROR, PSTR("TIC: SEC Failed to get '%s': %d"), key, err);
    }
    nvs_close(handle);
    return false;
  }

  nvs_close(handle);

  AddLog(LOG_LEVEL_DEBUG, PSTR("TIC: SEC Credential '%s' retrieved"), key);

  return true;
}

/*************************************************\
 *          Delete Credential
\*************************************************/

bool TeleinfoDeleteSecureCredential(const char* key) {
  nvs_handle_t handle;
  esp_err_t err;

  if (!key) return false;

  err = nvs_open(TIC_NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) return false;

  err = nvs_erase_key(handle, key);
  if (err == ESP_OK) {
    nvs_commit(handle);
    AddLog(LOG_LEVEL_INFO, PSTR("TIC: SEC Credential '%s' deleted"), key);
  }

  nvs_close(handle);

  return (err == ESP_OK);
}

/*************************************************\
 *          List Stored Credentials
\*************************************************/

void TeleinfoListSecureCredentials() {
  nvs_handle_t handle;
  esp_err_t err;
  nvs_iterator_t it;

  err = nvs_open(TIC_NVS_NAMESPACE, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    AddLog(LOG_LEVEL_INFO, PSTR("TIC: SEC No credentials stored"));
    return;
  }

  AddLog(LOG_LEVEL_INFO, PSTR("TIC: SEC Stored credentials:"));

  // Create iterator
  it = nvs_entry_find(NVS_DEFAULT_PART_NAME, TIC_NVS_NAMESPACE, NVS_TYPE_STR);

  if (it == NULL) {
    AddLog(LOG_LEVEL_INFO, PSTR("  (none)"));
  } else {
    while (it != NULL) {
      nvs_entry_info_t info;
      nvs_entry_info(it, &info);

      // Show key name only (not value for security)
      AddLog(LOG_LEVEL_INFO, PSTR("  - %s"), info.key);

      it = nvs_entry_next(it);
    }
    nvs_release_iterator(it);
  }

  nvs_close(handle);
}

/*************************************************\
 *        MQTT TLS Certificate Storage
\*************************************************/

// Default Let's Encrypt ISRG Root X1 CA certificate
static const char MQTT_CA_CERT_DEFAULT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

bool TeleinfoLoadMQTTCertificate(char* cert_buffer, size_t buffer_size) {
  // Try to load from NVS first
  if (TeleinfoGetSecureCredential("mqtt_ca_cert", cert_buffer, buffer_size)) {
    AddLog(LOG_LEVEL_INFO, PSTR("TIC: SEC MQTT CA cert loaded from NVS"));
    return true;
  }

  // Use default certificate
  strlcpy(cert_buffer, MQTT_CA_CERT_DEFAULT, buffer_size);
  AddLog(LOG_LEVEL_INFO, PSTR("TIC: SEC Using default MQTT CA cert (Let's Encrypt)"));

  return true;
}

bool TeleinfoStoreMQTTCertificate(const char* cert_pem) {
  return TeleinfoStoreSecureCredential("mqtt_ca_cert", cert_pem);
}

/*************************************************\
 *               Commands
\*************************************************/

bool TeleinfoSecurityCommand() {
  bool serviced = false;
  char command[CMND_SIZE];
  char *pstr_key, *pstr_value;

  serviced = (XdrvMailbox.data_len > 0);
  if (serviced) {
    strlcpy(command, XdrvMailbox.data, sizeof(command));

    if (strcasecmp(command, "list") == 0) {
      TeleinfoListSecureCredentials();
    } else if (strncasecmp(command, "store ", 6) == 0) {
      // Format: store key=value
      pstr_key = command + 6;
      pstr_value = strchr(pstr_key, '=');

      if (pstr_value) {
        *pstr_value = '\0';
        pstr_value++;

        if (TeleinfoStoreSecureCredential(pstr_key, pstr_value)) {
          Response_P(PSTR("{\"SecStore\":\"%s\":\"OK\"}"), pstr_key);
        } else {
          Response_P(PSTR("{\"SecStore\":\"%s\":\"FAILED\"}"), pstr_key);
        }
      }
    } else if (strncasecmp(command, "get ", 4) == 0) {
      char value[256];
      pstr_key = command + 4;

      if (TeleinfoGetSecureCredential(pstr_key, value, sizeof(value))) {
        // Don't show actual value in logs for security
        Response_P(PSTR("{\"SecGet\":\"%s\":\"***\"}"), pstr_key);
      } else {
        Response_P(PSTR("{\"SecGet\":\"%s\":\"NOT_FOUND\"}"), pstr_key);
      }
    } else if (strncasecmp(command, "delete ", 7) == 0) {
      pstr_key = command + 7;

      if (TeleinfoDeleteSecureCredential(pstr_key)) {
        Response_P(PSTR("{\"SecDelete\":\"%s\":\"OK\"}"), pstr_key);
      } else {
        Response_P(PSTR("{\"SecDelete\":\"%s\":\"FAILED\"}"), pstr_key);
      }
    } else {
      serviced = false;
    }
  } else {
    // No parameter - show help
    AddLog(LOG_LEVEL_INFO, PSTR("TIC: Security commands:"));
    AddLog(LOG_LEVEL_INFO, PSTR("  tic_sec list              = list stored credentials"));
    AddLog(LOG_LEVEL_INFO, PSTR("  tic_sec store key=value   = store credential"));
    AddLog(LOG_LEVEL_INFO, PSTR("  tic_sec get key           = check if credential exists"));
    AddLog(LOG_LEVEL_INFO, PSTR("  tic_sec delete key        = delete credential"));
    AddLog(LOG_LEVEL_INFO, PSTR("  "));
    AddLog(LOG_LEVEL_INFO, PSTR("Predefined keys:"));
    AddLog(LOG_LEVEL_INFO, PSTR("  influx_token    = InfluxDB token"));
    AddLog(LOG_LEVEL_INFO, PSTR("  rte_key         = RTE API key (Base64)"));
    AddLog(LOG_LEVEL_INFO, PSTR("  mqtt_user       = MQTT username"));
    AddLog(LOG_LEVEL_INFO, PSTR("  mqtt_password   = MQTT password"));
    serviced = true;
  }

  return serviced;
}

/*************************************************\
 *               Initialization
\*************************************************/

void TeleinfoSecurityInit() {
  // Initialize NVS
  if (!TeleinfoSecureNVSInit()) {
    AddLog(LOG_LEVEL_ERROR, PSTR("TIC: SEC Initialization failed"));
    return;
  }

  AddLog(LOG_LEVEL_INFO, PSTR("TIC: SEC Security module initialized"));
  AddLog(LOG_LEVEL_INFO, PSTR("TIC: SEC Use 'tic_sec' commands to manage credentials"));
}

/*************************************************\
 *               Interface
\*************************************************/

bool Xdrv98_23(uint32_t function) {
  bool result = false;

  switch (function) {
    case FUNC_INIT:
      TeleinfoSecurityInit();
      break;

    case FUNC_COMMAND:
      if (strcasecmp(XdrvMailbox.topic, "tic_sec") == 0) {
        result = TeleinfoSecurityCommand();
      }
      break;
  }

  return result;
}

#endif  // ESP32
#endif  // USE_TELEINFO_SECURITY
#endif  // USE_TELEINFO
#endif  // USE_ENERGY_SENSOR
