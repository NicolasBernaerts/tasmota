/*
  xdrv_98_lightctrl.ino - Light switch controler handling push button, motion detector and permanent switch with Sonoff
  
  Copyright (C) 2019  Nicolas Bernaerts

    08/07/2019 - v1.0 - Creation
    14/07/2019 - v1.1 - Conversion du drv

  Input devices should be configured as followed :
   - Switch 2 = Light ON detection

  Settings are stored using some unused display parameters :
   - Settings.display_refresh = Timeout before switching off the light (mn) 

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

#ifdef USE_LIGHTCTRL

/*********************************************************************************************\
 * Universal Impulse switch with push buttons and motion detector as input
\*********************************************************************************************/

#define XDRV_98                          98
#define XSNS_98                          98

// in the configuration, select Switch2 (index starts from 0)
#define LIGHT_BUTTON_INDEX               1

#define D_PAGE_LIGHTCTRL                 "lc"
#define D_CMND_LIGHTCTRL_SWITCH          "switch"
#define D_CMND_LIGHTCTRL_TIMEOUT         "timeout"

#define D_JSON_LIGHTCTRL                 "LightControler"
#define D_JSON_LIGHTCTRL_RELAY           "Relay"
#define D_JSON_LIGHTCTRL_STATE           "State"
#define D_JSON_LIGHTCTRL_TIMEOUT         "Timeout"
#define D_JSON_LIGHTCTRL_TIMELEFT        "TimeLeft"
#define D_JSON_LIGHTCTRL_FORCED          "Forced"
#define D_JSON_LIGHTCTRL_OFF             "Off"

