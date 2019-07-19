/*
  xdrv_98_impulse.ino - Push button and motion detector support for impulse switch with Sonoff
  
  Copyright (C) 2019  Nicolas Bernaerts

    08/07/2019 - v1.0 - Creation
    14/07/2019 - v1.1 - Conversion du drv

  Input devices should be configured as followed :
   - Switch1 = Push button
   - Switch2 = Motion detector

  Settings are stored using some unused display parameters :
   - Settings.display_model   = Push button configuration (disabled, enabled)
   - Settings.display_mode    = Motion detector configuration (disabled, enabled or windowed)
   - Settings.display_refresh = Temporisation when impulse comes from push button (mn) 
   - Settings.display_size    = Temporisation when impulse comes from motion detector (sec)

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

#ifdef USE_IMPULSE

/*********************************************************************************************\
 * Universal Impulse switch with push buttons and motion detector as input
\*********************************************************************************************/

#define XDRV_98                          98
#define XSNS_98                          98

#define IMPULSE_BUTTON                   1
#define IMPULSE_MOTION                   2

#define D_PAGE_IMPULSE                   "pulse"
#define D_CMND_IMPULSE_BUTTON            "button"
#define D_CMND_IMPULSE_MOTION            "motion"
#define D_CMND_IMPULSE_WINDOW            "window"
#define D_CMND_IMPULSE_BUTTON_TEMPO      "btempo"
#define D_CMND_IMPULSE_MOTION_TEMPO      "mtempo"
#define D_CMND_IMPULSE_START             "start"
#define D_CMND_IMPULSE_STOP              "stop"

#define D_JSON_IMPULSE                   "Impulse"
#define D_JSON_IMPULSE_MODE              "Mode"
#define D_JSON_IMPULSE_STATE             "State"
#define D_JSON_IMPULSE_RELAY             "Relay"
#define D_JSON_IMPULSE_BUTTON            "Button"
#define D_JSON_IMPULSE_MOTION            "Motion"
#define D_JSON_IMPULSE_PRESSED           "Pressed"
#define D_JSON_IMPULSE_DETECTED          "Detected"
#define D_JSON_IMPULSE_TEMPO             "Tempo"
#define D_JSON_IMPULSE_WINDOW            "Window"

#define D_COLOR_GREEN                    "green"
#define D_COLOR_RED                      "red"

#define IMPULSE_LABEL_BUFFER_SIZE        16
#define IMPULSE_MESSAGE_BUFFER_SIZE      64

