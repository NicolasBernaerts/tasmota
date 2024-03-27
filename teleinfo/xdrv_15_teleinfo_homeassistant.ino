/*
  xdrv_15_teleinfo_homeassistant.ino - Home assistant auto-discovery integration for Teleinfo

  Copyright (C) 2024  Nicolas Bernaerts

  Version history :
    23/03/2024 - v1.0 - Creation (with help of msevestre31)

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
#ifdef USE_HOME_ASSISTANT

/*************************************************\
 *               Variables
\*************************************************/

#ifdef USE_WINKY
const uint32_t teleinfo_ha_publish = 50;
#else
const uint32_t teleinfo_ha_publish = 5;
#endif

enum TeleinfoHomeAssistantStatus  {TIC_HOMEASSISTANT_NONE, TIC_HOMEASSISTANT_PENDING, TIC_HOMEASSISTANT_PUBLISH, TIC_HOMEASSISTANT_DONE, TIC_HOMEASSISTANT_MAX};

const char D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR[]   PROGMEM = "%s/sensor/%s/config";
const char D_TELEINFO_HOMEASSISTANT_CONFIG_UNIT[]    PROGMEM = "{\"name\":\"%s\",\"stat_t\":\"%s\",\"uniq_id\":\"%s\",\"dev_cla\":\"%s\",\"unit_of_meas\":\"%s\",\"val_tpl\":\"{{value_json%s}}\",\"ic\":\"mdi:%s\",\"dev\":{\"ids\":[\"%06X\"],\"name\":\"Teleinfo\"}}";
const char D_TELEINFO_HOMEASSISTANT_CONFIG_NOUNIT[]  PROGMEM = "{\"name\":\"%s\",\"stat_t\":\"%s\",\"uniq_id\":\"%s\",\"dev_cla\":\"%s\",\"val_tpl\":\"{{value_json%s}}\",\"ic\":\"mdi:%s\",\"dev\":{\"ids\":[\"%06X\"],\"name\":\"Teleinfo\"}}";
const char D_TELEINFO_HOMEASSISTANT_CONFIG_NOCLASS[] PROGMEM = "{\"name\":\"%s\",\"stat_t\":\"%s\",\"uniq_id\":\"%s\",\"val_tpl\":\"{{value_json%s}}\",\"ic\":\"mdi:%s\",\"dev\":{\"ids\":[\"%06X\"],\"name\":\"Teleinfo\"}}";

const char kTicHomeAssistantPhase[]                PROGMEM = "|numeric-1-box-multiple|numeric-1-box-multiple|numeric-1-box-multiple";

/********************************\
 *              Data
\********************************/

static struct {
  uint8_t published = 0;                  // no publication needed
} teleinfo_homeassistant;

/***************************************\
 *           JSON publication
\***************************************/