// impulse icon coded in base64
const char strIcon0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAAEAAAAAgCAMAAACVQ462AAAK4XpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHjarZlpdjM5b4X/cxVZQnEml8PxnOwgy88DsCTLtuSv0ydSv64SySIB3Iup2qz/+e9t/ouPTzWYEHNJNaWLT6ihusZNuc7nXO0V9K9+wj3F72/jxj0mHEOeqz8/07rXN8bj1wP53sn27+Mmj3ufcm90Tzw29HKynHavK/dG3p1xe/829X6uhRd17n81X9/0+fk7ZIwxI/t5Z9zy1l/8LXKKRwJffeOa+cs9iy5fuHc6br17bzvzvP1hvPLBdle7V/jvpjBXuhekHza6x218bzu10KtE9nHrvk/Ecs3r9fNiu71n2Xsd7VpIWCqZW6nr3kLvWNgxpdfHEt/Mv8h91m/lW1BxgNgEzc53GFutw9rbBjtts9suvQ47EDG45TJX54bzOlZ8dtUNBSXI126XgWEaEHF+gJpn2D1lsXpu1fOGFTWnZaWzbAZyv7/m3eC/+T432luoa60YsxxbIZcT1iCGICd/WQUgdt82jWpf/ZonrF8fAdaDYFQzFxRsVz9b9Gi/uOUVZ8+6eAVz093meW+AiTg7Ioz1IHAl66NN9srOZWuxYwGfhuTOB9dBwMbopjUbbLxPgFOcnM0z2epaF90ZJrQARPQJtyniKIAVQoQ/ORQ41KKPwcQYU8yxxBpb8imkmFLKSWJUyz6HHHPK";
const char strIcon1[] PROGMEM = "OZdccyu+hBJLKrmUUkurrnpCWKypZlNLrbU1Dm1s3Xi6saK17rrvoceeeu6l194G9BlhxJFGHmXU0aabfuL+M81sZpl1tmUXVFphxZVWXmXV1TZc236HHXfaeZddd3uidqP6HTX7A7m/UbM3aoJY0HX5CzWGc35sYSWcRMEMxFywIJ4FAQjtBLOr2BCcICeYXdXhFNGBmo0CzrSCGAiGZV3c9ondF3J/4mZi+D/h5j4hZwS6/w/kjEB3I/cbtzeozaYZxStA4oVi08tvAtvOqbnSXIf+l97htM/rvgIPx7xXioTFntnsuiYWWzvHddXdg7DCjI5LJ05luodEiCrZzuF3SvoIShNV58ys6jF37orLa0x9AsAkhLESidbkkdzBbFg3h4rS0hvh/ryaXxN+WMSpHFhr8gizupydyrS7e3QcY2wdkYkeW1HVzS/dwy23Ss2IyC1CizFinui+lgrdCOl6I/m+mcfNufanOGKdp0CI87TOdCqhdyNjHWy3SBZhm2QH3rFnbR3hUlaj5yg4zy9g3qnURbTYr9AzZjHIUkt5kfFfXfdlPrDkk6U+KG63+ZKafdNBCLkbxrBQOggx5NxW6mEn8+XerNWg6/GtbRDoBvANd9tueK+tCLanyxMZYt341GqxExH8YItl5WHDkJMHB9upKdtz7kzphEjbGmzfu4QMLPXI7qzOxVWMivo1JzOMxFXjrFE2QZ59dDmaiB4q8TngoaZ51VPU7LOy";
const char strIcon2[] PROGMEM = "qbWLB4+So8CJmVVQGcY2MqHDbJibW8NXE+1SYJRn0VU0iuGwKcZMfLCuLyeAEIr2GqwqVU0O53DwMRVD0tH2OS8IyaJGPdn93A1zhioLrkIkInKxfeC41Se+6Gc5D7/KZUQwN4haSUxMqlB1Ssyg03Qz6zhkXsCZxRwEuxXTEAtwfBorZ6aCaWv3P1DyZDzl2OgPsQg8fdqf8pojsCXhkBIKzwwvQk2CN4Ek6gNritnOBGroFKrJJFNxiZ3MOzvI098HjxSc9pEQ5jcj/h0hzG9G/EmIY5s7Jr1Mdsqa3L2wxDMXThKol5q78cuPPXBloqxuQepe8GWdVJH3BWHKFohMKr1fsuskqUZL3q21yFXanv9wtQu22d6HJ4zsqz7O2i0Rk9F0y2Gju7m7kCcvzkWb3JVpPv5imk94/wvViFNPqrUn1WBaIiARegnKk1B9xRe/6eP4Daip61AUTGSEEJQC2/UkG8qSlDEeARIzA8eL183wzesMbncyan54nbLm1e+KVc8pyo6uQHSNYWzrxUt3wkZeAFw4MQbZbE0FUqg1dl6dJFJVDHvYo2rME2xrzw8tmmiRzK2GkN0/1XBhrZFmrHZc+30m29RmiLVig5V7JgN2+VNKlNpJOc7DbNjx6QzfOy6uTHH2iygQsvNBI5y8+py45BFJIyvkQcllxQBilYQtYASNARNZ0kQtWVyRsxDfFAnnytRFdalynYDOubP0AE+F83AIhvnblXrE";
const char strIcon3[] PROGMEM = "xmwcpj5hj/dL2Lt0tXTkU0II2m7nT1bxQk6KXvzumsdl9SDKPFzhuaF57HiHKx1HfDlKhvUUylM45cbuiVpnS8b3ScwZ26J9ZDzjtJlKMlD4nTq/DwitJUWXEr/xYKe0lGTc9bjEIqn2ULXiLN5jMMJhNVVS2lyKDuCh5HSKRKRADlbewbjP0fEZOKf5GVLbEq4PTflaLX2K5yxIdQ700VrG/LWYCHHhXKdG+6OAkDiq8L9NyOTsC1ErxVaQJoMwOmjOcYqdDiARZEcVWCjaDe2Br5COZE/IYGkcNn/5qFTaQCDOoB7aIKjEyOOgjZDj1UGnqQM2JqkZ1qQrmP80Rf/M0OaECodrYH0pIounQBbZK2GsS05iKTLHfUIHzgElOs4kHpkINnnjuWxE3HDYqUvEwOyIjKHFIGhPW0HVv0RAnq1KxAMFOuEFTLreURjUrgCIf3Qc0BdjDdwLVSXMpbeBxfyOLE0eHhkna1Lyeh9Koi8MdGLcS3bD2wriyiq2QHpMaWiRV+p4vG04DdZ8TpFupqNkpElG4bRaiydnnJ8/uhEjN68+88NlSHcudUDQfk3VkaKgj+MJ8tZyt3C3EFMiw4O3p7lqEpOFtxpWelLaytSZ0GGpvpOW/y0myhqFXP1SKu6pAaTeoW5KAtAEjHkvQC/x01rzXHxNOE08lt5DGH23drTWAoTEvZ604NEJHRZWnYDYk9G2x4ZGSUL9/+j6vFCrakZ9UEu3xXfy";
const char strIcon4[] PROGMEM = "g1xJSEGgV3INjZApfrFL8s9rhgfGlwx/ysAu3nxSfJKMKineNFpv/K5Dc6km3yamdO1xLcTT8ueYmXMLjlO2190l+BMDUpoTjHO3gw1hAKGpTVEBFp6WpnxuUXOnuTcC5iP/LcL9ab5/9N70gFuSGYeHrSEF8vo+RBbiQ5rkNa9BNtBnNdLQHNMJctsJwqMOSiJyAotWpSaQWODujJOrKilBQ5hg3gbz/Ls8LimLzYezWiJKBNAiUepH7deWPWmvKZMp144xcY6avu/693HmnEc5qDsVpdnUzUjr9q6VWps7lwv69HXzadAWjLsnuMiFxmvOwGca9dppJvzvfII3OE+iPK3hFlezQchI6NYqb5s5q+RWcC47iC+nUag/KD0Jz5KNYyCm+flq2B+KKl0NxXf9WXxHTbiXvHkYhwLvV1kt66uW9eZbXf/j+ceit7X/j43N8/wq0W1rdBMXecY3yKy54K1acVwkQfqcYsR9pAXXcsWKk9NRVM2enUod15HClT6BDNukkJo5v9YY/Cf/cySax83b69dble+V6JvXVEbeU9n3HlY+vHrx0gnA5xPJoo0hFJXobdvy+RWPFDS/86B5vFSp5Oyylv38iue8W/r05s0Qbq+a1rGKt9c/7LB+Xc2z9Xq+CPxnlgo/FDc/gKinqj1Siw9GMQaVzqh3YoKS7g3KN/xfbybxqHG3nSIUIn3Z5108Bzv60Q4h37+5ekLz6d1bcLO42O9mZATz";
const char strIcon5[] PROGMEM = "iYv/9PqgifnNk4/Wsp+UFzzMU3KVWzj1lrpV3qiY/wXNfgLLEqQDdQAAAltQTFRFIW4C/////v7+/f398/Pz4eHh8vLy4uLi/Pz8q6urNzc3+/v7qampODg4+vr6+Pj4qKioNjY29/f3Ozs7rKysOjo6xcXFiYmJioqKh4eHk5OTXFxcHh4elJSUnp6eysrKhYWFPT09Xl5eW1tbT09PUFBQUVFRV1dXYGBgUlJSTExMDg4O6enps7OzDw8P6+vrU1NTi4uL9vb2ISEh5ubmm5ubQkJChoaG8fHxHR0d5eXlMTExeXl5DQ0Nzc3NoKCgaWlpRkZGhISEfHx8fn5+cnJyr6+vKCgot7e3tbW1CgoKZWVlWlpa4+PjEhISzMzMra2tMzMzgICAJycnwsLC9fX17+/vFBQU7e3tKysrGRkZFxcXGxsbICAguLi4EBAQAAAA6urq6Ojotra2y8vLpKSkqqqqZ2dnSEhIJiYmubm5srKy2dnZDAwMLy8viIiI+fn59PT03NzceHh43t7e5OTk5+fn2traYmJiIiIiHx8fFhYWCwsL7OzsLS0tpqamNTU13d3dOTk5x8fHfX19/v//+/3//f7/9fv//v7/+Pz/+f3/7Pf//P7/iM//KKn/4vP/b8b/9Pr/2O//7vj/acP/xej/Trf/zOr/wef/ltT/TrX/ptz/x+n/QLP/yun/Xbz/vOX/q93/eMf/3PH/gs3/l9X/Z8L/FZv/Oq//";
const char strIcon6[] PROGMEM = "YsH/Yrz/AZP/DJ3/W73/WLv/AJL/c8T/p9z/CJz/g83/ktP/RLT/idD/ULn/RrX/WLz/fMr/Vrv/i9H/9/z/6Pb/6fb/8vr/+v3/8Pn/5vX/7/n/+fz/9vv/7fj/UfUfIQAAAAF0Uk5TAEDm2GYAAAABYktHRACIBR1IAAAACXBIWXMAAA6/AAAOvwE4BVMkAAAAB3RJTUUH4wcTBh0MXt+7aAAAAMJJREFUSMdjYEAFCQkMRAIcKkcNGLEGJOAFOA0IghmSICKGBJqbkXli9gmEXZFQxMeBCzDbJRD2RUIRMyMCMME1M4F4JBvAlCNnCgV2TIQMSEgQwDCAud6usQQCwogwgAHTgFQdDmYIQPICIj4gLEOUeEQzIJEJOUSwG8BAqgH4ExJ1DID6m5mbFAOCEAbweaZCQH2xPhku4GBkEjKCAiEU/cQZkMMSigBGRqHIoJkYAzKQM58Wal4MThgt0kYNIMYAAFqRdovyYmCAAAAAAElFTkSuQmCC";
const char *const arrIconBase64[] PROGMEM = {strIcon0, strIcon1, strIcon2, strIcon3, strIcon4, strIcon5, strIcon6};