// impulse icon coded in base64
const char strIcon0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAAFMAAAAgCAMAAABZ2rRdAAAKu3pUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHjarZhrkuwqroX/M4oegnnDcACJiDuDHn5/wq6s566z+0ZnVqUzbQxCS1pastN//992/+IVfUou5dpKL+XilXrqYfClXffrPvornc/zyvG55j+fd+E5fwVO2aBnYNFn/OB8fr+hpuf8/Hze1fXM056JngtvE0Zb2VZ7xrVnohju8/757fpz30gftvP89/qce659/Z0qzpDMfDG4oNHHi8+zSsSC2OPgWM9nZ9B1zoSYz2f92Xfu9fWL80r42XfXeEbEz65wV3kGlC8+es77/LPvjoc+WuTfvobPF3DyvD6+Pvhub2l76727kQqeKu7Z1PVMcb4xkElSPLcV3pX/zPd63p13Y4sLxAQ0J+/lfPcBb2+fvPjht9dzXH5hYgoaKscQVojnXIs19LAOKMnefocKGOJiA4kFapHT4WWLP+v2s97yjZXFMzJ4JvPc8e3tfjr5/3m/JtrbQtd7c2YJx1fYFSwIMMOQs09GAYjfj0/z8e95uxes7y8DNoJgPm5ubHBc855iZv8eW/HgHBmXr+SecPdVnglwEWtnjPHQg7+Kj9kXf9UQqvf4sYHPwPIQU5gg4HMO4t0GmxgL4LRga3NP9WdsyOE+DbUARI6FtGmWLoCVUiZ+amrE0MgxJ5dzLrnmlnseJZZUcimlFuOo";
const char strIcon1[] PROGMEM = "UWNNNddSa22119FiSy230mprrbfRQ49QWO6lV9db730MFh1MPbh7MGKMGWacaeZZZp1t9jkW4bPSyqusutrqa0iQKKS/FKlOmnQZ6pVQ0qRZi1Zt2nVsYm3HnXbeZdfddt/jhdqD6mfU/BfkfkfNP6gZYumMq++ocbrWtym80Uk2zEAsJA/i1RAgoINhdjW4PhhyhtnVA0mRA6j5bOCIN8RAMKkPefsXdu/I/Yqby+m/wi38CTln0P0vkHMG3YPcd9x+QE3GqSjxAGRZaD694obYGKBthDasJv183OnaDeBI1hBLtvlqG5X7Vg9zbZ3irqjBBuyFWYXBWUPfm3qRBvb8Mvvno/t+QQmZeq/OlC1GFlzJfmubbUxYVu2XDcKAZ5B7Rr3G/JUF2iBVSHlxTHsASHG5+jFSHbBEugrssVojgoKnMojmXveUjg/GUJ1dr7an75ObrjIiJsHHVUSrA7LgZWsq0c9ayyr8KcCW6ncFMttsIhwEz7WU6krTtpTCLkPsWyMN6nabalD3IvqVgN7D15mL8S6BdoZ1ZlplhFlAyXvqzAFHLqycnEJGsMRySSDiq7XdusGZVfJZ0WTFcXAo7YpA/Tv807UNy06bZuWK23I1exvpIWVqxjLPpkrpfsdWSuxLl9+iW8IkDl82uu9GJqGOwNN5xn+09Wz+OM5x7c1vt9e4hNsu3yTlsva7+5jgcaDlyHEhDnwzyllx/WwSw15GFQW5KbCGwCVb";
const char strIcon2[] PROGMEM = "AbRImgo5dPUliTSdbTUVx9675PQXgfh7xLsTzrtkqZNNyDi7jnWFAhusZXwAD1TzOSuKX1U14uhWJMbluQMnEqxup7gJ1LZksvGyo/GM6p6EpIisA+KFp3Hb6uJTNvmZvx3d+eInLqPkT/AfyQLr6hD/HcGETZJNnkBinhFNakqrT+guD0VlxtGpa1i5YDuDJXhLLZznDQA4RXLcpQIHpMbWt+yZtsLvyO2pDR21yCO1nKNkHzg9P6adZ1zjIyrpF8mzOI7F3dSelJM0cdmNX+9zn29kywDPnkqU1u4terOI6GhClJWUOb32dTMSObHK9KDn0to1W2qMilpcHZ2xEjfK8gVUFDLnVRsau2QuLah0sUmLUIsJpp1elxHbB5PgATOl3ku+LdjVCAQ1tjNbBNAziYX0tdqWckBxU8VYxDI8sOPc+tcN+Tf3HOdY5cc9fjQqnoRcE6V8kP27ZXIJsbdz7OTlsIDZFrYorzIvGaKlafBJZu9BZ1xoWNyjM9N9EGATLClH20K1EXVZAlvoq1JG8o8x93bMQnGi4AX2SPRR/mRQIFkgpNKBmIxviHKjayRFbZrYPyagCUZWv8FmUlMDOdC6YE4I1BMogrqdHAE3UtNxWcKPUA1c0gsRICXN0TWICu7zCBBFOOwwrJHwnZ3GdXnorw4YwRHkSfdVxHKPJEjRQ5x1Wimg3g9KtO62PA0icGq2SJZIl+fJ3SW164lYaET7pEChFqAGOrKF";
const char strIcon3[] PROGMEM = "yqBi5UhFmfyhkPoUVNHVW8NOZdO6KVA4e6FiBjUazdtcXEjAPk0v6bpIKZJtIFO0yp897qeoETVabh36zctdVmWRLjr6npGkJYOph8SdUCYiqbvGXjfVfsxq4/qPeQ35/5Da5PXJ1t4Xa154FWnj/SwXpSYao4VxuNJn84+FhFtqgV2o2ScNN4xD9qjnJ863PIjceQi1NmHnso8yzWQ3CcptMDZp6O4d2gaXxSRLs8m4IdW8Ih6tRUavkObKBRNz6kg/4tYMIRVIxdmoneKsAs1hXf82P2br5O9jXU/0H8K4w59FTwKA4Z0Cf5MiaZNqIS9kJSFAGtP5ECRzd3infS40c7gIYUAkRKUUonJNQiwgzCXmvRQVWdKyCSrSrlDCKyVgr0FtGIcyjLp1FlCLtc2wfYH/2sxEI/CgklApzYQAbVTrVypq4hypTPRrDGK1J9nwUeCnKsPhzNmoElqKhRCFAIKGgykQlNPb7LQvdDbynPXQztQVP1udCfK9Sk9JNHVXSaGSMTkfjWFSYYHyJLlCI0kqDV5DqKFU95XJ+6xYU1GqHe2uc+MTiuRFl02rDicP0zV7UFtxpLRrkvCCQo8KadZKiq0laBo56yVbTz+i7N7hfsGeomImwl2y5px8p0UO9RRz8wv24i5EkfaolgLw1hhOThGNuCzOxvJzKjKntzyJCmI6UEDhG1U6AyOZkMmksAKik+7Bz05sQEi0ojV56Gv2ga0DAgKYKDP0";
const char strIcon4[] PROGMEM = "i0y9KH7DHmOgDVQilcvow0g9TegWNXvk9mp1uSOL8i2+ocF11AhRdJdAmOFE9u40dP7WTrnSyiDN5pE0yCSyUi83qGi59PdiY49O/CauyWWF7G59b8rCpuk4gLwMgc+39Y5CIrLPvPeCn5c7i2GfXq/V3ir/s1pjJ8jBdS1xCEoiehCPxArhHqswq4IKvwq0PcwCb52fiVjwOIaP+0Bk01xN+iBHjEXCHD42bC9o2Ro9yJ+S1M5TI9NgqDXawkYW+bUn06QcLDwQmjAloWbP2JAJln56cx4bNyzoAYDldACz3WI4tN+uOwagywOZl8dLPMCd3/3E3Uf7/rxNd/aZ/sEWlor/sJR71qKEIntwmgRrwSLhbylQ0Tmkd5nWPTDbKPSwjbrCkuTGRYHMkjcJ7QLQo5vJDXo0GvGFOyH2v9Hda66CEDUoanOV0rIvcgjFB/oJ8K3ZKIwjSkgJpEcacklGGlu2isUsYUn5KtagXTT3Y6uDLPOs1lqaAuvBN+ChdZyo9xDt0Rg5uGj6yU2qKDVz2GMBeCemPWmrMMqeZDu16PDmipokJtqvQPIXyk5PZCFlL3Lvpqm0vs2ecdjOMPrL0f3pwnNEnJ1+3vLs6eetDwgnXV49/U6X+70z/PMKX4/umwnzFpy3DYOsg1kGNYXPoEg0wmuaYL8fOjwDbGtnjM1zj/p7Gz4f3bsb/vxY4y+eaLC1X/q8r48tvj61+H0ivCNIRPcfmGaIROpz";
const char strIcon5[] PROGMEM = "A6MAAAJbUExURXAQfP////7+/v39/fPz8+Hh4fLy8uLi4vz8/Kurqzc3N/v7+6mpqTg4OPr6+vj4+KioqDY2Nvf39zs7O6ysrDo6OsXFxYmJiYqKioeHh5OTk1xcXB4eHpSUlJ6ensrKyoWFhT09PV5eXltbW09PT1BQUFFRUVdXV2BgYFJSUkxMTA4ODunp6bOzsw8PD+vr61NTU4uLi/b29iEhIebm5pubm0JCQoaGhvHx8R0dHeXl5TExMXl5eQ0NDc3NzaCgoGlpaUZGRoSEhHx8fH5+fnJycq+vrygoKLe3t7W1tQoKCmVlZVpaWuPj4xISEszMzK2trTMzM4CAgCcnJ8LCwvX19e/v7xQUFO3t7SsrKxkZGRcXFxsbGyAgILi4uBAQEAAAAOrq6ujo6La2tsvLy6SkpKqqqmdnZ0hISCYmJrm5ubKystnZ2QwMDC8vL4iIiPn5+fT09Nzc3Hh4eN7e3uTk5Ofn59ra2mJiYiIiIh8fHxYWFgsLC+zs7C0tLaampjU1Nd3d3Tk5OcfHx319ff7///v9//3+//X7//7+//j8//n9/+z3//z+/4jP/yip/+Lz/2/G//T6/9jv/+74/2nD/8Xo/063/8zq/8Hn/5bU/061/6bc/8fp/0Cz/8rp/128/7zl/6vd/3jH/9zx/4LN/5fV/2fC/xWb/zqv/2LB/2K8/wGT/wyd/1u9/1i7/wCS/3PE/6fc/wic/4PN/5LT/0S0";
const char strIcon6[] PROGMEM = "/4nQ/1C5/0a1/1i8/3zK/1a7/4vR//f8/+j2/+n2//L6//r9//D5/+b1/+/5//n8//b7/+34/1gr3N8AAAABdFJOUwBA5thmAAAAAWJLR0QAiAUdSAAAAAlwSFlzAAAOvwAADr8BOAVTJAAAAAd0SU1FB+MHCQcZHBlis8QAAADFSURBVEjHY2DAChISGIgEoypHVQ5ZlQl4AU5tQXjMTRARQwLNzcg8MfsEstyaUMTHgQsw2yWQ5f2EImZGBGCCm8cE4lHDTKYcOVMosGMiZGZCggAxZjLX2zWWQEAYEWYyEGVmqg4HMwQg+R2RAiAsQ0KJCc3MRCbk0MVuJgMVzCQ5zdPMTGgYMnOTYmYQXjP5PFMhoL5Ynzru5GBkEjKCAiEUI8k2M4clFAGMjEKRQTOZZmYgF0NaqKVScMJo6T2qcrioBADiRnaLCHmwugAAAABJRU5ErkJggg==";
const char *const arrIconBase64[] PROGMEM = {strIcon0, strIcon1, strIcon2, strIcon3, strIcon4, strIcon5, strIcon6};

