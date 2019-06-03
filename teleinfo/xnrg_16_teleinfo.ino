/*
  xnrg_16_teleinfo.ino - France Teleinfo energy sensor support for Sonoff-Tasmota
  
  Copyright (C) 2019  Nicolas Bernaerts

    05/05/2019 - v1.0 - Creation
    16/05/2019 - v1.1 - Add Tempo and EJP contracts
    
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

#ifdef USE_ENERGY_SENSOR
#ifdef USE_TELEINFO

/*********************************************************************************************\
 * Teleinfo historical
 * docs https://www.enedis.fr/sites/default/files/Enedis-NOI-CPT_54E.pdf
 * Teleinfo hardware will be enabled if 
 *     Hardware RX = [Teleinfo 1200] or [Teleinfo 9600]
 *     Hardware TX = [Teleinfo TX]
\*********************************************************************************************/

// declare energy driver and teleinfo sensor
#define XNRG_16   16
#define XSNS_98   98

#include <TasmotaSerial.h>

// teleinfo constant
#define TELEINFO_MESSAGE_BUFFER_SIZE  64
#define TELEINFO_MESSAGE_JSON_SIZE    512
#define TELEINFO_READ_TIMEOUT         150

// default voltage (not provided by teleinfo)
#define TELEINFO_VOLTAGE  230

// teleinfo data
int   teleinfo_enabled = false;
int   teleinfo_speed   = 0;
int   teleinfo_adps    = 0;
int   teleinfo_counter = 0;
uint8_t teleinfo_byte_counter  = 0;
char* teleinfo_message_buffer  = NULL;
char* teleinfo_message_temp    = NULL;
char* teleinfo_message_json    = NULL;
TasmotaSerial *teleinfo_serial = NULL;

// teleinfo counters
uint32_t teleinfo_total   = 0;
uint32_t teleinfo_base    = 0;
uint32_t teleinfo_hchc    = 0;
uint32_t teleinfo_hchp    = 0;
uint32_t teleinfo_bbrhcjb = 0;
uint32_t teleinfo_bbrhpjb = 0;
uint32_t teleinfo_bbrhcjw = 0;
uint32_t teleinfo_bbrhpjw = 0;
uint32_t teleinfo_bbrhcjr = 0;
uint32_t teleinfo_bbrhpjr = 0;
uint32_t teleinfo_ejphn   = 0;
uint32_t teleinfo_ejphpm  = 0;

/*********************************************************************************************/