// state enumeration
//  LIGHTCTRL_OFF     : switched off
//  LIGHTCTRL_TIMEOUT : switched on with timeout (motion detection or push button)
//  LIGHTCTRL_FORCED  : switched on
enum LightCtrlState { LIGHTCTRL_OFF, LIGHTCTRL_TIMEOUT, LIGHTCTRL_FORCED };

// impulse commands
enum LightCtrlCommands { CMND_LIGHTCTRL_SWITCH, CMND_LIGHTCTRL_TIMEOUT };
const char kLightCtrlCommands[] PROGMEM = D_CMND_LIGHTCTRL_SWITCH "|" D_CMND_LIGHTCTRL_TIMEOUT;

// variables
uint32_t lightctrl_stop  = 0;               // timestamp when light should be switched off
uint8_t  lightctrl_state = LIGHTCTRL_OFF;   // light current state (OFF, ON thru motion or ON forced)

/*********************************************************************************************/

// get light controler feedback state
bool LightCtrlIsLightOn ()
{
  uint8_t button_status;
  
  // check if push button pressed
  button_status = SwitchGetVirtual (LIGHT_BUTTON_INDEX);
  return (button_status == PRESSED);
}

// set light controler timeout (mn)
void LightCtrlSetTimeout (uint8_t timeout)
{
  // return value
  Settings.display_refresh = timeout;
}