// impulse enumerations
enum ImpulseState { IMPULSE_DISABLE, IMPULSE_ENABLE, IMPULSE_WINDOW, IMPULSE_OFF, IMPULSE_ON };

// impulse commands
enum ImpulseCommands { CMND_IMPULSE_BUTTON, CMND_IMPULSE_MOTION, CMND_IMPULSE_WINDOW, CMND_IMPULSE_BUTTON_TEMPO, CMND_IMPULSE_MOTION_TEMPO, CMND_IMPULSE_START, CMND_IMPULSE_STOP };
const char kImpulseCommands[] PROGMEM = D_CMND_IMPULSE_BUTTON "|" D_CMND_IMPULSE_MOTION "|" D_CMND_IMPULSE_WINDOW "|" D_CMND_IMPULSE_BUTTON_TEMPO "|" D_CMND_IMPULSE_MOTION_TEMPO "|" D_CMND_IMPULSE_START "|" D_CMND_IMPULSE_STOP;

// time slot structure
struct timeslot {
  uint8_t  number;
  uint8_t  start_hour;
  uint8_t  start_minute;
  uint8_t  stop_hour;
  uint8_t  stop_minute;
};

// variables
bool  impulse_button_pressed  = false;               // current state of push button
bool  impulse_motion_detected = false;               // current state of motion detector
ulong impulse_tempo_start     = 0;                   // timestamp when tempo started
ulong impulse_tempo_delay     = 0;                   // tempo delay (in ms)

