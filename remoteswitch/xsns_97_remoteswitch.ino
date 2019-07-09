/*
  xsns_97_remoteswitch.ino - Remote switch support for Sonoff
  Allow operation thru push buttons and motion detectors
  
  Copyright (C) 2019  Nicolas Bernaerts

    08/07/2019 - v1.0 - Creation

  Settings are stored using some unused display parameters :
   - Settings.display_model     = Push button enabled
   - Settings.display_mode      = Motion detector enabled
   - Settings.display_refresh   = Debounce duration (sec)
   - Settings.display_size      = Switch duration (sec)
    
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

#ifdef USE_REMOTESWITCH

/*********************************************************************************************\
 * Universal Remote switch
\*********************************************************************************************/

#define XSNS_97                               97

#define D_PAGE_REMOTESWITCH                   "switch"
#define D_CMND_REMOTESWITCH_BUTTON            "button"
#define D_CMND_REMOTESWITCH_MOTION            "motion"
#define D_CMND_REMOTESWITCH_DEBOUNCE          "debounce"
#define D_CMND_REMOTESWITCH_DURATION          "duration"
#define D_CMND_REMOTESWITCH_SLOT0             "s0"
#define D_CMND_REMOTESWITCH_SLOT0_START_HOUR  "s0srthr"
#define D_CMND_REMOTESWITCH_SLOT0_START_MIN   "s0srtmn"
#define D_CMND_REMOTESWITCH_SLOT0_STOP_HOUR   "s0stphr"
#define D_CMND_REMOTESWITCH_SLOT0_STOP_MIN    "s0stpmn"
#define D_CMND_REMOTESWITCH_SLOT1             "s1"
#define D_CMND_REMOTESWITCH_SLOT1_START_HOUR  "s1srthr"
#define D_CMND_REMOTESWITCH_SLOT1_START_MIN   "s1srtmn"
#define D_CMND_REMOTESWITCH_SLOT1_STOP_HOUR   "s1stphr"
#define D_CMND_REMOTESWITCH_SLOT1_STOP_MIN    "s1stpmn"

#define D_JSON_REMOTESWITCH                   "RemoteSwitch"
#define D_JSON_REMOTESWITCH_ENABLED           "Enabled"
#define D_JSON_REMOTESWITCH_STATE             "State"
#define D_JSON_REMOTESWITCH_RELAY             "Relay"
#define D_JSON_REMOTESWITCH_BUTTON            "PushButton"
#define D_JSON_REMOTESWITCH_MOTION            "MotionDetector"
#define D_JSON_REMOTESWITCH_DEBOUNCE          "Debounce"
#define D_JSON_REMOTESWITCH_DURATION          "Duration"
#define D_JSON_REMOTESWITCH_SLOT              "Slot"


#define REMOTESWITCH_LABEL_BUFFER_SIZE        16
#define REMOTESWITCH_MESSAGE_BUFFER_SIZE      64

#define REMOTESWITCH_DEFAULT_DURATION         30         // 30 seconds
#define REMOTESWITCH_DEFAULT_DEBOUNCE         2          // 2 seconds