void TeleinfoEvery200ms ()
{
  uint8_t index;
  uint32_t teleinfo_delta;
  uint32_t teleinfo_newtotal;
  ulong time_start;
  ulong time_now;
  char* token;
  char* etiquette;
  char* donnee;
  char* arr_token[4];
  char  serial_str[2];
  char  separator[2];

  // init timers
  time_start = millis ();
  time_now = time_start;

  // init serial string
  serial_str[0] = 0;
  serial_str[1] = 0;

  // loop as long as serial port buffer is not empty and timeout not reached
  while ((teleinfo_serial->available () > 0) && (time_now >= time_start) && (time_now - time_start < TELEINFO_READ_TIMEOUT))
  {
    // read caracter
    serial_str[0] = teleinfo_serial->read ();
    switch (serial_str[0])
    {
      // caracter is 0x02 (begin of message) 
      case 2:
        // reset message counter and initialise JSON 
        teleinfo_counter = 0;
        snprintf_P (teleinfo_message_temp, TELEINFO_MESSAGE_JSON_SIZE, PSTR("\"Teleinfo\":{"));
        break;
          
      // caracter is Ox03 (end of message)
      case 3:
        // generate final JSON and reset temporary JSON
        snprintf_P (teleinfo_message_json, TELEINFO_MESSAGE_JSON_SIZE, PSTR("%s}"), teleinfo_message_temp);
        snprintf_P (teleinfo_message_temp, TELEINFO_MESSAGE_JSON_SIZE, "");

        // update energy counter
        teleinfo_newtotal = teleinfo_base + teleinfo_hchc + teleinfo_hchp;
        teleinfo_newtotal += teleinfo_bbrhcjb + teleinfo_bbrhpjb + teleinfo_bbrhcjw + teleinfo_bbrhpjw + teleinfo_bbrhcjr + teleinfo_bbrhpjr;
        teleinfo_newtotal += teleinfo_ejphn + teleinfo_ejphpm;
        teleinfo_delta = teleinfo_newtotal - teleinfo_total;
        teleinfo_total = teleinfo_newtotal;
        energy_kWhtoday += (unsigned long)(teleinfo_delta * 100);
        EnergyUpdateToday ();
        break;
        
      // caracter is 0x0A (begin of line)
      case 10:
        // reset reception buffer
        snprintf_P (teleinfo_message_buffer, TELEINFO_MESSAGE_BUFFER_SIZE, "");
        break;

      // caracter is 0x0D (end of line)
      case 13:
        // init
        etiquette = NULL;
        donnee    = NULL;

        // if message has tab (0x09), set tab as separator, otherwise set space
        token = strchr (teleinfo_message_buffer, '\t');
        if (token != NULL) strcpy (separator, "\t");
        else strcpy (separator, " ");

        // loop to extract message tokens
        index = 0;
        token = strtok (teleinfo_message_buffer, separator);
        while (token != NULL)
        {
          if (index < 4) arr_token[index++] = token;
          token = strtok (NULL, separator);
        }

        // extract data without horodatage (3 tokens) or with horodatage (4 tokens)
        if (index == 3) { etiquette = arr_token[0]; donnee = arr_token[1]; }
        else if (index == 4) { etiquette = arr_token[0]; donnee = arr_token[2]; }

        // if message is valid
        if ((etiquette != NULL) && (donnee != NULL))
        {
          // add to JSON message
          if (teleinfo_counter++ > 0) snprintf_P (teleinfo_message_temp, TELEINFO_MESSAGE_JSON_SIZE, PSTR("%s,"), teleinfo_message_temp);
          snprintf_P (teleinfo_message_temp, TELEINFO_MESSAGE_JSON_SIZE, PSTR("%s\"%s\":\"%s\""), teleinfo_message_temp, etiquette, donnee);

          // instant current
          if (token == NULL) { token = strstr (etiquette, "IINST"); if (token != NULL) energy_current = atof (donnee); }

          // active power
          if (token == NULL) { token = strstr (etiquette, "PAPP"); if (token != NULL) energy_apparent_power = atof (donnee); }
          
          // maximum power outage (ADPS)
          if (token == NULL) { token = strstr (etiquette, "ADPS"); if (token != NULL) teleinfo_adps = atoi (donnee); }

          // Contract Base
          if (token == NULL) { token = strstr (etiquette, "BASE"); if (token != NULL) teleinfo_base = atol (donnee); }

          // Contract Heures Creuses
          if (token == NULL) { token = strstr (etiquette, "HCHC"); if (token != NULL) teleinfo_hchc = atol (donnee); }
          if (token == NULL) { token = strstr (etiquette, "HCHP"); if (token != NULL) teleinfo_hchp = atol (donnee); }

          // Contract Tempo
          if (token == NULL) { token = strstr (etiquette, "BBRHCJB"); if (token != NULL) teleinfo_bbrhcjb = atol (donnee); }
          if (token == NULL) { token = strstr (etiquette, "BBRHPJB"); if (token != NULL) teleinfo_bbrhpjb = atol (donnee); }
          if (token == NULL) { token = strstr (etiquette, "BBRHCJW"); if (token != NULL) teleinfo_bbrhcjw = atol (donnee); }
          if (token == NULL) { token = strstr (etiquette, "BBRHPJW"); if (token != NULL) teleinfo_bbrhpjw = atol (donnee); }
          if (token == NULL) { token = strstr (etiquette, "BBRHCJR"); if (token != NULL) teleinfo_bbrhcjr = atol (donnee); }
          if (token == NULL) { token = strstr (etiquette, "BBRHPJR"); if (token != NULL) teleinfo_bbrhpjr = atol (donnee); }

          // Contract EJP
          if (token == NULL) { token = strstr (etiquette, "EJPHN"); if (token != NULL) teleinfo_ejphn = atol (donnee); }
          if (token == NULL) { token = strstr (etiquette, "EJPHPM"); if (token != NULL) teleinfo_ejphpm = atol (donnee); }
        }
        break;

      // if caracter is anything else : message body
      default:
        // if caracter printable or tab, add it to message buffer
        if ((isprint (serial_str[0])) || (serial_str[0] == 9)) snprintf_P (teleinfo_message_buffer, TELEINFO_MESSAGE_BUFFER_SIZE, "%s%s", teleinfo_message_buffer, serial_str);
      }

    // update timer
    time_now = millis();
  }
}