/*********************************************************************************************/

// save motion time slot (format is HH:MM-HH:MM)
void ImpulseMotionSetWindow (char* strWindow)
{
  uint8_t index = 0;
  char* token;
  char* arr_token[4];
  uint32_t time_setting;
  timeslot time_slot;

  AddLog_P(LOG_LEVEL_INFO, "ImpulseMotionSetWindow : ", strWindow);

  // split string into array of values
  token = strtok (strWindow, ":");
  if (token != NULL)
  {
    arr_token[index++] = token;
    token = strtok (NULL, "-");
  }
  if (token != NULL)
  {
    arr_token[index++] = token;
    token = strtok (NULL, ":");
  }
  if (token != NULL) 
  {
    arr_token[index++] = token;
    token = strtok (NULL, ":");
  }
  if (token != NULL) arr_token[index++] = token;

  // convert strings to time slot
  time_slot.start_hour   = atoi (arr_token[0]);
  time_slot.start_minute = atoi (arr_token[1]);
  time_slot.stop_hour    = atoi (arr_token[2]);
  time_slot.stop_minute  = atoi (arr_token[3]);
  
  // calculate value to save according to time slot
  time_setting = time_slot.start_hour * 1000000;
  time_setting += time_slot.start_minute * 10000;
  time_setting += time_slot.stop_hour * 100;
  time_setting += time_slot.stop_minute;

  // write time slot in timer setting
  Settings.timer[0].data = time_setting;
}

// get motion time slot data
struct timeslot ImpulseMotionGetWindow ()
{
  uint32_t slot_value;
  div_t    div_result;
  timeslot slot_result;

  // read time slot raw data
  slot_value = Settings.timer[0].data;

  // read start hour
  div_result = div (slot_value, 1000000);
  slot_result.start_hour = div_result.quot;

  // read start minute
  div_result = div (slot_value, 1000000);
  div_result = div (div_result.rem, 10000);
  slot_result.start_minute = div_result.quot;

  // read stop hour
  div_result = div (slot_value, 10000);
  div_result = div (div_result.rem, 100);
  slot_result.stop_hour = div_result.quot;

  // read start minute
  div_result = div (slot_value, 100);
  slot_result.stop_minute = div_result.rem;

  AddLog_P(LOG_LEVEL_INFO, "ImpulseMotionGetWindow : ");

  return slot_result;
}

// set push button enabled status
void ImpulseButtonSetMode (uint8_t button_mode)
{
  if (button_mode > IMPULSE_ENABLE) button_mode = IMPULSE_ENABLE;
  Settings.display_model = button_mode;
}

// get push button mode
uint8_t ImpulseButtonGetMode ()
{
  return Settings.display_model;
}

// get push button enable state
bool ImpulseButtonIsEnabled ()
{
  bool    result;
  uint8_t button_mode;

  // get push button mode
  button_mode = ImpulseButtonGetMode ();
  result = (button_mode == IMPULSE_ENABLE);

  return result;
}

// get push button pressed state
bool ImpulseButtonIsPressed ()
{
  bool    result;
  uint8_t button_status;
  
  // get push button enabled state
  result = ImpulseButtonIsEnabled ();
  if (result == true)
  {
    // check if push button pressed
    button_status = SwitchGetVirtual (IMPULSE_BUTTON);
    if (button_status != PRESSED) result = false;
  }
  
  return result;
}

// set push button tempo (mn)
void ImpulseButtonSetTempo (uint8_t tempo)
{
  Settings.display_refresh = tempo;
}

// get push button tempo (mn)
uint32_t ImpulseButtonGetTempo (bool inMs)
{
  uint32_t tempo;

  // read tempo (in mn)
  tempo = (uint32_t) Settings.display_refresh;

  // if needed, convert to ms
  if (inMs == true) tempo = tempo * 60000;

  return tempo;
}