// remote switch icon coded in base64
const char strIcon0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAAFMAAAAgCAMAAABZ2rRdAAAKu3pUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHjarZhrkuwqroX/M4oegnnDcACJiDuDHn5/wq6s566z+0ZnVqUzbQxCS1pastN//992/+IVfUou5dpKL+XilXrqYfClXffrPvornc/zyvG55j+fd+E5fwVO2aBnYNFn/OB8fr+hpuf8/Hze1fXM056JngtvE0Zb2VZ7xrVnohju8/757fpz30gftvP89/qce659/Z0qzpDMfDG4oNHHi8+zSsSC2OPgWM9nZ9B1zoSYz2f92Xfu9fWL80r42XfXeEbEz65wV3kGlC8+es77/LPvjoc+WuTfvobPF3DyvD6+Pvhub2l76727kQqeKu7Z1PVMcb4xkElSPLcV3pX/zPd63p13Y4sLxAQ0J+/lfPcBb2+fvPjht9dzXH5hYgoaKscQVojnXIs19LAOKMnefocKGOJiA4kFapHT4WWLP+v2s97yjZXFMzJ4JvPc8e3tfjr5/3m/JtrbQtd7c2YJx1fYFSwIMMOQs09GAYjfj0/z8e95uxes7y8DNoJgPm5ubHBc855iZv8eW/HgHBmXr+SecPdVnglwEWtnjPHQg7+Kj9kXf9UQqvf4sYHPwPIQU5gg4HMO4t0GmxgL4LRga3NP9WdsyOE+DbUARI6FtGmWLoCVUiZ+amrE0MgxJ5dzLrnmlnseJZZUcimlFuOo";
const char strIcon1[] PROGMEM = "UWNNNddSa22119FiSy230mprrbfRQ49QWO6lV9db730MFh1MPbh7MGKMGWacaeZZZp1t9jkW4bPSyqusutrqa0iQKKS/FKlOmnQZ6pVQ0qRZi1Zt2nVsYm3HnXbeZdfddt/jhdqD6mfU/BfkfkfNP6gZYumMq++ocbrWtym80Uk2zEAsJA/i1RAgoINhdjW4PhhyhtnVA0mRA6j5bOCIN8RAMKkPefsXdu/I/Yqby+m/wi38CTln0P0vkHMG3YPcd9x+QE3GqSjxAGRZaD694obYGKBthDasJv183OnaDeBI1hBLtvlqG5X7Vg9zbZ3irqjBBuyFWYXBWUPfm3qRBvb8Mvvno/t+QQmZeq/OlC1GFlzJfmubbUxYVu2XDcKAZ5B7Rr3G/JUF2iBVSHlxTHsASHG5+jFSHbBEugrssVojgoKnMojmXveUjg/GUJ1dr7an75ObrjIiJsHHVUSrA7LgZWsq0c9ayyr8KcCW6ncFMttsIhwEz7WU6krTtpTCLkPsWyMN6nabalD3IvqVgN7D15mL8S6BdoZ1ZlplhFlAyXvqzAFHLqycnEJGsMRySSDiq7XdusGZVfJZ0WTFcXAo7YpA/Tv807UNy06bZuWK23I1exvpIWVqxjLPpkrpfsdWSuxLl9+iW8IkDl82uu9GJqGOwNN5xn+09Wz+OM5x7c1vt9e4hNsu3yTlsva7+5jgcaDlyHEhDnwzyllx/WwSw15GFQW5KbCGwCVb";
const char strIcon2[] PROGMEM = "AbRImgo5dPUliTSdbTUVx9675PQXgfh7xLsTzrtkqZNNyDi7jnWFAhusZXwAD1TzOSuKX1U14uhWJMbluQMnEqxup7gJ1LZksvGyo/GM6p6EpIisA+KFp3Hb6uJTNvmZvx3d+eInLqPkT/AfyQLr6hD/HcGETZJNnkBinhFNakqrT+guD0VlxtGpa1i5YDuDJXhLLZznDQA4RXLcpQIHpMbWt+yZtsLvyO2pDR21yCO1nKNkHzg9P6adZ1zjIyrpF8mzOI7F3dSelJM0cdmNX+9zn29kywDPnkqU1u4terOI6GhClJWUOb32dTMSObHK9KDn0to1W2qMilpcHZ2xEjfK8gVUFDLnVRsau2QuLah0sUmLUIsJpp1elxHbB5PgATOl3ku+LdjVCAQ1tjNbBNAziYX0tdqWckBxU8VYxDI8sOPc+tcN+Tf3HOdY5cc9fjQqnoRcE6V8kP27ZXIJsbdz7OTlsIDZFrYorzIvGaKlafBJZu9BZ1xoWNyjM9N9EGATLClH20K1EXVZAlvoq1JG8o8x93bMQnGi4AX2SPRR/mRQIFkgpNKBmIxviHKjayRFbZrYPyagCUZWv8FmUlMDOdC6YE4I1BMogrqdHAE3UtNxWcKPUA1c0gsRICXN0TWICu7zCBBFOOwwrJHwnZ3GdXnorw4YwRHkSfdVxHKPJEjRQ5x1Wimg3g9KtO62PA0icGq2SJZIl+fJ3SW164lYaET7pEChFqAGOrKF";
const char strIcon3[] PROGMEM = "yqBi5UhFmfyhkPoUVNHVW8NOZdO6KVA4e6FiBjUazdtcXEjAPk0v6bpIKZJtIFO0yp897qeoETVabh36zctdVmWRLjr6npGkJYOph8SdUCYiqbvGXjfVfsxq4/qPeQ35/5Da5PXJ1t4Xa154FWnj/SwXpSYao4VxuNJn84+FhFtqgV2o2ScNN4xD9qjnJ863PIjceQi1NmHnso8yzWQ3CcptMDZp6O4d2gaXxSRLs8m4IdW8Ih6tRUavkObKBRNz6kg/4tYMIRVIxdmoneKsAs1hXf82P2br5O9jXU/0H8K4w59FTwKA4Z0Cf5MiaZNqIS9kJSFAGtP5ECRzd3infS40c7gIYUAkRKUUonJNQiwgzCXmvRQVWdKyCSrSrlDCKyVgr0FtGIcyjLp1FlCLtc2wfYH/2sxEI/CgklApzYQAbVTrVypq4hypTPRrDGK1J9nwUeCnKsPhzNmoElqKhRCFAIKGgykQlNPb7LQvdDbynPXQztQVP1udCfK9Sk9JNHVXSaGSMTkfjWFSYYHyJLlCI0kqDV5DqKFU95XJ+6xYU1GqHe2uc+MTiuRFl02rDicP0zV7UFtxpLRrkvCCQo8KadZKiq0laBo56yVbTz+i7N7hfsGeomImwl2y5px8p0UO9RRz8wv24i5EkfaolgLw1hhOThGNuCzOxvJzKjKntzyJCmI6UEDhG1U6AyOZkMmksAKik+7Bz05sQEi0ojV56Gv2ga0DAgKYKDP0";
const char strIcon4[] PROGMEM = "i0y9KH7DHmOgDVQilcvow0g9TegWNXvk9mp1uSOL8i2+ocF11AhRdJdAmOFE9u40dP7WTrnSyiDN5pE0yCSyUi83qGi59PdiY49O/CauyWWF7G59b8rCpuk4gLwMgc+39Y5CIrLPvPeCn5c7i2GfXq/V3ir/s1pjJ8jBdS1xCEoiehCPxArhHqswq4IKvwq0PcwCb52fiVjwOIaP+0Bk01xN+iBHjEXCHD42bC9o2Ro9yJ+S1M5TI9NgqDXawkYW+bUn06QcLDwQmjAloWbP2JAJln56cx4bNyzoAYDldACz3WI4tN+uOwagywOZl8dLPMCd3/3E3Uf7/rxNd/aZ/sEWlor/sJR71qKEIntwmgRrwSLhbylQ0Tmkd5nWPTDbKPSwjbrCkuTGRYHMkjcJ7QLQo5vJDXo0GvGFOyH2v9Hda66CEDUoanOV0rIvcgjFB/oJ8K3ZKIwjSkgJpEcacklGGlu2isUsYUn5KtagXTT3Y6uDLPOs1lqaAuvBN+ChdZyo9xDt0Rg5uGj6yU2qKDVz2GMBeCemPWmrMMqeZDu16PDmipokJtqvQPIXyk5PZCFlL3Lvpqm0vs2ecdjOMPrL0f3pwnNEnJ1+3vLs6eetDwgnXV49/U6X+70z/PMKX4/umwnzFpy3DYOsg1kGNYXPoEg0wmuaYL8fOjwDbGtnjM1zj/p7Gz4f3bsb/vxY4y+eaLC1X/q8r48tvj61+H0ivCNIRPcfmGaIROpz";
const char strIcon5[] PROGMEM = "A6MAAAJbUExURXAQfP////7+/v39/fPz8+Hh4fLy8uLi4vz8/Kurqzc3N/v7+6mpqTg4OPr6+vj4+KioqDY2Nvf39zs7O6ysrDo6OsXFxYmJiYqKioeHh5OTk1xcXB4eHpSUlJ6ensrKyoWFhT09PV5eXltbW09PT1BQUFFRUVdXV2BgYFJSUkxMTA4ODunp6bOzsw8PD+vr61NTU4uLi/b29iEhIebm5pubm0JCQoaGhvHx8R0dHeXl5TExMXl5eQ0NDc3NzaCgoGlpaUZGRoSEhHx8fH5+fnJycq+vrygoKLe3t7W1tQoKCmVlZVpaWuPj4xISEszMzK2trTMzM4CAgCcnJ8LCwvX19e/v7xQUFO3t7SsrKxkZGRcXFxsbGyAgILi4uBAQEAAAAOrq6ujo6La2tsvLy6SkpKqqqmdnZ0hISCYmJrm5ubKystnZ2QwMDC8vL4iIiPn5+fT09Nzc3Hh4eN7e3uTk5Ofn59ra2mJiYiIiIh8fHxYWFgsLC+zs7C0tLaampjU1Nd3d3Tk5OcfHx319ff7///v9//3+//X7//7+//j8//n9/+z3//z+/4jP/yip/+Lz/2/G//T6/9jv/+74/2nD/8Xo/063/8zq/8Hn/5bU/061/6bc/8fp/0Cz/8rp/128/7zl/6vd/3jH/9zx/4LN/5fV/2fC/xWb/zqv/2LB/2K8/wGT/wyd/1u9/1i7/wCS/3PE/6fc/wic/4PN/5LT/0S0";
const char strIcon6[] PROGMEM = "/4nQ/1C5/0a1/1i8/3zK/1a7/4vR//f8/+j2/+n2//L6//r9//D5/+b1/+/5//n8//b7/+34/1gr3N8AAAABdFJOUwBA5thmAAAAAWJLR0QAiAUdSAAAAAlwSFlzAAAOvwAADr8BOAVTJAAAAAd0SU1FB+MHCQcZHBlis8QAAADFSURBVEjHY2DAChISGIgEoypHVQ5ZlQl4AU5tQXjMTRARQwLNzcg8MfsEstyaUMTHgQsw2yWQ5f2EImZGBGCCm8cE4lHDTKYcOVMosGMiZGZCggAxZjLX2zWWQEAYEWYyEGVmqg4HMwQg+R2RAiAsQ0KJCc3MRCbk0MVuJgMVzCQ5zdPMTGgYMnOTYmYQXjP5PFMhoL5Ynzru5GBkEjKCAiEUI8k2M4clFAGMjEKRQTOZZmYgF0NaqKVScMJo6T2qcrioBADiRnaLCHmwugAAAABJRU5ErkJggg==";
const char *const arrIconBase64[] PROGMEM = {strIcon0, strIcon1, strIcon2, strIcon3, strIcon4, strIcon5, strIcon6};