// get light controler timeout (mn)
uint8_t LightCtrlGetTimeout ()
{
  uint8_t timeout;

  // read timeout
  timeout = Settings.display_refresh;

  // if not set, force to 15 mn
  if (timeout == 0)
  {
    timeout = 15;
    Settings.display_refresh = timeout;
  }
  // return value
  return timeout;
}

// switch relay off
void LightCtrlSwitchRelayOff ()
{
  uint8_t relay_state;

  // reset values
  lightctrl_state = LIGHTCTRL_FORCED;
  lightctrl_stop  = 0;

  // switch relay OFF
  ExecuteCommandPower (1, POWER_OFF, SRC_IGNORE);
}

// switch relay on
void LightCtrlSwitchRelayOn (bool enable_timeout)
{
  uint8_t relay_state;
  uint8_t timeout_mn;
  uint32_t   timeout_ms, time_start;

  char tstart[32];
  char tstop[32];

  // if asked, set switch off time
  if (enable_timeout == true)
  {
    // set light start to now
    time_start = millis ();

    // calculate timeout in ms
    timeout_mn = LightCtrlGetTimeout ();
    timeout_ms = 60000 * (uint32_t) timeout_mn;

    // set timeout
    lightctrl_state = LIGHTCTRL_TIMEOUT; 
    lightctrl_stop  = time_start + timeout_ms;

    sprintf(tstart,"start : %lu", time_start);
    sprintf(tstop,"stop : %lu", lightctrl_stop);
    AddLog_P(LOG_LEVEL_INFO, tstart);
    AddLog_P(LOG_LEVEL_INFO, tstop);
  }
  else
  {
    // no timeout
    lightctrl_state = LIGHTCTRL_FORCED; 
    lightctrl_stop  = 0;
  }

  // switch relay ON
  ExecuteCommandPower (1, POWER_ON, SRC_IGNORE);
}