// set motion detection enabled status
void ImpulseMotionSetMode (uint8_t motion_mode)
{
  if (motion_mode > IMPULSE_WINDOW) motion_mode = IMPULSE_WINDOW;
  Settings.display_mode = motion_mode;
}

// get motion detection enabled status
uint8_t ImpulseMotionGetMode ()
{
  return Settings.display_mode;
}

// get motion time enable state (according to window activity)
bool ImpulseMotionIsEnabled ()
{
  bool     result = false;
  timeslot slot_window;
  uint8_t  motion_mode;
  uint32_t current_time, current_minute, start_minute, stop_minute;
  TIME_T   current_dst;

  // get motion detector mode
  motion_mode = ImpulseMotionGetMode ();

  // if motion detection is enabled for a specific window
  result = (motion_mode == IMPULSE_ENABLE);
  if (motion_mode == IMPULSE_WINDOW)
  {
    // get current DST time
    current_time = LocalTime();
    BreakTime (current_time, current_dst);
    current_minute = (current_dst.hour * 60) + current_dst.minute;

    // get current window start and stop
    slot_window  = ImpulseMotionGetWindow ();
    start_minute = (slot_window.start_hour * 60) + slot_window.start_minute;
    stop_minute = (slot_window.start_hour * 60) + slot_window.start_minute;

    // calculate if in the active window
    result = ((current_minute >= start_minute) && (current_minute <= stop_minute));
  }

  return result;
}

// get motion detected state
bool ImpulseMotionIsDetected ()
{
  bool    result;
  uint8_t motion_status;
  
  // get motion detector enabled state
  result = ImpulseMotionIsEnabled ();
  if (result == true)
  {
    // check if motion is detected
    motion_status = SwitchGetVirtual (IMPULSE_MOTION);
    if (motion_status != PRESSED) result = false;
  }
  
  return result;
}

// set impulse switch standard duration
void ImpulseMotionSetTempo (uint8_t tempo)
{
  Settings.display_size = tempo;
}

// get motion detector standard tempo (sec)
uint32_t ImpulseMotionGetTempo (bool inMs)
{
  uint32_t tempo;

  // read tempo (in mn)
  tempo = (uint32_t) Settings.display_size;

  // if needed, convert to ms
  if (inMs == true) tempo = tempo * 1000;

  return tempo;
}

// Show JSON status (for MQTT)
void ImpulseShowJSON (bool append)
{
  uint8_t  relay, bmode, mmode, btempo, mtempo;
  bool     bpressed, mdetected;
  timeslot mwindow;

  // read relay state
  relay = bitRead (power, 0);

  // collect data
  bmode     = ImpulseButtonGetMode ();
  btempo    = ImpulseButtonGetTempo (false);
  bpressed  = ImpulseButtonIsPressed ();
  mmode     = ImpulseMotionGetMode ();
  mtempo    = ImpulseMotionGetTempo (false);
  mdetected = ImpulseMotionIsDetected ();
  mwindow   = ImpulseMotionGetWindow ();
  
  // start message  -->  {  or  ,
  if (append == false) snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("{"));
  else snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,"), mqtt_data);

  // "Impulse":{"Relay":1,
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_IMPULSE "\":{"), mqtt_data);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_IMPULSE_RELAY "\":%d,"), mqtt_data, relay);

  // "Button":{"Mode":1,"Tempo":10,"Pressed":1},
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_IMPULSE_BUTTON "\":{"), mqtt_data);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_IMPULSE_MODE "\":%d,"), mqtt_data, bmode);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_IMPULSE_TEMPO "\":%d,"), mqtt_data, btempo);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_IMPULSE_PRESSED "\":%d},"), mqtt_data, bpressed);

  // "Motion":{"Mode":3,"Tempo":30,"Detected":1,"Window":"01:00-12:00"}}
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_IMPULSE_MOTION "\":{"), mqtt_data);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_IMPULSE_MODE "\":%d,"), mqtt_data, mmode);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_IMPULSE_TEMPO "\":%d,"), mqtt_data, mtempo);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_IMPULSE_DETECTED "\":%d,"), mqtt_data, mdetected);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_IMPULSE_WINDOW "\":\"%2d:%2d-%2d:%2d\"}}"), mqtt_data, mwindow.start_hour, mwindow.start_minute, mwindow.stop_hour, mwindow.stop_minute);

  // if not in append mode, publish message 
  if (append == false)
  { 
    // end of message   ->  }
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);

    // publish full state
    MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
  }
}

// pre init main status
void ImpulsePreInit ()
{
  // set switch mode
  Settings.switchmode[IMPULSE_BUTTON] = FOLLOW;
  Settings.switchmode[IMPULSE_MOTION] = FOLLOW;

  // disable serial log
  Settings.seriallog_level = 0;
}