// remote switch enumerations
enum RemoteSwitchSlot { SLOT_START_HOUR, SLOT_START_MINUTE, SLOT_STOP_HOUR, SLOT_STOP_MINUTE };

// remote switch commands
enum RemoteSwitchCommands { CMND_REMOTESWITCH_BUTTON, CMND_REMOTESWITCH_MOTION, CMND_REMOTESWITCH_DEBOUNCE, CMND_REMOTESWITCH_DURATION, CMND_REMOTESWITCH_SLOT0, CMND_REMOTESWITCH_SLOT0_START_HOUR, CMND_REMOTESWITCH_SLOT0_START_MIN, CMND_REMOTESWITCH_SLOT0_STOP_HOUR, CMND_REMOTESWITCH_SLOT0_STOP_MIN, CMND_REMOTESWITCH_SLOT1, CMND_REMOTESWITCH_SLOT1_START_HOUR, CMND_REMOTESWITCH_SLOT1_START_MIN, CMND_REMOTESWITCH_SLOT1_STOP_HOUR, CMND_REMOTESWITCH_SLOT1_STOP_MIN };
const char kRemoteSwitchCommands[] PROGMEM = D_CMND_REMOTESWITCH_BUTTON "|" D_CMND_REMOTESWITCH_MOTION "|" D_CMND_REMOTESWITCH_DEBOUNCE "|" D_CMND_REMOTESWITCH_DURATION "|" D_CMND_REMOTESWITCH_SLOT0 "|" D_CMND_REMOTESWITCH_SLOT0_START_HOUR "|" D_CMND_REMOTESWITCH_SLOT0_START_MIN "|" D_CMND_REMOTESWITCH_SLOT0_STOP_HOUR "|" D_CMND_REMOTESWITCH_SLOT0_STOP_MIN "|" D_CMND_REMOTESWITCH_SLOT1 "|" D_CMND_REMOTESWITCH_SLOT1_START_HOUR "|" D_CMND_REMOTESWITCH_SLOT1_START_MIN "|" D_CMND_REMOTESWITCH_SLOT1_STOP_HOUR "|" D_CMND_REMOTESWITCH_SLOT1_STOP_MIN;

