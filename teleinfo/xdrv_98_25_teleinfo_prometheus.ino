/*
  xdrv_98_25_teleinfo_prometheus.ino - Prometheus metrics endpoint for Teleinfo

  Copyright (C) 2025 Nicolas Bernaerts & Olive

  This module provides Prometheus-compatible metrics endpoint:
  - Standard /metrics endpoint
  - Power, voltage, current, cos φ metrics
  - System metrics (uptime, memory, network)
  - Compatible with Grafana dashboards

  Benefits:
  - Professional monitoring
  - Grafana integration
  - Alert manager compatibility
  - Long-term storage
  - Standard metrics format

  Version history:
    26/11/2025 - v1.0 - Creation for ESP32 POE monitoring

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

#ifdef USE_ENERGY_SENSOR
#ifdef USE_TELEINFO
#ifdef USE_TELEINFO_PROMETHEUS
#ifdef ESP32

/*************************************************\
 *               Prometheus Metrics
\*************************************************/

void TeleinfoPrometheusPublish() {
  char response[4096];
  int len = 0;

  // HTTP header (handled by Tasmota webserver)
  // Content-Type: text/plain; version=0.0.4

  // ===== Power Metrics =====

  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("# HELP teleinfo_power_active_watts Active power in watts\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("# TYPE teleinfo_power_active_watts gauge\n"));

#ifdef USE_TELEINFO
  // Total active power
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("teleinfo_power_active_watts{phase=\"total\"} %d\n"),
    teleinfo_conso.pact);

  // Per-phase active power (if multi-phase)
  if (teleinfo_contract.phase > 1) {
    for (uint8_t phase = 0; phase < teleinfo_contract.phase; phase++) {
      len += snprintf_P(response + len, sizeof(response) - len,
        PSTR("teleinfo_power_active_watts{phase=\"%d\"} %d\n"),
        phase + 1, teleinfo_conso.phase[phase].pact);
    }
  }
#endif

  // Apparent power
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("\n# HELP teleinfo_power_apparent_va Apparent power in VA\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("# TYPE teleinfo_power_apparent_va gauge\n"));

#ifdef USE_TELEINFO
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("teleinfo_power_apparent_va{phase=\"total\"} %d\n"),
    teleinfo_conso.papp);

  if (teleinfo_contract.phase > 1) {
    for (uint8_t phase = 0; phase < teleinfo_contract.phase; phase++) {
      len += snprintf_P(response + len, sizeof(response) - len,
        PSTR("teleinfo_power_apparent_va{phase=\"%d\"} %d\n"),
        phase + 1, teleinfo_conso.phase[phase].papp);
    }
  }
#endif

  // ===== Voltage Metrics =====

  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("\n# HELP teleinfo_voltage_volts Voltage in volts\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("# TYPE teleinfo_voltage_volts gauge\n"));

#ifdef USE_TELEINFO
  for (uint8_t phase = 0; phase < teleinfo_contract.phase; phase++) {
    len += snprintf_P(response + len, sizeof(response) - len,
      PSTR("teleinfo_voltage_volts{phase=\"%d\"} %d\n"),
      phase + 1, teleinfo_conso.phase[phase].voltage);
  }
#endif

  // ===== Current Metrics =====

  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("\n# HELP teleinfo_current_amperes Current in amperes\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("# TYPE teleinfo_current_amperes gauge\n"));

#ifdef USE_TELEINFO
  // Total current
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("teleinfo_current_amperes{phase=\"total\"} %.2f\n"),
    (float)teleinfo_conso.current / 100.0);

  // Per-phase current
  for (uint8_t phase = 0; phase < teleinfo_contract.phase; phase++) {
    len += snprintf_P(response + len, sizeof(response) - len,
      PSTR("teleinfo_current_amperes{phase=\"%d\"} %.2f\n"),
      phase + 1, (float)teleinfo_conso.phase[phase].current / 100.0);
  }
#endif

  // ===== Power Factor =====

  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("\n# HELP teleinfo_power_factor Power factor (cos phi)\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("# TYPE teleinfo_power_factor gauge\n"));

#ifdef USE_TELEINFO
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("teleinfo_power_factor %.3f\n"),
    (float)teleinfo_conso.cosphi.value / 1000.0);
#endif

  // ===== Energy Counters =====

  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("\n# HELP teleinfo_energy_total_wh Total energy consumed (Wh)\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("# TYPE teleinfo_energy_total_wh counter\n"));

#ifdef USE_TELEINFO
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("teleinfo_energy_total_wh %lu\n"),
    teleinfo_contract.total_conso);

  // Daily energy
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("\n# HELP teleinfo_energy_today_wh Energy consumed today (Wh)\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("# TYPE teleinfo_energy_today_wh gauge\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("teleinfo_energy_today_wh %u\n"),
    teleinfo_conso.today_wh);
#endif

  // ===== Contract Info =====

  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("\n# HELP teleinfo_contract_subscribed_power Subscribed power (VA)\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("# TYPE teleinfo_contract_subscribed_power gauge\n"));

#ifdef USE_TELEINFO
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("teleinfo_contract_subscribed_power %u\n"),
    teleinfo_contract.ssousc);

  // Contract level (Tempo: blue=1, white=2, red=3)
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("\n# HELP teleinfo_contract_level Current tariff level\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("# TYPE teleinfo_contract_level gauge\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("teleinfo_contract_level %u\n"),
    teleinfo_contract.period_level);
#endif

  // ===== System Metrics =====

  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("\n# HELP teleinfo_uptime_seconds Module uptime in seconds\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("# TYPE teleinfo_uptime_seconds counter\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("teleinfo_uptime_seconds %lu\n"),
    millis() / 1000);

  // Free heap
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("\n# HELP teleinfo_free_heap_bytes Free heap memory in bytes\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("# TYPE teleinfo_free_heap_bytes gauge\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("teleinfo_free_heap_bytes %u\n"),
    ESP.getFreeHeap());

  // WiFi signal strength
  if (WiFi.status() == WL_CONNECTED) {
    len += snprintf_P(response + len, sizeof(response) - len,
      PSTR("\n# HELP teleinfo_wifi_rssi_dbm WiFi signal strength (dBm)\n"));
    len += snprintf_P(response + len, sizeof(response) - len,
      PSTR("# TYPE teleinfo_wifi_rssi_dbm gauge\n"));
    len += snprintf_P(response + len, sizeof(response) - len,
      PSTR("teleinfo_wifi_rssi_dbm %d\n"),
      WiFi.RSSI());
  }

  // Network status
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("\n# HELP teleinfo_network_connected Network connection status\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("# TYPE teleinfo_network_connected gauge\n"));

#ifdef USE_ETHERNET
  bool eth_connected = EthernetConnected();
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("teleinfo_network_connected{type=\"ethernet\"} %d\n"),
    eth_connected ? 1 : 0);
#endif

  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("teleinfo_network_connected{type=\"wifi\"} %d\n"),
    (WiFi.status() == WL_CONNECTED) ? 1 : 0);

  // MQTT status
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("\n# HELP teleinfo_mqtt_connected MQTT connection status\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("# TYPE teleinfo_mqtt_connected gauge\n"));
  len += snprintf_P(response + len, sizeof(response) - len,
    PSTR("teleinfo_mqtt_connected %d\n"),
    MqttIsConnected() ? 1 : 0);

  // Finish response
  response[len] = '\0';

  // Send response
  WSContentSend_P(response);
}

/*************************************************\
 *               Web Handler
\*************************************************/

void TeleinfoPrometheusWebHandler() {
  // Set response headers
  Webserver->sendHeader(F("Content-Type"), F("text/plain; version=0.0.4; charset=utf-8"));
  Webserver->send(200, F("text/plain; version=0.0.4"), "");

  // Send metrics
  WSContentBegin(200, CT_PLAIN);
  TeleinfoPrometheusPublish();
  WSContentEnd();
}

/*************************************************\
 *               Initialization
\*************************************************/

void TeleinfoPrometheusInit() {
  // Register /metrics endpoint
  Webserver->on("/metrics", HTTP_GET, TeleinfoPrometheusWebHandler);

  AddLog(LOG_LEVEL_INFO, PSTR("TIC: Prometheus endpoint available at /metrics"));
}

/*************************************************\
 *               Commands
\*************************************************/

bool TeleinfoPrometheusCommand() {
  bool serviced = false;

  if (XdrvMailbox.data_len == 0) {
    // No parameter - show help
    AddLog(LOG_LEVEL_INFO, PSTR("TIC: Prometheus metrics available at:"));
    AddLog(LOG_LEVEL_INFO, PSTR("  http://%s/metrics"), WiFi.localIP().toString().c_str());
    AddLog(LOG_LEVEL_INFO, PSTR("  "));
    AddLog(LOG_LEVEL_INFO, PSTR("Example Prometheus config:"));
    AddLog(LOG_LEVEL_INFO, PSTR("  scrape_configs:"));
    AddLog(LOG_LEVEL_INFO, PSTR("    - job_name: 'teleinfo'"));
    AddLog(LOG_LEVEL_INFO, PSTR("      scrape_interval: 10s"));
    AddLog(LOG_LEVEL_INFO, PSTR("      static_configs:"));
    AddLog(LOG_LEVEL_INFO, PSTR("        - targets: ['%s:80']"), WiFi.localIP().toString().c_str());
    serviced = true;
  }

  return serviced;
}

/*************************************************\
 *               Interface
\*************************************************/

bool Xdrv98_25(uint32_t function) {
  bool result = false;

  switch (function) {
    case FUNC_INIT:
      TeleinfoPrometheusInit();
      break;

    case FUNC_COMMAND:
      if (strcasecmp(XdrvMailbox.topic, "tic_prom") == 0) {
        result = TeleinfoPrometheusCommand();
      }
      break;
  }

  return result;
}

#endif  // ESP32
#endif  // USE_TELEINFO_PROMETHEUS
#endif  // USE_TELEINFO
#endif  // USE_ENERGY_SENSOR