// Handle impulse switch MQTT commands
bool ImpulseCommand ()
{
  bool serviced = true;
  timeslot time_slot;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kImpulseCommands);

  // handle command
  switch (command_code)
  {
    case CMND_IMPULSE_BUTTON:       // set push button operation mode
      ImpulseButtonSetMode (XdrvMailbox.payload);
      break;
    case CMND_IMPULSE_MOTION:     // set motion detector operation mode
      ImpulseMotionSetMode (XdrvMailbox.payload);
      break;
    case CMND_IMPULSE_MOTION_TEMPO:  // set motion detector tempo
      ImpulseMotionSetTempo (XdrvMailbox.payload);
      break;
    case CMND_IMPULSE_BUTTON_TEMPO:  // set push button tempo
      ImpulseButtonSetTempo (XdrvMailbox.payload);
      break;
    case CMND_IMPULSE_WINDOW:     // set motion detector active window
      ImpulseMotionSetWindow (XdrvMailbox.data);
      break;
    default:
      serviced = false;
  }

  // send MQTT status
  if (serviced == true) ImpulseShowJSON (false);
  
  return serviced;
}

// update impulse switch relay states according to button and motion detector
void ImpulseEvery250MSecond ()
{
  bool  button_pressed, motion_detected, relay_on;
  ulong tempo_now, tempo_left, tempo_delay;

  // update temporisation
  tempo_now  = millis ();
  tempo_left = impulse_tempo_start + impulse_tempo_delay - tempo_now;

  // read relay, button and motion detector status
  relay_on        = (bool) (bitRead (power, 0) == 1);
  button_pressed  = ImpulseButtonIsPressed ();
  motion_detected = ImpulseMotionIsDetected ();
  
  // if relay is off and button has been triggered, switch relay on with timeout
  if ((relay_on == false) && (button_pressed == true) && (impulse_button_pressed == false))
  {
    // update status
    impulse_button_pressed = true;
    impulse_tempo_start    = tempo_now;
    impulse_tempo_delay    = ImpulseButtonGetTempo (true);
    
    // switch on relay
    ExecuteCommandPower (1, POWER_ON, SRC_IGNORE);
    AddLog_P(LOG_LEVEL_INFO, "relay is off and button pressed, switch relay on");
  }

  // else, if relay is off and motion has been triggered, switch relay on with tempo
  else if ((relay_on == false) && (motion_detected == true) && (impulse_motion_detected == false))
  {
    // motion detector state should change, with tempo reset
    impulse_motion_detected = true;
    impulse_tempo_start     = tempo_now;
    impulse_tempo_delay     = ImpulseMotionGetTempo (true);  
    
    // switch on relay
    ExecuteCommandPower (1, POWER_ON, SRC_IGNORE);
    AddLog_P(LOG_LEVEL_INFO, "relay is off and motion detected, switch relay on");
  }

  // else if relay on and button pressed, switch relay off
  else if ((relay_on == true) && (button_pressed == true) && (impulse_button_pressed == false))
  {
    // relay state should change, with no tempo
    impulse_button_pressed  = true;
    impulse_tempo_start     = 0;
    impulse_tempo_delay     = 0;        
    
    // switch off relay
    ExecuteCommandPower (1, POWER_OFF, SRC_IGNORE);
    AddLog_P(LOG_LEVEL_INFO, "relay on and button pressed, switch relay off");
  }

  // else if relay on and motion is triggered, update delay
  else if ((relay_on == true) && (motion_detected == true))
  {
    // motion detected
    impulse_motion_detected = true;
    
    // if tempo left is smaller that motion tempo, update
    tempo_delay = ImpulseMotionGetTempo (true);
    if (tempo_left < tempo_delay)
    {
      impulse_tempo_start = tempo_now;        
      impulse_tempo_delay = tempo_delay;
    }

    AddLog_P(LOG_LEVEL_INFO, "relay on and motion tempo updated");
  }

  // else, if relay on and tempo target has been reached, switch relay off
  else if ((relay_on == true) && (impulse_tempo_start + impulse_tempo_delay < tempo_now))
  {
    // reset tempo
    impulse_tempo_start = 0;
    impulse_tempo_delay = 0;        
    
    // switch off relay
    ExecuteCommandPower (1, POWER_OFF, SRC_IGNORE);
    AddLog_P(LOG_LEVEL_INFO, "relay on and tempo over, switch relay off");
  }

  // if button not pressed, update it
  if (button_pressed == false) impulse_button_pressed = false;
  
  // if motion not detected, update it
  if (motion_detected == false) impulse_motion_detected = false;
}

#ifdef USE_WEBSERVER

// Pilot Wire icon
void ImpulseWebDisplayIcon (uint8_t height)
{
  uint8_t nbrItem, index;

  // display img according to height
  WSContentSend_P ("<img height=%d src='data:image/png;base64,", height);

  // loop to display base64 segments
  nbrItem = sizeof (arrIconBase64) / sizeof (char*);
  for (index=0; index<nbrItem; index++) { WSContentSend_P (arrIconBase64[index]); }

  WSContentSend_P ("'/>");
}