// time slot structure
struct timeslot {
  uint8_t  number;
  uint8_t  start_hour;
  uint8_t  start_minute;
  uint8_t  stop_hour;
  uint8_t  stop_minute;
};

// variables
ulong start_switch   = 0;            // time of latest switch command
ulong start_debounce = 0;            // time of latest debounced command
bool  motion_used = false;

/*********************************************************************************************/

// save motion time slot (format is HH:HH-HH:MM)
void RemoteSwitchMotionSetSlot (uint8_t slot_number, char* strSlot)
{
  uint8_t index = 0;
  char* token;
  char* arr_token[4];
  uint32_t time_setting;
  timeslot time_slot;
  
  // split string into array of values
  token = strtok (strSlot, ":");
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
  Settings.timer[slot_number].data = time_setting;
}

// get motion time slot data
struct timeslot RemoteSwitchMotionGetSlot (uint8_t slot_number)
{
  div_t    div_result;
  uint32_t slot_value = 0;
  timeslot slot_result;

  // set time slot number
  slot_result.number = slot_number;

  // read time slot raw data
  if (slot_number < 2) slot_value = Settings.timer[slot_number].data;

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

  return slot_result;
}

// update motion detector usage according to current time slots
bool RemoteSwitchMotionUpdateUsage ()
{
  uint8_t  index;
  uint8_t  current_hour, current_minute;
  timeslot current_slot;

  // get current time
  current_hour = RtcTime.hour;
  current_minute = RtcTime.minute;

  // loop thru both time slots
  for (index = 0; index < 2; index ++)
  {
    // get current slot
    current_slot = RemoteSwitchMotionGetSlot (index);

    // update motion collect state
    if ((current_hour == current_slot.start_hour) && (current_minute == current_slot.start_minute)) motion_used = false;
    else if ((current_hour == current_slot.stop_hour) && (current_minute == current_slot.stop_minute)) motion_used = true;
  }
  
  return motion_used;
}