// trigger publication
void TeleinfoHomeAssistantEverySecond ()
{
  bool    retain = true;
  uint8_t index;
  char str_unit[32];
  char str_code[32];
  char str_icon[32];
  char str_unique[32];
  char str_topic[64];
  char str_name[64];
  char str_sensor[128];

  if ((teleinfo_homeassistant.published == 0) && (teleinfo_meter.nb_message > teleinfo_ha_publish))
  {
    // get sensor topic
    GetTopic_P (str_sensor, TELE, TasmotaGlobal.mqtt_topic, PSTR(D_RSLT_SENSOR));

    // if METER section is enabled
    if (teleinfo_config.meter)
    {
      // conso
      if (teleinfo_conso.global_wh > 0)
      {
        // Conso I
        snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_METER_I"), ESP_getChipId ());
        snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
        Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_UNIT, "Courant", str_sensor, str_unique, "current", "A", "['METER']['I']", "alpha-c-circle", ESP_getChipId ());
        MqttPublish (str_topic, retain);

        // Conso VA
        snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_METER_P"), ESP_getChipId ());
        snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
        Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_UNIT, "Puissance apparente", str_sensor, str_unique, "apparent_power", "VA", "['METER']['P']", "alpha-c-circle", ESP_getChipId ());
        MqttPublish (str_topic, retain);

        // Conso W
        snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_METER_W"), ESP_getChipId ());
        snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
        Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_UNIT, "Puissance active", str_sensor, str_unique, "power", "W", "['METER']['W']", "alpha-c-circle", ESP_getChipId ());
        MqttPublish (str_topic, retain);

        // Conso Cosphi
        snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_METER_C"), ESP_getChipId ());
        snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
        Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_NOUNIT, "Cosphi", str_sensor, str_unique, "power_factor", "['METER']['C']", "alpha-c-circle", ESP_getChipId ());
        MqttPublish (str_topic, retain);

        // Conso V
        if (teleinfo_contract.phase == 1)
        {
          snprintf_P (str_code, sizeof (str_code), PSTR ("['METER']['U1']"), index);
          snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_METER_U1"), ESP_getChipId (), index);
          snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
          Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_UNIT, "Tension", str_sensor, str_unique, "voltage", "V", str_code, "alpha-c-circle", ESP_getChipId ());
          MqttPublish (str_topic, retain);
        }

        // loop thru phases
        if (teleinfo_contract.phase > 1) for (index = 1; index <= teleinfo_contract.phase; index++)
        {
          GetTextIndexed (str_icon, sizeof (str_icon), index, kTicHomeAssistantPhase);

          // V
          snprintf_P (str_code, sizeof (str_code), PSTR ("['METER']['U%u']"), index);
          snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_METER_U%u"), ESP_getChipId (), index);
          snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
          Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_UNIT, "Tension", str_sensor, str_unique, "voltage", "V", str_code, str_icon, ESP_getChipId ());
          MqttPublish (str_topic, retain);

          // I
          snprintf_P (str_code, sizeof (str_code), PSTR ("['METER']['I%u']"), index);
          snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_METER_I%u"), ESP_getChipId (), index);
          snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
          Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_UNIT, "Courant", str_sensor, str_unique, "current", "A", str_code, str_icon, ESP_getChipId ());
          MqttPublish (str_topic, retain);

          // VA
          snprintf_P (str_code, sizeof (str_code), PSTR ("['METER']['P%u']"), index);
          snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_METER_P%u"), ESP_getChipId (), index);
          snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
          Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_UNIT, "Puissance apparente", str_sensor, str_unique, "apparent_power", "VA", str_code, str_icon, ESP_getChipId ());
          MqttPublish (str_topic, retain);

          // W
          snprintf_P (str_code, sizeof (str_code), PSTR ("['METER']['W%u']"), index);
          snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_METER_W%u"), ESP_getChipId (), index);
          snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
          Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_UNIT, "Puissance active", str_sensor, str_unique, "power", "W", str_code, str_icon, ESP_getChipId ());
          MqttPublish (str_topic, retain);
        }
      }

      // if METER section is enabled
      if (teleinfo_prod.total_wh > 0)
      {
        // Apparent power
        snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_METER_PP"), ESP_getChipId ());
        snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
        Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_UNIT, "Puissance apparente", str_sensor, str_unique, "apparent_power", "VA", "['METER']['PP']", "alpha-p-circle", ESP_getChipId ());
        MqttPublish (str_topic, retain);

        // Active power
        snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_METER_PW"), ESP_getChipId ());
        snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
        Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_UNIT, "Puissance active", str_sensor, str_unique, "power", "W", "['METER']['PW']", "alpha-p-circle", ESP_getChipId ());
        MqttPublish (str_topic, retain);

        // Cosphi
        snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_METER_PC"), ESP_getChipId ());
        snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
        Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_NOUNIT, "Cosphi", str_sensor, str_unique, "power_factor", "['METER']['PC']", "alpha-p-circle", ESP_getChipId ());
        MqttPublish (str_topic, retain);
      }
    }

    // if CONTRACT section is enabled
    if (teleinfo_config.contract)
    {
      // contract name
      snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_CONTRACT_NAME"), ESP_getChipId ());
      snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
      Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_NOCLASS, "Contrat", str_sensor, str_unique, "['CONTRACT']['name']", "calendar", ESP_getChipId ());
      MqttPublish (str_topic, retain);

      // contract period
      snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_CONTRACT_PERIOD"), ESP_getChipId ());
      snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
      Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_NOCLASS, "Période", str_sensor, str_unique, "['CONTRACT']['period']", "calendar", ESP_getChipId ());
      MqttPublish (str_topic, retain);

      // contract current color
      snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_CONTRACT_COLOR"), ESP_getChipId ());
      snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
      Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_NOCLASS, "Couleur", str_sensor, str_unique, "['CONTRACT']['color']", "calendar", ESP_getChipId ());
      MqttPublish (str_topic, retain);

      // contract current hour type
      snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_CONTRACT_HOUR"), ESP_getChipId ());
      snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
      Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_NOCLASS, "HC / HP", str_sensor, str_unique, "['CONTRACT']['hour']", "calendar", ESP_getChipId ());
      MqttPublish (str_topic, retain);

      // contract today
      snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_CONTRACT_TODAY"), ESP_getChipId ());
      snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
      Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_NOCLASS, "Aujourd'hui", str_sensor, str_unique, "['CONTRACT']['today']", "calendar", ESP_getChipId ());
      MqttPublish (str_topic, retain);

      // contract tomorrow
      snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_CONTRACT_TOMORROW"), ESP_getChipId ());
      snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
      Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_NOCLASS, "Demain", str_sensor, str_unique, "['CONTRACT']['tomorrow']", "calendar", ESP_getChipId ());
      MqttPublish (str_topic, retain);

      // loop thru conso counters
      for (index = 0; index < teleinfo_contract.period_qty; index++)
      {
        if (teleinfo_conso.index_wh[index] > 0)
        {
          TeleinfoPeriodGetCode (index, str_unit, sizeof (str_unit));
          TeleinfoPeriodGetName (index, str_name, sizeof (str_name));
          snprintf_P (str_code, sizeof (str_code), PSTR ("['CONTRACT']['%s']"), str_unit);
          snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_CONTRACT_%u"), ESP_getChipId (), index);
          snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
          Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_UNIT, str_name, str_sensor, str_unique, "energy", "Wh", str_code, "counter", ESP_getChipId ());
          MqttPublish (str_topic, retain);
        }
      }

      // prod counter
      if (teleinfo_prod.total_wh != 0)
      {
        snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_CONTRACT_PROD"), ESP_getChipId ());
        snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
        Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_UNIT, "Production", str_sensor, str_unique, "energy", "Wh", "['CONTRACT']['PROD']", "counter", ESP_getChipId ());
        MqttPublish (str_topic, retain);
      }
    }

    // if RELAY section is enabled
    if (teleinfo_config.relay)
    {
      // loop thru periods
      for (index = 1; index <= 8; index++)
      {
        itoa (index, str_unit, 10);
        snprintf_P (str_name, sizeof (str_name), PSTR ("Relai %u"), index);
        snprintf_P (str_code, sizeof (str_code), PSTR ("['RELAY']['R%u']"), index);
        snprintf_P (str_unique, sizeof (str_unique), PSTR ("%06X_RELAY_%u"), ESP_getChipId (), index);
        snprintf_P (str_topic, sizeof (str_topic), D_TELEINFO_HOMEASSISTANT_TOPIC_SENSOR, HOME_ASSISTANT_DISCOVERY_PREFIX, str_unique);
        Response_P (D_TELEINFO_HOMEASSISTANT_CONFIG_NOCLASS, str_name, str_sensor, str_unique, str_code, "toggle-switch-off", ESP_getChipId ());
        MqttPublish (str_topic, retain);
      }
    }

    // publication has been done
    teleinfo_homeassistant.published = 1;
  }
}

#endif      // USE_HOME_ASSISTANT
#endif      // USE_TELEINFO

#endif      // FIRMWARE_SAFEBOOT