// light controler switch command
void LightCtrlSwitch (uint8_t action)
{
  // handle action
  switch (action)
  {
    // switch off
    case LIGHTCTRL_OFF:
      AddLog_P(LOG_LEVEL_INFO, "Switch OFF");
      LightCtrlSwitchRelayOff ();
      break;

    // action is a light with timeout
    case LIGHTCTRL_TIMEOUT:
      AddLog_P(LOG_LEVEL_INFO, "Switch ON (with timeout)");
      LightCtrlSwitchRelayOn (true);
      break;
 
      // action is a forced switch
    case LIGHTCTRL_FORCED:
      AddLog_P(LOG_LEVEL_INFO, "Switch ON (forced)");
      LightCtrlSwitchRelayOn (false);
      break;
  }
}

// light controler initialisation (switch relay OFF)
void LightCtrlInit ()
{
  // force relay off
  LightCtrlSwitch (LIGHTCTRL_OFF);
}

// Show light controler JSON status (for MQTT)
void LightCtrlShowJSON (bool append)
{
  TIME_T  current_dst;
  uint32_t   time_now, time_left;
  uint8_t relay_state, light_timeout;

  // update current status
  relay_state   = bitRead (power, 0);
  light_timeout = LightCtrlGetTimeout ();

  // start message  -->  {  or  ,
  if (append == false) snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("{"));
  else snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,"), mqtt_data);

  // "LightCtrl":{"Relay":1,"State":1,"Timeout":15,"TimeLeft":"3:15"}
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_LIGHTCTRL "\":{"), mqtt_data);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_LIGHTCTRL_RELAY "\":%d,"), mqtt_data, relay_state);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_LIGHTCTRL_STATE "\":%d,"), mqtt_data, lightctrl_state);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_LIGHTCTRL_TIMEOUT "\":%d,"), mqtt_data, light_timeout);

  // handle action
  switch (lightctrl_state)
  {
    // light is OFF
    case LIGHTCTRL_OFF:
      snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_LIGHTCTRL_TIMELEFT "\":\"%s\""), mqtt_data, D_JSON_LIGHTCTRL_OFF);
      break;

    // if light is ON with a timeout
    case LIGHTCTRL_TIMEOUT:
      // calculate temporisation left
      time_now  = millis ();
      time_left = lightctrl_stop - time_now;

      // convert it to mn and sec
      BreakTime (time_left/1000, current_dst);

      // display temporisation left
      snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_LIGHTCTRL_TIMELEFT "\":\"%d:%d\""), mqtt_data, current_dst.minute, current_dst.second);
      break;
 
      // light is forced ON
    case LIGHTCTRL_FORCED:
      snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_LIGHTCTRL_TIMELEFT "\":\"%s\""), mqtt_data, D_JSON_LIGHTCTRL_FORCED);
      break;
  }

  // if not in append mode, publish message 
  if (append == false)
  { 
    // end of message   ->  }
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);

    // publish full state
    MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
  }
}