// get push button enable status
bool RemoteSwitchButtonIsEnabled ()
{
  return (Settings.display_model == 1);
}

// set push button status
void RemoteSwitchButtonEnable (uint8_t status)
{
  if (status > 1) status = 1;
  Settings.display_model = status;
}

// get motion detection enable status
bool RemoteSwitchMotionIsEnabled ()
{
  return (Settings.display_mode == 1);
}

// set motion detection status
void RemoteSwitchMotionEnable (uint8_t status)
{
  if (status > 1) status = 1;
  Settings.display_mode = status;
}

// get remote switch debounce delay (sec)
uint8_t RemoteSwitchGetDebounce ()
{
  return Settings.display_refresh;
}

// set remote switch debounce delay (sec)
void RemoteSwitchSetDebounce (uint8_t debounce)
{
  Settings.display_refresh = debounce;
}

// get remote switch standard duration (sec)
uint8_t RemoteSwitchGetDuration ()
{
  return Settings.display_size;
}

// set remote switch standard duration
void RemoteSwitchSetDuration (uint8_t duration)
{
  Settings.display_size = duration;
}

// Show JSON status (for MQTT)
void RemoteSwitchShowJSON (bool append)
{
  bool     button_enabled, button_state;
  bool     motion_enabled, motion_state;
  uint8_t  relay_state, relay_duration;
  uint8_t  debounce, duration;
  timeslot slot_0, slot_1;

  // collect data
  button_enabled = RemoteSwitchButtonIsEnabled ();
  button_state = 1;
  motion_enabled = RemoteSwitchMotionIsEnabled ();
  motion_state = 0;
  debounce = RemoteSwitchGetDebounce ();
  duration = RemoteSwitchGetDuration ();
  slot_0 = RemoteSwitchMotionGetSlot (0);
  slot_1 = RemoteSwitchMotionGetSlot (1);
  
  // read relay state
  relay_state = bitRead (power, 0);

  // start message  -->  {  or  ,
  if (append == false) snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("{"));
  else snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,"), mqtt_data);

  // "RemoteSwitch":{"Debounce":2,"Duration":35,
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_REMOTESWITCH "\":{\"" D_JSON_REMOTESWITCH_DEBOUNCE "\":%d,\"" D_JSON_REMOTESWITCH_DURATION "\":%d,"), mqtt_data, debounce, duration);
  
  // "Relay":{"State":1,"Duration":35},
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_REMOTESWITCH_RELAY "\":{\"" D_JSON_REMOTESWITCH_STATE "\":%d,\"" D_JSON_REMOTESWITCH_DURATION "\":%d},"), mqtt_data, relay_state, relay_duration);

  // "PushButton":{"Enabled":1,"State":0},
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_REMOTESWITCH_BUTTON "\":{\"" D_JSON_REMOTESWITCH_ENABLED "\":%d,\"" D_JSON_REMOTESWITCH_STATE "\":%d},"), mqtt_data, button_enabled, button_state);

  // "MotionDetector":{"Enabled":1,"State":1,"Slot1":"01:00-12:00","Slot2":"00:00-00:00"}}
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_REMOTESWITCH_MOTION "\":{\"" D_JSON_REMOTESWITCH_ENABLED "\":%d,\"" D_JSON_REMOTESWITCH_STATE "\":%d}}"), mqtt_data, motion_enabled, motion_state);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_REMOTESWITCH_SLOT "1\":\"%2d:%2d-%2d:%2d\","), mqtt_data, slot_0.start_hour, slot_0.start_minute, slot_0.stop_hour, slot_0.stop_minute);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_REMOTESWITCH_SLOT "2\":\"%2d:%2d-%2d:%2d\"},"), mqtt_data, slot_1.start_hour, slot_1.start_minute, slot_1.stop_hour, slot_1.stop_minute);

  // if not in append mode, publish message 
  if (append == false)
  { 
    // end of message   ->  }
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);

    // publish full state
    MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
  }
}