bool TeleinfoPreInit ()
{
  bool detected = false;
  
  // if no energy sensor detected
  if (!energy_flg)
  {
    // if teleinfo TX and RX pins are set
    if ((pin[GPIO_TELEINFO_TX] < 99) && ((pin[GPIO_TELEINFO_1200] < 99) || (pin[GPIO_TELEINFO_9600] < 99)))
    {
      // set energy flag
      detected   = true;
      energy_flg = XNRG_16;

      // allocate and initialise buffers
      teleinfo_message_temp = (char*)(malloc (TELEINFO_MESSAGE_JSON_SIZE));
      snprintf_P (teleinfo_message_temp, sizeof(teleinfo_message_temp), "");

      teleinfo_message_json = (char*)(malloc (TELEINFO_MESSAGE_JSON_SIZE));
      snprintf_P (teleinfo_message_json, sizeof(teleinfo_message_json), "");

      teleinfo_message_buffer = (char*)(malloc (TELEINFO_MESSAGE_BUFFER_SIZE));
      snprintf_P (teleinfo_message_buffer, sizeof(teleinfo_message_buffer), "");
      
      // set speed
      if (pin[GPIO_TELEINFO_1200] < 99) teleinfo_speed = 1200;
      else teleinfo_speed = 9600;
    }
  }
  return detected;
}

void TeleinfoInit ()
{
  // if sensor has been pre initialised
  if (XNRG_16 == energy_flg)
  {
    // set serial port
    if (teleinfo_speed == 1200) teleinfo_serial = new TasmotaSerial (pin[GPIO_TELEINFO_1200], pin[GPIO_TELEINFO_TX], 1);
    else teleinfo_serial = new TasmotaSerial (pin[GPIO_TELEINFO_9600], pin[GPIO_TELEINFO_TX], 1);
    
    // flush and set speed
    Serial.flush ();
    Serial.begin (teleinfo_speed, SERIAL_7E1);

    // check port allocated
    teleinfo_enabled = teleinfo_serial->hardwareSerial ();
    if ( teleinfo_enabled == true) ClaimSerial ();
    else energy_flg = ENERGY_NONE;

    // set energy voltage
    energy_voltage = TELEINFO_VOLTAGE;
    RtcSettings.energy_kWhtotal = 0;
  }
}

void TeleinfoJSONAppend ()
{
  // if some teleinfo data are available
  if (strlen (teleinfo_message_json) > 0)
  {
    // append teleinfo data to mqtt message
    snprintf_P (mqtt_data, sizeof(mqtt_data), "%s,%s", mqtt_data, teleinfo_message_json);

    // reset teleinfo data
    snprintf_P (teleinfo_message_json, sizeof(teleinfo_message_json), "");
  }
}

void TeleinfoLoop ()
{
  // if teleinfo ADPS power outage is set
  if (teleinfo_adps > 0)
  {
    // add teleinfo data to mqtt message
    snprintf_P (mqtt_data, sizeof(mqtt_data), "{\"Teleinfo\":{\"ADPS\":%d}}", teleinfo_adps);

    // add teleinfo data to mqtt message
    MqttPublishPrefixTopic_P(TELE, PSTR(D_RSLT_SENSOR));

    // reset teleinfo ADPS value
    teleinfo_adps = 0;
  }
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

// energy driver
int Xnrg16 (uint8_t function)
{
  bool result = false;
  
  // swtich according to context
  switch (function)
  {
    case FUNC_PRE_INIT:
      result = TeleinfoPreInit ();
      break;
    case FUNC_INIT:
      TeleinfoInit ();
      break;
    case FUNC_EVERY_200_MSECOND:
      if (teleinfo_enabled == true) TeleinfoEvery200ms ();
      break;
    case FUNC_LOOP:
      if (teleinfo_enabled == true) TeleinfoLoop ();
      break;
  }
  return result;
}

// teleinfo sensor
bool Xsns98 (uint8_t function)
{
  // swtich according to context
  switch (function) 
  {
    case FUNC_JSON_APPEND:
      if (teleinfo_enabled == true) TeleinfoJSONAppend ();
      break;
  }
  return false;
}

#endif   // USE_TELEINFO
#endif   // USE_ENERGY_SENSOR
