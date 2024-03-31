/*
  xdrv_15_teleinfo_homeassistant.ino - Home assistant auto-discovery integration for Teleinfo

  Copyright (C) 2024  Nicolas Bernaerts

  Version history :
    23/03/2024 - v1.0 - Creation (with help of msevestre31)
    28/03/2024 - v1.1 - Home Assistant auto-discovery only with SetOption19 1

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FIRMWARE_SAFEBOOT

#ifdef USE_TELEINFO

#ifdef USE_TELEINFO_HOMEASSISTANT

/*************************************************\
 *               Variables
\*************************************************/

#define TELENINFO_DISCOVERY_NB_MESSAGE    5

const char kTicHomeAssistantPhase[] PROGMEM = "|numeric-1-box-multiple|numeric-2-box-multiple|numeric-3-box-multiple";

const char kTicHomeAssistantVersion[] PROGMEM = EXTENSION_VERSION;

/********************************\
 *              Data
\********************************/

static struct {
  uint8_t  published = 0;                  // auto-discovery publication flag
  uint8_t  nb_sensor = 0;                  // number of published sensors
} teleinfo_homeassistant;

/***************************************\
 *           JSON publication
\***************************************/

void TeleinfoHomeAssistantPublish (const char* pstr_name, const char* pstr_unique, const char* pstr_key, const char* pstr_icon, const char* pstr_class, const char* pstr_unit)
{
//  uint32_t chip_id = ESP_getChipId ();
  uint32_t ip_address;
  char     str_sensor[64];
  char     str_topic[64];

  // get IP address
#if defined(ESP32) && defined(USE_ETHERNET)
  if (static_cast<uint32_t>(EthernetLocalIP()) != 0) ip_address = (uint32_t)EthernetLocalIP ();
    else
#endif  
  ip_address = (uint32_t)WiFi.localIP ();

  // get sensor topic
  GetTopic_P (str_sensor, TELE, TasmotaGlobal.mqtt_topic, PSTR (D_RSLT_SENSOR));
  snprintf_P (str_topic, sizeof (str_topic), PSTR ("homeassistant/sensor/%s_%s/config"), NetworkUniqueId().c_str(), pstr_unique);

  // publish auto-discovery retained message
  Response_P ("{\"name\":\"%s\",\"stat_t\":\"%s\",\"uniq_id\":\"%s_%s\",\"val_tpl\":\"{{value_json%s}}\",\"ic\":\"mdi:%s\"", pstr_name, str_sensor, NetworkUniqueId().c_str(), pstr_unique, pstr_key, pstr_icon);

  // if first call, append device description
  if (teleinfo_homeassistant.nb_sensor == 0) ResponseAppend_P (",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\",\"mf\":\"Tasmota Teleinfo\",\"sw_version\":\"%s / %s\",\"configuration_url\":\"http://%_I\"}", NetworkUniqueId().c_str(), SettingsText( SET_DEVICENAME), kTicHomeAssistantVersion, TasmotaGlobal.version, ip_address);
    else ResponseAppend_P (",\"dev\":{\"ids\":[\"%s\"]}", NetworkUniqueId().c_str());
  if (pstr_class != nullptr) ResponseAppend_P (",\"dev_cla\":\"%s\"", pstr_class);
  if (pstr_unit != nullptr) ResponseAppend_P (",\"unit_of_meas\":\"%s\"", pstr_unit);
  ResponseAppend_P ("}");
  MqttPublish (str_topic, true);
  teleinfo_homeassistant.nb_sensor++;
}