// Handle remote switch MQTT commands
bool RemoteSwitchCommand ()
{
  bool serviced = true;
  timeslot time_slot;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kRemoteSwitchCommands);

  // handle command
  switch (command_code)
  {
    case CMND_REMOTESWITCH_BUTTON:       // enable/disable push button
      RemoteSwitchButtonEnable(XdrvMailbox.payload);
      break;
    case CMND_REMOTESWITCH_MOTION:     // enable/disable motion detector
      RemoteSwitchMotionEnable(XdrvMailbox.payload);
      break;
    case CMND_REMOTESWITCH_DEBOUNCE:  // set debounce delay
      RemoteSwitchSetDebounce (XdrvMailbox.payload);
      break;
    case CMND_REMOTESWITCH_DURATION:  // set switch minimum duration
      RemoteSwitchSetDuration (XdrvMailbox.payload);
      break;
    case CMND_REMOTESWITCH_SLOT0:     // set motion detector first disable slot
      RemoteSwitchMotionSetSlot (0, XdrvMailbox.data);
      break;
    case CMND_REMOTESWITCH_SLOT1:     // set motion detector second disable slot
      RemoteSwitchMotionSetSlot (1, XdrvMailbox.data);
      break;
    default:
      serviced = false;
  }

  // send MQTT status
  if (serviced == true) RemoteSwitchShowJSON (false);
  
  return serviced;
}

// update pilot wire relay states according to current status
void RemoteSwitchEvery250MSecond ()
{
}

#ifdef USE_WEBSERVER

// Pilot Wire icon
void RemoteSwitchWebDisplayIcon (uint8_t height)
{
  uint8_t nbrItem, index;

  // display img according to height
  WSContentSend_P ("<img height=%d src='data:image/png;base64,", height);

  // loop to display base64 segments
  nbrItem = sizeof (arrIconBase64) / sizeof (char*);
  for (index=0; index<nbrItem; index++) { WSContentSend_P (arrIconBase64[index]); }

  WSContentSend_P ("'/>");
}

// remote switch configuration button
void RemoteSwitchWebConfigButton ()
{
  // beginning
  WSContentSend_P (PSTR ("<table style='width:100%%;'><tr>"));

  // vmc icon
  WSContentSend_P (PSTR ("<td align='center'>"));
  RemoteSwitchWebDisplayIcon (32);
  WSContentSend_P (PSTR ("</td>"));

  // button
  WSContentSend_P (PSTR ("<td><form action='%s' method='get'><button>%s</button></form></td>"), D_PAGE_REMOTESWITCH, D_REMOTESWITCH_CONFIGURE);

  // end
  WSContentSend_P (PSTR ("</tr></table>"));
}