// impulse switch configuration button
void ImpulseWebConfigButton ()
{
  // beginning
  WSContentSend_P (PSTR ("<table style='width:100%%;'><tr>"));

  // impulse switch icon
  WSContentSend_P (PSTR ("<td align='center'>"));
  ImpulseWebDisplayIcon (32);
  WSContentSend_P (PSTR ("</td>"));

  // button
  WSContentSend_P (PSTR ("<td><form action='%s' method='get'><button>%s</button></form></td>"), D_PAGE_IMPULSE, D_IMPULSE_CONFIGURE);

  // end
  WSContentSend_P (PSTR ("</tr></table>"));
}

// impulse switch web configuration page
void ImpulseWebPage ()
{
  bool     updated = false;
  bool     bpressed, mdetected;
  uint8_t  bmode, mmode, btempo, mtempo;
  timeslot mwindow;
  char     argument[IMPULSE_LABEL_BUFFER_SIZE];
  char     window[IMPULSE_LABEL_BUFFER_SIZE];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // get push button mode according to 'button' parameter
  if (WebServer->hasArg(D_CMND_IMPULSE_BUTTON))
  {
    WebGetArg (D_CMND_IMPULSE_BUTTON, argument, IMPULSE_LABEL_BUFFER_SIZE);
    ImpulseButtonSetMode (atoi (argument)); 
    updated = true;
  }

  // get motion detector mode according to 'motion' parameter
  if (WebServer->hasArg(D_CMND_IMPULSE_MOTION))
  {
    WebGetArg (D_CMND_IMPULSE_MOTION, argument, IMPULSE_LABEL_BUFFER_SIZE);
    ImpulseMotionSetMode (atoi (argument)); 
    updated = true;
  }

  // get motion detector tempo according to 'mtempo' parameter
  if (WebServer->hasArg(D_CMND_IMPULSE_MOTION_TEMPO))
  {
    WebGetArg (D_CMND_IMPULSE_MOTION_TEMPO, argument, IMPULSE_LABEL_BUFFER_SIZE);
    ImpulseMotionSetTempo ((uint8_t) atoi (argument)); 
    updated = true;
  }

  // get push button tempo according to 'btempo' parameter
  if (WebServer->hasArg(D_CMND_IMPULSE_BUTTON_TEMPO))
  {
    WebGetArg (D_CMND_IMPULSE_BUTTON_TEMPO, argument, IMPULSE_LABEL_BUFFER_SIZE);
    ImpulseButtonSetTempo ((uint8_t) atoi (argument)); 
    updated = true;
  }

  // get motion detector active window according to 'window' parameters
  if (WebServer->hasArg(D_CMND_IMPULSE_START))
  {
    WebGetArg (D_CMND_IMPULSE_START, argument, IMPULSE_LABEL_BUFFER_SIZE);
    strcpy (window, argument);
    strcat (window, "-");
    WebGetArg (D_CMND_IMPULSE_STOP, argument, IMPULSE_LABEL_BUFFER_SIZE);
    strcat (window, argument);
   
    ImpulseMotionSetWindow (window); 
    updated = true;
  }

  // if parameters updated, back to main page
  if (updated == true)
  {
    WebServer->sendHeader ("Location", "/", true);
    WebServer->send ( 302, "text/plain", "");
  }
  
  // collect data
  bmode     = ImpulseButtonGetMode ();
  btempo    = ImpulseButtonGetTempo (false);
  bpressed  = ImpulseButtonIsPressed ();
  mmode     = ImpulseMotionGetMode ();
  mtempo    = ImpulseMotionGetTempo (false);
  mdetected = ImpulseMotionIsDetected ();
  mwindow   = ImpulseMotionGetWindow ();
  
  // beginning of form
  WSContentStart_P (D_IMPULSE_CONFIGURE);
  WSContentSendStyle ();

  // form
  WSContentSend_P (PSTR ("<fieldset><legend><b>&nbsp;%s&nbsp;</b></legend><form method='get' action='%s'>"), D_IMPULSE_BUTTON, D_PAGE_IMPULSE);

  // push button mode
  WSContentSend_P (PSTR ("<p><b>%s</b></p>"), D_IMPULSE_BUTTON);
  
  if (bmode == IMPULSE_DISABLE) { strcpy (argument, "checked"); } else { strcpy (argument, ""); }
  WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>"), D_CMND_IMPULSE_BUTTON, IMPULSE_DISABLE, argument, D_IMPULSE_DISABLE);
  
  if (bmode == IMPULSE_ENABLE) { strcpy (argument, "checked"); } else { strcpy (argument, ""); }
  WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>"), D_CMND_IMPULSE_BUTTON, IMPULSE_ENABLE, argument, D_IMPULSE_ENABLE);

  // push button tempo
  WSContentSend_P (PSTR ("<p>%s <i>(mn)</i></p>"), D_IMPULSE_BUTTON_TEMPO);
  WSContentSend_P (PSTR ("<p><input type='number' name='%s' min='0' max='255' step='1' value='%d'></p>"), D_CMND_IMPULSE_BUTTON_TEMPO, btempo);

  // motion detector input
  WSContentSend_P (PSTR ("<p><b>%s</b></p>"), D_IMPULSE_MOTION);
  
  if (mmode == IMPULSE_DISABLE) { strcpy (argument, "checked"); } else { strcpy (argument, ""); }
  WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>"), D_CMND_IMPULSE_MOTION, IMPULSE_DISABLE, argument, D_IMPULSE_DISABLE);
  
  if (mmode == IMPULSE_ENABLE) { strcpy (argument, "checked"); } else { strcpy (argument, ""); }
  WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>"), D_CMND_IMPULSE_MOTION, IMPULSE_ENABLE, argument, D_IMPULSE_ENABLE);

  if (mmode == IMPULSE_WINDOW) { strcpy (argument, "checked"); } else { strcpy (argument, ""); }
  WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s "), D_CMND_IMPULSE_MOTION, IMPULSE_WINDOW, argument, D_IMPULSE_FROM);
  WSContentSend_P (PSTR ("<input type='time' name='%s' style='width:120px;' min='00:00' max='23:59' value='%02u:%02u' required>"), D_CMND_IMPULSE_START, mwindow.start_hour, mwindow.start_minute);
  WSContentSend_P (PSTR (" %s "), D_IMPULSE_TO);
  WSContentSend_P (PSTR ("<input type='time' name='%s' style='width:120px;' min='00:00' max='23:59' value='%02u:%02u' required>"), D_CMND_IMPULSE_STOP, mwindow.stop_hour, mwindow.stop_minute);
  WSContentSend_P (PSTR ("</p>"));

  // motion detector tempo
  WSContentSend_P (PSTR ("<p>%s <i>(sec)</i></p>"), D_IMPULSE_MOTION_TEMPO);
  WSContentSend_P (PSTR ("<p><input type='number' name='%s' min='0' max='255' step='1' value='%d'></p>"),  D_CMND_IMPULSE_MOTION_TEMPO, mtempo);

  // end of form
  WSContentSend_P(HTTP_FORM_END);
  WSContentSpaceButton(BUTTON_CONFIGURATION);
  WSContentStop();
}