// trigger publication
void TeleinfoHomeAssistantEverySecond ()
{
  uint8_t index;
  char    str_key[32];
  char    str_code[32];
  char    str_icon[32];
  char    str_unique[32];
  char    str_name[64];

  // if already published or running on battery, ignore 
  if (teleinfo_homeassistant.published) return;
  if (teleinfo_config.battery) return;
  
  // if message ready to publish -> publication
  if (teleinfo_meter.nb_message > TELENINFO_DISCOVERY_NB_MESSAGE)
  {
    // if METER section is enabled
    if (teleinfo_config.meter)
    {
      // conso
      if (teleinfo_conso.total_wh > 0)
      {
        // Conso I
        TeleinfoHomeAssistantPublish ("Conso Courant", "METER_I", "['METER']['I']", "alpha-c-circle", "current", "A");

        // Conso VA
        TeleinfoHomeAssistantPublish ("Conso Puissance apparente", "METER_P", "['METER']['P']", "alpha-c-circle", "apparent_power", "VA");

        // Conso W
        TeleinfoHomeAssistantPublish ("Conso Puissance active", "METER_W", "['METER']['W']", "alpha-c-circle", "power", "W");

        // Conso Cosphi
        TeleinfoHomeAssistantPublish ("Conso Cos φ", "METER_C", "['METER']['C']", "alpha-c-circle", "power_factor", nullptr);

        // Conso V
        if (teleinfo_contract.phase == 1) TeleinfoHomeAssistantPublish ("Conso Tension", "METER_U1", "['METER']['U1']", "alpha-c-circle", "voltage", "V");

        // Conso Yesterday
        TeleinfoHomeAssistantPublish ("Conso Total Hier", "METER_YDAY", "['METER']['YDAY']", "alpha-c-circle", "energy", "Wh");

        // Conso Today
        TeleinfoHomeAssistantPublish ("Conso Total Aujourd'hui", "METER_2DAY", "['METER']['2DAY']", "alpha-c-circle", "energy", "Wh");

        // if triphase, loop thru phases
        if (teleinfo_contract.phase > 1) for (index = 1; index <= teleinfo_contract.phase; index++)
        {
          // get phase icon
          GetTextIndexed (str_icon, sizeof (str_icon), index, kTicHomeAssistantPhase);

          // V
          snprintf_P (str_unique, sizeof (str_unique), PSTR ("METER_U%u"), index);
          snprintf_P (str_key, sizeof (str_key), PSTR ("['METER']['U%u']"), index);
          TeleinfoHomeAssistantPublish ("Phase Tension", str_unique, str_key, str_icon, "voltage", "V");

          // I
          snprintf_P (str_unique, sizeof (str_unique), PSTR ("METER_I%u"), index);
          snprintf_P (str_key, sizeof (str_key), PSTR ("['METER']['I%u']"), index);
          TeleinfoHomeAssistantPublish ("Phase Courant", str_unique, str_key, str_icon, "current", "A");

          // VA
          snprintf_P (str_unique, sizeof (str_unique), PSTR ("METER_P%u"), index);
          snprintf_P (str_key, sizeof (str_key), PSTR ("['METER']['P%u']"), index);
          TeleinfoHomeAssistantPublish ("Phase Puissance apparente", str_unique, str_key, str_icon, "apparent_power", "VA");

          // W
          snprintf_P (str_unique, sizeof (str_unique), PSTR ("METER_W%u"), index);
          snprintf_P (str_key, sizeof (str_key), PSTR ("['METER']['W%u']"), index);
          TeleinfoHomeAssistantPublish ("Phase Puissance active", str_unique, str_key, str_icon, "power", "W");
        }
      }

      // if METER section is enabled
      if (teleinfo_prod.total_wh > 0)
      {
        // Prod Apparent power
        TeleinfoHomeAssistantPublish ("Prod Puissance apparente", "METER_PP", "['METER']['PP']", "alpha-p-circle", "apparent_power", "VA");

        // Prod Active power
        TeleinfoHomeAssistantPublish ("Prod Puissance active", "METER_PW", "['METER']['PW']", "alpha-p-circle", "power", "W");

        // Prod Cosphi
        TeleinfoHomeAssistantPublish ("Prod Cos φ", "METER_PC", "['METER']['PC']", "alpha-p-circle", "power_factor", nullptr);

        // Prod Yesterday
        TeleinfoHomeAssistantPublish ("Prod Total Hier", "METER_PYDAY", "['METER']['PYDAY']", "alpha-p-circle", "energy", "Wh");

        // Prod Today
        TeleinfoHomeAssistantPublish ("Prod Total Aujourd'hui", "METER_P2DAY", "['METER']['P2DAY']", "alpha-p-circle", "energy", "Wh");
      }
    }

    // if CONTRACT section is enabled
    if (teleinfo_config.contract)
    {
      // contract name
      TeleinfoHomeAssistantPublish ("Contrat", "CONTRACT_NAME", "['CONTRACT']['name']", "calendar", nullptr, nullptr);

      // contract period
      TeleinfoHomeAssistantPublish ("Contrat Période actuelle", "CONTRACT_PERIOD", "['CONTRACT']['period']", "calendar", nullptr, nullptr);

      // contract current color
      TeleinfoHomeAssistantPublish ("Contrat Couleur actuelle", "CONTRACT_COLOR", "['CONTRACT']['color']", "calendar", nullptr, nullptr);

      // contract today
      TeleinfoHomeAssistantPublish ("Couleur Aujourd'hui", "CONTRACT_TODAY", "['CONTRACT']['today']", "calendar", nullptr, nullptr);

      // contract tomorrow
      TeleinfoHomeAssistantPublish ("Couleur Demain", "CONTRACT_TOMORROW", "['CONTRACT']['tomorrow']", "calendar", nullptr, nullptr);

      // loop thru conso counters
      for (index = 0; index < teleinfo_contract.period_qty; index++)
      {
        if (teleinfo_conso.index_wh[index] > 0)
        {
          snprintf_P (str_unique, sizeof (str_unique), PSTR ("CONTRACT_%u"), index);
          TeleinfoPeriodGetName (index, str_code, sizeof (str_code));
          snprintf_P (str_name, sizeof (str_name), PSTR ("Total période %s"), str_code);
          TeleinfoPeriodGetCode (index, str_code, sizeof (str_code));
          snprintf_P (str_key, sizeof (str_key), PSTR ("['CONTRACT']['%s']"), str_code);
          TeleinfoHomeAssistantPublish (str_name, str_unique, str_key, "counter", "energy", "Wh");
        }
      }

      // prod counter
      if (teleinfo_prod.total_wh != 0) TeleinfoHomeAssistantPublish ("Total période Production", "CONTRACT_PROD", "['CONTRACT']['PROD']", "counter", "energy", "Wh");
    }

    // if RELAY section is enabled
    if (teleinfo_config.relay)
    {
      // loop thru periods
      for (index = 0; index < 8; index++)
      {
        snprintf_P (str_unique, sizeof (str_unique), PSTR ("RELAY_%u"), index + 1);
        GetTextIndexed (str_code, sizeof (str_code), index, kTeleinfoRelayName);
        snprintf_P (str_name, sizeof (str_name), PSTR ("Relai %u (%s)"), index + 1, str_code);
        snprintf_P (str_key, sizeof (str_key), PSTR ("['RELAY']['R%u']"), index + 1);
        TeleinfoHomeAssistantPublish (str_name, str_unique, str_key, "toggle-switch-off", nullptr, nullptr);
      }
    }

    // publication has been done
    teleinfo_homeassistant.published = 1;
  }
}

#endif      // USE_TELEINFO_HOMEASSISTANT

#endif      // USE_TELEINFO

#endif      // FIRMWARE_SAFEBOOT