// remote switch web configuration page
void RemoteSwitchWebPage ()
{
  bool     updated = false;
  bool     button_enabled, motion_enabled;
  uint8_t  duration, debounce;
  timeslot slot_0, slot_1;
  char     argument[REMOTESWITCH_LABEL_BUFFER_SIZE];
  char     slot[REMOTESWITCH_LABEL_BUFFER_SIZE];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // get remote switch button enabled according to 'button' parameter
  if (WebServer->hasArg(D_CMND_REMOTESWITCH_BUTTON))
  {
    WebGetArg (D_CMND_REMOTESWITCH_BUTTON, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    RemoteSwitchButtonEnable ((uint8_t) atoi (argument)); 
    updated = true;
  }

  // get remote switch motion detector enabled according to 'motion' parameter
  if (WebServer->hasArg(D_CMND_REMOTESWITCH_MOTION))
  {
    WebGetArg (D_CMND_REMOTESWITCH_MOTION, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    RemoteSwitchMotionEnable ((uint8_t) atoi (argument)); 
    updated = true;
  }

  // get remote switch debounced delay according to 'debounce' parameter
  if (WebServer->hasArg(D_CMND_REMOTESWITCH_DEBOUNCE))
  {
    WebGetArg (D_CMND_REMOTESWITCH_DEBOUNCE, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    RemoteSwitchSetDebounce ((uint8_t) atoi (argument)); 
    updated = true;
  }

  // get remote switch duration according to 'duration' parameter
  if (WebServer->hasArg(D_CMND_REMOTESWITCH_DURATION))
  {
    WebGetArg (D_CMND_REMOTESWITCH_DURATION, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    RemoteSwitchSetDuration ((uint8_t) atoi (argument)); 
    updated = true;
  }

  // get remote switch first motion detector slot according to 'slot0' parameters
  if (WebServer->hasArg(D_CMND_REMOTESWITCH_SLOT0_START_HOUR))
  {
    WebGetArg (D_CMND_REMOTESWITCH_SLOT0_START_HOUR, slot, REMOTESWITCH_LABEL_BUFFER_SIZE);
    WebGetArg (D_CMND_REMOTESWITCH_SLOT0_START_MIN, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    strcat (slot, ":");
    strcat (slot, argument);
    WebGetArg (D_CMND_REMOTESWITCH_SLOT0_STOP_HOUR, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    strcat (slot, "-");
    strcat (slot, argument);
    WebGetArg (D_CMND_REMOTESWITCH_SLOT0_STOP_MIN, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    strcat (slot, ":");
    strcat (slot, argument);
    RemoteSwitchMotionSetSlot (0, slot); 
    updated = true;
  }

  // get remote switch second motion detector slot according to 'slot1' parameters
  if (WebServer->hasArg(D_CMND_REMOTESWITCH_SLOT1_START_HOUR))
  {
    WebGetArg (D_CMND_REMOTESWITCH_SLOT1_START_HOUR, slot, REMOTESWITCH_LABEL_BUFFER_SIZE);
    WebGetArg (D_CMND_REMOTESWITCH_SLOT1_START_MIN, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    strcat (slot, ":");
    strcat (slot, argument);
    WebGetArg (D_CMND_REMOTESWITCH_SLOT1_STOP_HOUR, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    strcat (slot, "-");
    strcat (slot, argument);
    WebGetArg (D_CMND_REMOTESWITCH_SLOT1_STOP_MIN, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    strcat (slot, ":");
    strcat (slot, argument);
    RemoteSwitchMotionSetSlot (1, slot); 
    updated = true;
  }

  // if parameters updated, back to main page
  if (updated == true)
  {
    WebServer->sendHeader ("Location", "/", true);
    WebServer->send ( 302, "text/plain", "");
  }
  
  // read data
  slot_0 = RemoteSwitchMotionGetSlot (0);
  slot_1 = RemoteSwitchMotionGetSlot (1);
  button_enabled = RemoteSwitchButtonIsEnabled ();
  motion_enabled = RemoteSwitchMotionIsEnabled ();
  duration = RemoteSwitchGetDuration ();
  debounce = RemoteSwitchGetDebounce ();
  
  // beginning of form
  WSContentStart_P (D_REMOTESWITCH_CONFIGURE);
  WSContentSendStyle ();

  // form
  WSContentSend_P (PSTR ("<fieldset><legend><b>&nbsp;%s&nbsp;</b></legend><form method='get' action='%s'>"), D_REMOTESWITCH_PARAMETERS, D_PAGE_REMOTESWITCH);

  // duration
  WSContentSend_P (PSTR ("<p><b>%s</b><br/><input type='number' name='%s' min='0' step='1' value='%d'></p>"), D_REMOTESWITCH_DURATION, D_CMND_REMOTESWITCH_DURATION, duration);

  // debounce
  WSContentSend_P (PSTR ("<p><b>%s</b><br/><input type='number' name='%s' min='0' step='1' value='%d'></p>"), D_REMOTESWITCH_DEBOUNCE, D_CMND_REMOTESWITCH_DEBOUNCE, debounce);

  // push button input
  WSContentSend_P (PSTR ("<p><b>%s</b>"), D_REMOTESWITCH_BUTTON);
  if (button_enabled == true) strcpy (argument, "checked");
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<br/><input type='radio' name='%s' value='1' %s>%s"), D_CMND_REMOTESWITCH_BUTTON, argument, D_REMOTESWITCH_ENABLE);
  if (button_enabled == true) strcpy (argument, "");
  else strcpy (argument, "checked");
  WSContentSend_P (PSTR ("<br/><input type='radio' name='%s' value='0' %s>%s"), D_CMND_REMOTESWITCH_BUTTON, argument, D_REMOTESWITCH_DISABLE);
  WSContentSend_P (PSTR ("</p>"));

  // motion detector input
  WSContentSend_P (PSTR ("<p><b>%s</b>"), D_REMOTESWITCH_MOTION);
  if (motion_enabled == true) strcpy (argument, "checked");
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<br/><input type='radio' name='%s' value='1' %s>%s"), D_CMND_REMOTESWITCH_MOTION, argument, D_REMOTESWITCH_ENABLE);
  if (motion_enabled == true) strcpy (argument, "");
  else strcpy (argument, "checked");
  WSContentSend_P (PSTR ("<br/><input type='radio' name='%s' value='0' %s>%s"), D_CMND_REMOTESWITCH_MOTION, argument, D_REMOTESWITCH_ENABLE);

  WSContentSend_P (PSTR ("<br />%s "), D_REMOTESWITCH_FROM);
  WSContentSend_P (PSTR ("<input type='number' name='%s' style='width:10%%;' min='0' max='23' step='1' value='%d'> h "), D_CMND_REMOTESWITCH_SLOT0_START_HOUR, slot_0.start_hour);
  WSContentSend_P (PSTR ("<input type='number' name='%s' style='width:10%%;' min='0' max='59' step='1' value='%d'>"), D_CMND_REMOTESWITCH_SLOT0_START_MIN, slot_0.start_minute);
  WSContentSend_P (PSTR (" %s "), D_REMOTESWITCH_UNTIL);
  WSContentSend_P (PSTR ("<input type='number' name='%s' style='width:10%%;' min='0' max='23' step='1' value='%d'> h "), D_CMND_REMOTESWITCH_SLOT0_STOP_HOUR, slot_0.stop_hour);
  WSContentSend_P (PSTR ("<input type='number' name='%s' style='width:10%%;' min='0' max='59' step='1' value='%d'>"), D_CMND_REMOTESWITCH_SLOT0_STOP_MIN, slot_0.stop_minute);

  WSContentSend_P (PSTR ("<br />%s "), D_REMOTESWITCH_FROM);
  WSContentSend_P (PSTR ("<input type='number' name='%s' style='width:10%%;' min='0' max='23' step='1' value='%d'> h "), D_CMND_REMOTESWITCH_SLOT1_START_HOUR, slot_1.start_hour);
  WSContentSend_P (PSTR ("<input type='number' name='%s' style='width:10%%;' min='0' max='59' step='1' value='%d'>"), D_CMND_REMOTESWITCH_SLOT1_START_MIN, slot_1.start_minute);
  WSContentSend_P (PSTR (" %s "), D_REMOTESWITCH_UNTIL);
  WSContentSend_P (PSTR ("<input type='number' name='%s' style='width:10%%;' min='0' max='23' step='1' value='%d'> h "), D_CMND_REMOTESWITCH_SLOT1_STOP_HOUR, slot_1.stop_hour);
  WSContentSend_P (PSTR ("<input type='number' name='%s' style='width:10%%;' min='0' max='59' step='1' value='%d'>"), D_CMND_REMOTESWITCH_SLOT1_STOP_MIN, slot_1.stop_minute);
  
  WSContentSend_P (PSTR ("</p>"));

  // end of form
  WSContentSend_P(HTTP_FORM_END);
  WSContentSpaceButton(BUTTON_CONFIGURATION);
  WSContentStop();
}

// append pilot wire state to main page
bool RemoteSwitchWebState ()
{
  float   corrected_temperature;
  float   target_temperature;
  uint8_t actual_mode;
  uint8_t actual_state;
  char*   actual_label;
  char    argument[REMOTESWITCH_LABEL_BUFFER_SIZE];

  // add push button state
  snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><th>%s</th><td>%s</td></tr>", mqtt_data, D_REMOTESWITCH_BUTTON, D_REMOTESWITCH_ON);

  // add motion detector state
  snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><th>%s</th><td>%s</td></tr>", mqtt_data, D_REMOTESWITCH_MOTION, D_REMOTESWITCH_OFF);

  // add times
  snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><th>%s</th><td>%d</td></tr>", mqtt_data, D_REMOTESWITCH_TIME_ON, 12);
  snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><th>%s</th><td>%d</td></tr>", mqtt_data, D_REMOTESWITCH_TIME_REMAIN, 12);
}

#endif  // USE_WEBSERVER

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns97 (byte callback_id)
{
  bool result = false;

  // main callback switch
  switch (callback_id)
  {
    case FUNC_COMMAND:
      result = RemoteSwitchCommand ();
      break;
    case FUNC_EVERY_250_MSECOND:
      RemoteSwitchEvery250MSecond ();
      break;
    case FUNC_JSON_APPEND:
      RemoteSwitchShowJSON (true);
      break;

#ifdef USE_WEBSERVER

    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_REMOTESWITCH, RemoteSwitchWebPage);
      break;
    case FUNC_WEB_APPEND:
      RemoteSwitchWebState ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      RemoteSwitchWebConfigButton ();
      break;

#endif  // USE_WEBSERVER

  }
  return result;
}

#endif // USE_REMOTESWITCH