// append pilot wire state to main page
bool ImpulseWebState ()
{
  time_t timestamp = time( NULL );
  TIME_T current_dst;
  char   state[IMPULSE_LABEL_BUFFER_SIZE];
  char   color[IMPULSE_LABEL_BUFFER_SIZE];
  bool   button, motion;
  uint32_t current_time;
  ulong    tempo_now, tempo_left;
  
  // get push button and motion detector state
  button = ImpulseButtonIsPressed ();
  motion = ImpulseMotionIsDetected ();

  // dislay current DST time
  current_time = LocalTime();
  BreakTime (current_time, current_dst);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR ("%s<tr><th>%s</th><td style='font-weight:bold;'>%02d:%02d:%02d</td></tr>"), mqtt_data, D_IMPULSE_TIME, current_dst.hour, current_dst.minute, current_dst.second);

  // calculate temporisation left
  tempo_now  = millis ();
  tempo_left = impulse_tempo_start + impulse_tempo_delay - tempo_now;

  // set push button display
  if (button == true) { strcpy (state, D_IMPULSE_ON); strcpy (color, D_COLOR_GREEN); }
  else { strcpy (state, D_IMPULSE_OFF); strcpy (color, D_COLOR_RED); }
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR ("%s<tr><th>%s</th><td style='font-weight:bold; color:%s;'>%s</td></tr>"), mqtt_data, D_IMPULSE_BUTTON, color, state);

  // set motion detector display
  if (motion == true) { strcpy (state, D_IMPULSE_ON); strcpy (color, D_COLOR_GREEN); }
  else { strcpy (state, D_IMPULSE_OFF); strcpy (color, D_COLOR_RED); }
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR ("%s<tr><th>%s</th><td style='font-weight:bold; color:%s;'>%s</td></tr>"), mqtt_data, D_IMPULSE_MOTION, color, state);

  // if needed, display relay on time left
  if (tempo_left < impulse_tempo_delay)
  {
    BreakTime ((uint32_t) tempo_left/1000, current_dst);
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR ("%s<tr><th>%s</th><td style='font-weight:bold; color:green;'>%02d:%02d</td></tr>"), mqtt_data, D_IMPULSE_TEMPO, current_dst.minute, current_dst.second);
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
    case FUNC_PRE_INIT:
      ImpulsePreInit ();
      break;
    case FUNC_COMMAND:
      result = ImpulseCommand ();
      break;
    case FUNC_EVERY_250_MSECOND:
      ImpulseEvery250MSecond ();
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
      ImpulseShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_IMPULSE, ImpulseWebPage);
      break;
    case FUNC_WEB_APPEND:
      ImpulseWebState ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      ImpulseWebConfigButton ();
      break;
#endif  // USE_WEBSERVER

  }
  return result;
}

#endif // USE_IMPULSE