// update light controler state 4 times per second
void LightCtrlEvery250MSecond ()
{
  uint32_t time_now;
  bool  light_on;

  // update current status
  time_now = millis ();
  light_on = LightCtrlIsLightOn ();

  // if light not switched on before and light detected on
  if ((lightctrl_state == LIGHTCTRL_OFF) && (light_on == true))
  {
    // switch the relay ON with timeout
    LightCtrlSwitch (LIGHTCTRL_TIMEOUT);
  }

  // if light is switched on with timeout set and reached, switch off relay
  if ((lightctrl_state == LIGHTCTRL_TIMEOUT) && (time_now > lightctrl_stop))
  {
    // switch the relay OFF
    LightCtrlSwitch (LIGHTCTRL_OFF);
  }

  // if light was forced on and is now off, set state to OFF
  if ((lightctrl_state == LIGHTCTRL_FORCED) && (light_on == false))
  {
    // state is OFF
    AddLog_P(LOG_LEVEL_INFO, "Detected OFF");
    lightctrl_state = LIGHTCTRL_OFF;
  }
}

// Handle light controler MQTT commands
bool LightCtrlCommand ()
{
  bool serviced = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kLightCtrlCommands);

  // handle command
  switch (command_code)
  {
    case CMND_LIGHTCTRL_SWITCH:               // switch on or off
      LightCtrlSwitch (XdrvMailbox.payload);
      break;
    case CMND_LIGHTCTRL_TIMEOUT:             // set light timeout (mn)
      LightCtrlSetTimeout (XdrvMailbox.payload);
      break;
    default:
      serviced = false;
  }

  // send MQTT status
  if (serviced == true) LightCtrlShowJSON (false);
  
  return serviced;
}

#ifdef USE_WEBSERVER

// light controler icon
void LightCtrlWebDisplayIcon (uint8_t height)
{
  uint8_t nbrItem, index;

  // display img according to height
  WSContentSend_P ("<img height=%d src='data:image/png;base64,", height);

  // loop to display base64 segments
  nbrItem = sizeof (arrIconBase64) / sizeof (char*);
  for (index=0; index<nbrItem; index++) { WSContentSend_P (arrIconBase64[index]); }

  WSContentSend_P ("'/>");
}

// light controler configuration button
void LightCtrlWebConfigButton ()
{
  // beginning
  WSContentSend_P (PSTR ("<table style='width:100%%;'><tr>"));

  // impulse switch icon
  WSContentSend_P (PSTR ("<td align='center'>"));
  LightCtrlWebDisplayIcon (32);
  WSContentSend_P (PSTR ("</td>"));

  // button
  WSContentSend_P (PSTR ("<td><form action='%s' method='get'><button>%s</button></form></td>"), D_PAGE_LIGHTCTRL, D_LIGHTCTRL_CONFIGURE);

  // end
  WSContentSend_P (PSTR ("</tr></table>"));
}

// light controler web configuration page
void LightCtrlWebPage ()
{
  bool    updated = false;
  uint8_t light_timeout;
  char    argument[32];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  if (WebServer->hasArg("save"))
  {
    // save timeout
    WebGetArg(D_CMND_LIGHTCTRL_TIMEOUT, argument, sizeof(argument));
    if (strlen(argument) > 0) LightCtrlSetTimeout (atoi (argument)); 

    // back to configuration screen
    HandleConfiguration ();
    return;
  }

  else
  {
  // collect data
  light_timeout = LightCtrlGetTimeout ();
  
  // beginning of form
  WSContentStart_P (D_LIGHTCTRL_CONFIGURE);
  WSContentSendStyle ();

  // form
  WSContentSend_P (PSTR ("<fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend><form method='get' action='%s'>"), D_LIGHTCTRL_CONFIGURE, D_LIGHTCTRL_PARAMETERS, D_PAGE_LIGHTCTRL);

  // light timeout (mn)
  WSContentSend_P (PSTR ("<p><b>%s</b> <i>(mn)</i><input type='number' name='%s' min='0' max='240' step='1' value='%d'></p>"), D_LIGHTCTRL_TIMEOUT, D_CMND_LIGHTCTRL_TIMEOUT, light_timeout);

  // end of form
  WSContentSend_P(HTTP_FORM_END);
  WSContentSpaceButton(BUTTON_CONFIGURATION);
  WSContentStop();   
  }
}

