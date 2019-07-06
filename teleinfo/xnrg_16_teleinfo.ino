/*
  xnrg_16_teleinfo.ino - France Teleinfo energy sensor support for Sonoff-Tasmota
  
  Copyright (C) 2019  Nicolas Bernaerts

    05/05/2019 - v1.0 - Creation
    16/05/2019 - v1.1 - Add Tempo and EJP contracts
    08/06/2019 - v1.2 - Handle active and apparent power
    05/07/2019 - v2.0 - Rework with selection thru web interface
    
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

// web configuration page
#define D_PAGE_TELEINFO               "teleinfo"
#define D_CMND_TELEINFO_MODE          "mode"

// teleinfo constant
#define TELEINFO_MESSAGE_BUFFER_SIZE  64
#define TELEINFO_MESSAGE_JSON_SIZE    512
#define TELEINFO_READ_TIMEOUT         150

// default voltage (not provided by teleinfo)
#define TELEINFO_VOLTAGE  230

// icon coded in base64
const char strIcon0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAAB0AAAAgCAMAAADZqYNOAAAJtHpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHjarZhpliQpDoT/c4o5AptYjsP63tygjz+fwCPXyurqeR1eGXjgOAiZySTKrL/+u81/+Pgi0UTJJdWULJ9YY/WNm2Lv57bOxvN9f8znmfvcb/zTbz1dgTbcn2k94xv98v5Cjk9//9xv8njmKc9Ez4PXhEFX1tWeceWZKPjb757fpj7vtfhhO8/fWGcK655Jv/6OGWdMoTN441egn++iqwQsCDU02sR3CDrI0ep95tuF/GvfmVV+7by3uy++s+3pD59dYWx6BqQvPnr6nfzad8dDHy1y7yt/emC3bfbj54Pv9p5l73V312LCU8k8m3pt5dwxsOPKcF5LXJk/4T6fq3IVlhkgNkGzcw3jqvN4e7vopmtuu3Xa4QYmRr98pvV++HD6Ssi++nFAiXq57TPwTBMKWA1QC3T7N1vcWbee9YYrrDwdI71jMscb3y7zq87/53qbaG+lrnO2XD9BC+zySkDMUOT0m1EA4vbjUzn+PZf5wBv7AdgAgnLcXNhgs/1O0cW9cyscnAPjxEZjb2i4PJ8JcBFrC8a4AAI2uSAuOZu9z87hxwI+Dct9iL6DgBPx05kNNiEkwCle1+ad7M5YL/52Iy0AIQRNBhoCCLBiFPiTY4FDTQJ6JCJJshSp0lJIMUlKKSfVqJZDjllyyjmXXHMr";
const char strIcon1[] PROGMEM = "ocQiJZVcSqmlVV8DEiY11WxqqbW2xqKNqRtvN0a01n0PPXbpqedeeu1tQJ8Rh4w08iijjjb9DJPwn2lmM8ussy23oNKKS1ZaeZVVV9twbYcdt+y08y677vaGmnvC9hNq7gtyv0fNPagpYvGMy++o0Z3zawqnciKKGYj56EA8KwIQ2itmtrgYvSKnmNnqCQrxoOZEwZlOEQPBuJyX7d6we0fut7gZif8IN/8Tckah+zeQMwrdg9x33H6B2mwno4QDkEah+tSGjbDtJM2X5rug9ecOEfyx3U6H2tR7quKX9NWynbGZOrAJdCWvzpK+p1RGWxm0CKXQ+hy+4e+FyDm3cnCrd520LYuXV84Rx/puxEX+lfb3tvy+NR+MDqntWI/xqzQ348pt7RRsqp3VZDYocRe2VnJfMxd325CMD7ziSl4t1Snp2BxCt2f6Zv+4NX87sA88MOs6LX1ZQu2OokSxPevOUK03b+Mn4wbjcW63baL2q2wJOK9haWpdSESznZ2JFg+fW/PTg7cWXk1ZNQhFgkwkstWxZa6Re5hOuM+oXjGjBY2Lte6bBI6T6gS3o6A5eWm96hRjz9xkktVkboJwOF+YceprgvIaJzuGmatIbkxORux4n8iSXuRxPlbEf+7sPmbruGTolKsMQijU1CM8SKkTuIkNzYhpUxcH+7HpMFZSwLSh45qAiN/NrcHOEv/6zCSRFLoL/VJIPNnnPoL7dRY0YmXvDJP2ppHf4Vu1HTlU";
const char strIcon2[] PROGMEM = "kDSggie4jzf9llGRmpHK7Di7+zZXFI2oE1YtVBMX9k4rYttY7C4WvxjOoMMTatFe0bQehFlDa9UyNRJYTgTshGfpRdjwJGtoOPRVLnLoWstlSA7AjD1sbtURCSF8IJeFzb+5n+KFmDeKIJnSrm92vHxZoWKrB9BDH8H+jppeWlNqHqCMznnCFBmBxQusIPeuGLCCZRxsZwvC2Jp72p4YQOPqiuBxmBWmtIFCjjFVOH0/2yLnvziNxq6hhOZvOTQ9rZJ2xDplK0NuyDzMNwGf1IjMeVfHcmPkEmfKVx1cF8ro1tluBsIquHMxXHADYxJxfJcNqkd04NHGiuIVWsKVxxupL4TR4Q7xUGWsHdnDIbunlnujcBsQxqio+YHD1XukUiw8Y6neIbl4sPSoKg5sZexLcg97x1RqL4jWibXcjbovXPdlnNH7YeM6sjGbdOIMxSO9oPLuLtKpjN6FFYapDUaNkFoZgIWk2eGhfEo8bGqdncXf7QlcHM9brUe2r7x626cKm4YFdqcVRyOumpIeN6rXVrebaR3wbwyf7RIaklCHReQvdthHJiwmy8Uy+TTcVUK03en+2lQWSD03R/Ddz6352qFEVj7UHSp8yKMlqxTAQSTjHcuyJzZVH99JL5nT0RvnjxJNjy1jafRRctQRatm2akBvnM+BDZFyuhzSQ/4U6BKXTc0MjSjyK0cCImQuxgJwQhioJ15u8Y0gEfIUXiajTLI7GftTpBt80/5A+Kl9";
const char strIcon3[] PROGMEM = "phQl/Ev6CTNAU9d3kvswYe5Ww5qLBMQIgldVNgFZKNP1zJTsFwUiqG/KzVGxxazLpVrK2q0bCok+KIC0LTfxTU3XFyk0qaKxBCJhrvR3VSimIpXPE5LxSTDGH0rfyVuBx72Pl58odypyvRaTPnPlOn0hECLl5EcVd+YLI77J+peFv6z77lWjVQY1bEJ64IjKWrk5GjKc+oeTNSY8O9X8+bz7YYkCGqa2RA1J9jpWRv+KxjD9urukHHCpF+rII+8ZRbhbKq6/b8V8ZrtTvHUBTWwUtD0TYlUBDYFtQri5U0dgoMPaHGh5BLJhJKN5hQhj+yRqFrVYtZWsbHJrAiKmV75U1lo8K5WPhwilmMr3akSlZnI/9dh7osgGAhrslV9E21bBRWge6ReynRY+QBkIN4MRPoVrxZN0kQk5fiazap35gPz7ktf8pLR+PIXgH1R/KtomfRNtubnvT+rZXgLWayEkRvEAlgPuPLnwUcB0FbA9CngE8IgV8pQRzkDeVUzvS974pqFa3/TBry+FwKMOQvUhPG6yJpUPKh/JXTXdUKdgV3HGG4U1tETOjzpnTSVXCDoy4CkTOC+InpBu9aRVBvnwZZyYY52qImmpqbRsDifUN7uNXN/fyZnS8sm12amYIhP9ViYjkmrMD0joMWANlGWeppHTI8mxUjCTDLSLaot6Lp36qydvWjnVx+v4oK1m7gb/s5bCnH/IpwTEbWPiqMAZltubu1FMrcyH8agGmk9e";
const char strIcon4[] PROGMEM = "7I3t4mgZURFkwUKZs5euffMGiS6nrsejdw1OGg0saICCICLThpmGfnvXA9FOmah5G3GbnCwn504fdJ86MTIN6I1Bs+hRVRXOnGJjcMZ9xf9wk73pPuFm310jOQBAANRctTbWHUo4aNgDWhglma66QyTiFiCpKtgE6ElX28828SzlA8z3p0zySgV0o2RlrfrIa1imalT+tC5l33PoaSCcFyLMS3rKVPizauPu0e0edAp/z5AbEVlDS2g8ZR5Dm56ewOPkajeClj6L0gL3aPm9KTjGcZpOwRsocyZ3MzBTIsINA5JZ/08CMAAY5bt2kZwocZ1z17B331A1UWWVuhNqSk7R4ClkO0P1RREfcCb9m+N4Re3+B/orwRn8e1zgAAAAe1BMVEWQnuJOgb1Og7w/ia1Ig7JCi6lKgrtCi6lAhLhCi6lNgb1Kfr9Ogb3///86apNEdqn9/v9KgLlWhsD1+Puhu9xKe7Q8bZfp7/b7/P2zyN+Qrs7Q3eq/0eVAcaJ/osfe5/A4apNul8ZLeJ5dh6g/cJs6apVlkL6ku884aJFDET/aAAAAAXRSTlMAQObYZgAAAAFiS0dEAIgFHUgAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAAHdElNRQfjBh4LJRMDmmhYAAAA7klEQVQoz6VS0Q7DIAj0C3ycEQxmsIj//4VD66Zb127JSEyEq3B31LklWITVHYSvtaZwAlY8AKV+Rw/HdjS5s8d5X57wdUe5";
const char strIcon5[] PROGMEM = "+DJuGvtTU83CwwLrJ6sldQs/ydi3slJvcWnp7ZHRaK8KZfIfrfwUE0K6VF82/tBHvzoMmsIoBMQcjux/Fc9ndZtyk48b7KypCfy0Xnra4deeRXgq7n7w3jxdVglvi6+Sl18I3lZLOCSjEeO4TMYYcfFDNf9kzr8h4uix/dKyfkZQAqTcvWAkS5US4URLpjBQuyaFNMQT0RVNQwBtkUkjqlqFCM753AEIHyqOp8BqRwAAAABJRU5ErkJggg==";
const char *const arrIconBase64[] PROGMEM = {strIcon0, strIcon1, strIcon2, strIcon3, strIcon4, strIcon5};

// teleinfo data
bool  teleinfo_configured = false;
bool  teleinfo_enabled    = false;
int   teleinfo_adps       = 0;
int   teleinfo_counter    = 0;
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

// get teleinfo mode (baud rate)
uint16_t TeleinfoGetMode ()
{
  uint16_t actual_mode;

  // read actual teleinfo mode
  actual_mode = (uint16_t) (Settings.sbaudrate * 1200);

  // if outvalue, set to disabled
  if ((actual_mode != 1200) && (actual_mode != 9600)) actual_mode = 0;
  
  return actual_mode;
}

// set teleinfo mode (baud rate)
void TeleinfoSetMode (uint16_t new_mode)
{
  // if within range, set baud rate
  if ((new_mode == 0) || (new_mode == 1200) || (new_mode == 9600))
  {
    // save mode
    Settings.sbaudrate = (uint8_t) (new_mode / 1200);

    // ask for restart
    restart_flag = 2;
  }
}

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

          // instant current and active power
          if (token == NULL)
          { 
            token = strstr (etiquette, "IINST"); 
            if (token != NULL)
            {
              energy_current      = atof (donnee);
              energy_active_power = energy_current * energy_voltage;
            }
          }

          // apparent power
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
  // if no energy sensor detected
  if (!energy_flg)
  {
    // if serial RX and TX are configured
    if ((pin[GPIO_TXD] < 99) && (pin[GPIO_RXD] < 99))
    {
      // set configuration flag
      teleinfo_configured = true;

      // set energy flag
      energy_flg = XNRG_16;

      // allocate and initialise buffers
      teleinfo_message_temp = (char*)(malloc (TELEINFO_MESSAGE_JSON_SIZE));
      snprintf_P (teleinfo_message_temp, sizeof(teleinfo_message_temp), "");

      teleinfo_message_json = (char*)(malloc (TELEINFO_MESSAGE_JSON_SIZE));
      snprintf_P (teleinfo_message_json, sizeof(teleinfo_message_json), "");

      teleinfo_message_buffer = (char*)(malloc (TELEINFO_MESSAGE_BUFFER_SIZE));
      snprintf_P (teleinfo_message_buffer, sizeof(teleinfo_message_buffer), "");
    }
  }
  
  return teleinfo_configured;
}

void TeleinfoInit ()
{
  uint16_t teleinfo_mode;

  // get teleinfo speed
  teleinfo_mode = TeleinfoGetMode ();

  // if sensor has been pre initialised
  if ((teleinfo_configured == true) && (teleinfo_mode > 0))
  {
    // set energy voltage
    energy_voltage = TELEINFO_VOLTAGE;
    RtcSettings.energy_kWhtotal = 0;

    // set serial port
    teleinfo_serial = new TasmotaSerial (pin[GPIO_RXD], pin[GPIO_TXD], 1);
    
    // flush and set speed
    Serial.flush ();
    Serial.begin (teleinfo_mode, SERIAL_7E1);

    // check port allocated
    teleinfo_enabled = teleinfo_serial->hardwareSerial ();
    if ( teleinfo_enabled == true) ClaimSerial ();
  }

  // if teleinfo is not enabled, reset energy flag
  if ( teleinfo_enabled == false) energy_flg = ENERGY_NONE;

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

#ifdef USE_WEBSERVER

// Display Teleinfo icon
void TeleinfoWebDisplayIcon (uint8_t height)
{
  uint8_t nbrItem, index;

  // display img according to height
  WSContentSend_P ("<img height=%d src='data:image/png;base64,", height);

  // loop to display base64 segments
  nbrItem = sizeof (arrIconBase64) / sizeof (char*);
  for (index=0; index<nbrItem; index++) { WSContentSend_P (arrIconBase64[index]); }

  WSContentSend_P ("'/>");
}

// Teleinfo mode select combo
void TeleinfoWebSelectMode (bool autosubmit)
{
  uint16_t actual_mode;
  char     argument[TELEINFO_MESSAGE_BUFFER_SIZE];

  // get mode
  actual_mode = TeleinfoGetMode ();

  // selection : beginning
  WSContentSend_P (PSTR ("<select name='%s'"), D_CMND_TELEINFO_MODE);
  if (autosubmit == true) WSContentSend_P (PSTR (" onchange='this.form.submit()'"));
  WSContentSend_P (PSTR (">"));
  
  // selection : disabled
  if (actual_mode == 0) strcpy (argument, "selected"); 
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), 0, argument, D_TELEINFO_DISABLED);

  if (teleinfo_configured == true)
  {
    // selection : 1200 baud
    if (actual_mode == 1200) strcpy (argument, "selected");
    else strcpy (argument, "");
    WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), 1200, argument, D_TELEINFO_1200);

    // selection : 9600 baud
    if (actual_mode == 9600) strcpy (argument, "selected");
    else strcpy (argument, "");
    WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), 9600, argument, D_TELEINFO_9600);
  }

  // selection : end
  WSContentSend_P (PSTR ("</select>"));
}

// append VMC buttons to main page
void TeleinfoWebMainButton ()
{
  // beginning
  WSContentSend_P (PSTR ("<table style='width:100%%;padding:5px 10px;'><tr>"));

  // teleinfo icon
  WSContentSend_P (PSTR ("<td>"));
  TeleinfoWebDisplayIcon (32);
  WSContentSend_P (PSTR ("</td>"));

  // select mode
  WSContentSend_P (PSTR ("<td><form action='%s' method='get'>"), D_PAGE_TELEINFO);
  TeleinfoWebSelectMode (true);
  WSContentSend_P (PSTR ("</form></td>"));

  // end
  WSContentSend_P (PSTR ("</tr></table>"));
}

// Teleinfo web page
void TeleinfoWebPage ()
{
  char argument[TELEINFO_MESSAGE_BUFFER_SIZE];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // get teleinfo mode according to MODE parameter
  if (WebServer->hasArg(D_CMND_TELEINFO_MODE))
  {
    WebGetArg (D_CMND_TELEINFO_MODE, argument, TELEINFO_MESSAGE_BUFFER_SIZE);
    TeleinfoSetMode ((uint16_t) atoi (argument)); 
  }
}

#endif  // USE_WEBSERVER

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
  bool result = false;
  
  // swtich according to context
  switch (function) 
  {
    case FUNC_JSON_APPEND:
      if (teleinfo_enabled == true) TeleinfoJSONAppend ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_TELEINFO, TeleinfoWebPage);
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      TeleinfoWebMainButton ();
      break;
#endif  // USE_WEBSERVER

  }
  return result;
}

#endif   // USE_TELEINFO
#endif   // USE_ENERGY_SENSOR