// append light controler status to main page
bool LightCtrlWebState ()
{
  TIME_T   current_dst;
  uint32_t time_now, time_left;
  uint8_t  light_timeout;
  bool     light_on;
  
  // get timeout
  light_timeout = LightCtrlGetTimeout ();
  
  // dislay current DST time
  time_now = LocalTime();
  BreakTime (time_now, current_dst);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR ("%s<tr><th>%s</th><td style='font-weight:bold;'>%02d:%02d:%02d</td></tr>"), mqtt_data, D_LIGHTCTRL_TIME, current_dst.hour, current_dst.minute, current_dst.second);

  // display light state
  light_on = LightCtrlIsLightOn ();
  if (light_on == true) snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR ("%s<tr><th>%s</th><td style='font-weight:bold; color:green;'>%s</td></tr>"), mqtt_data, D_LIGHTCTRL_LIGHT, D_LIGHTCTRL_ON);
  else snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR ("%s<tr><th>%s</th><td style='font-weight:bold; color:red;'>%s</td></tr>"), mqtt_data, D_LIGHTCTRL_LIGHT, D_LIGHTCTRL_OFF);

  // handle action
  switch (lightctrl_state)
  {
    // if light is ON with a timeout
    case LIGHTCTRL_TIMEOUT:
      // calculate temporisation left
      time_now  = millis ();
      time_left = lightctrl_stop - time_now;

      // convert it to mn and sec
      BreakTime ((uint32_t) time_left/1000, current_dst);

      // display temporisation left
      if (current_dst.minute > 0) snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR ("%s<tr><th>%s</th><td style='font-weight:bold;'><span style='color:green;'>%02d mn %02d</span> / %d mn</td></tr>"), mqtt_data, D_LIGHTCTRL_TEMPO, current_dst.minute, current_dst.second, light_timeout);
      else snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR ("%s<tr><th>%s</th><td style='font-weight:bold;'><span style='color:green;'>%02d sec</span> / %d mn</td></tr>"), mqtt_data, D_LIGHTCTRL_TEMPO, current_dst.second, light_timeout);
      break;
 
      // action is a forced switch
    case LIGHTCTRL_FORCED:
      snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR ("%s<tr><th>%s</th><td style='font-weight:bold; color:blue;'>%s</td></tr>"), mqtt_data, D_LIGHTCTRL_TEMPO, D_LIGHTCTRL_FORCED);
      break;
  }
}

#endif  // USE_WEBSERVER

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv98 (byte callback_id)
{
  bool result = false;

  // main callback switch
  switch (callback_id)
  { 
    case FUNC_INIT:
      LightCtrlInit ();
      break;
    case FUNC_COMMAND:
      result = LightCtrlCommand ();
      break;
    case FUNC_EVERY_250_MSECOND:
      LightCtrlEvery250MSecond ();
      break;
  }
  
  return result;
}

bool Xsns98 (byte callback_id)
{
  bool result = false;

  // main callback switch
  switch (callback_id)
  {
    case FUNC_JSON_APPEND:
      LightCtrlShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_LIGHTCTRL, LightCtrlWebPage);
      break;
    case FUNC_WEB_APPEND:
      LightCtrlWebState ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      LightCtrlWebConfigButton ();
      break;
#endif  // USE_WEBSERVER

  }
  return result;
}

#endif // USE_LIGHTCTRL
