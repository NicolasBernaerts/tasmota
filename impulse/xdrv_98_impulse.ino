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
const char strIcon0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAAEAAAAAgCAMAAACVQ462AAAK4XpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHjarZlpdjM5b4X/cxVZQnEml8PxnOwgy88DsCTLtuSv0ydSv64SySIB3Iup2qz/+e9t/ouPTzWYEHNJNaWLT6ihusZNuc7nXO0V9K9+wj3F72/jxj0mHEOeqz8/07rXN8bj1wP53sn27+Mmj3ufcm90Tzw29HKynHavK/dG3p1xe/829X6uhRd17n81X9/0+fk7ZIwxI/t5Z9zy1l/8LXKKRwJffeOa+cs9iy5fuHc6br17bzvzvP1hvPLBdle7V/jvpjBXuhekHza6x218bzu10KtE9nHrvk/Ecs3r9fNiu71n2Xsd7VpIWCqZW6nr3kLvWNgxpdfHEt/Mv8h91m/lW1BxgNgEzc53GFutw9rbBjtts9suvQ47EDG45TJX54bzOlZ8dtUNBSXI126XgWEaEHF+gJpn2D1lsXpu1fOGFTWnZaWzbAZyv7/m3eC/+T432luoa60YsxxbIZcT1iCGICd/WQUgdt82jWpf/ZonrF8fAdaDYFQzFxRsVz9b9Gi/uOUVZ8+6eAVz093meW+AiTg7Ioz1IHAl66NN9srOZWuxYwGfhuTOB9dBwMbopjUbbLxPgFOcnM0z2epaF90ZJrQARPQJtyniKIAVQoQ/ORQ41KKPwcQYU8yxxBpb8imkmFLKSWJUyz6HHHPK";
const char strIcon1[] PROGMEM = "OZdccyu+hBJLKrmUUkurrnpCWKypZlNLrbU1Dm1s3Xi6saK17rrvoceeeu6l194G9BlhxJFGHmXU0aabfuL+M81sZpl1tmUXVFphxZVWXmXV1TZc236HHXfaeZddd3uidqP6HTX7A7m/UbM3aoJY0HX5CzWGc35sYSWcRMEMxFywIJ4FAQjtBLOr2BCcICeYXdXhFNGBmo0CzrSCGAiGZV3c9ondF3J/4mZi+D/h5j4hZwS6/w/kjEB3I/cbtzeozaYZxStA4oVi08tvAtvOqbnSXIf+l97htM/rvgIPx7xXioTFntnsuiYWWzvHddXdg7DCjI5LJ05luodEiCrZzuF3SvoIShNV58ys6jF37orLa0x9AsAkhLESidbkkdzBbFg3h4rS0hvh/ryaXxN+WMSpHFhr8gizupydyrS7e3QcY2wdkYkeW1HVzS/dwy23Ss2IyC1CizFinui+lgrdCOl6I/m+mcfNufanOGKdp0CI87TOdCqhdyNjHWy3SBZhm2QH3rFnbR3hUlaj5yg4zy9g3qnURbTYr9AzZjHIUkt5kfFfXfdlPrDkk6U+KG63+ZKafdNBCLkbxrBQOggx5NxW6mEn8+XerNWg6/GtbRDoBvANd9tueK+tCLanyxMZYt341GqxExH8YItl5WHDkJMHB9upKdtz7kzphEjbGmzfu4QMLPXI7qzOxVWMivo1JzOMxFXjrFE2QZ59dDmaiB4q8TngoaZ51VPU7LOy";
const char strIcon2[] PROGMEM = "qbWLB4+So8CJmVVQGcY2MqHDbJibW8NXE+1SYJRn0VU0iuGwKcZMfLCuLyeAEIr2GqwqVU0O53DwMRVD0tH2OS8IyaJGPdn93A1zhioLrkIkInKxfeC41Se+6Gc5D7/KZUQwN4haSUxMqlB1Ssyg03Qz6zhkXsCZxRwEuxXTEAtwfBorZ6aCaWv3P1DyZDzl2OgPsQg8fdqf8pojsCXhkBIKzwwvQk2CN4Ek6gNritnOBGroFKrJJFNxiZ3MOzvI098HjxSc9pEQ5jcj/h0hzG9G/EmIY5s7Jr1Mdsqa3L2wxDMXThKol5q78cuPPXBloqxuQepe8GWdVJH3BWHKFohMKr1fsuskqUZL3q21yFXanv9wtQu22d6HJ4zsqz7O2i0Rk9F0y2Gju7m7kCcvzkWb3JVpPv5imk94/wvViFNPqrUn1WBaIiARegnKk1B9xRe/6eP4Daip61AUTGSEEJQC2/UkG8qSlDEeARIzA8eL183wzesMbncyan54nbLm1e+KVc8pyo6uQHSNYWzrxUt3wkZeAFw4MQbZbE0FUqg1dl6dJFJVDHvYo2rME2xrzw8tmmiRzK2GkN0/1XBhrZFmrHZc+30m29RmiLVig5V7JgN2+VNKlNpJOc7DbNjx6QzfOy6uTHH2iygQsvNBI5y8+py45BFJIyvkQcllxQBilYQtYASNARNZ0kQtWVyRsxDfFAnnytRFdalynYDOubP0AE+F83AIhvnblXrE";
const char strIcon3[] PROGMEM = "xmwcpj5hj/dL2Lt0tXTkU0II2m7nT1bxQk6KXvzumsdl9SDKPFzhuaF57HiHKx1HfDlKhvUUylM45cbuiVpnS8b3ScwZ26J9ZDzjtJlKMlD4nTq/DwitJUWXEr/xYKe0lGTc9bjEIqn2ULXiLN5jMMJhNVVS2lyKDuCh5HSKRKRADlbewbjP0fEZOKf5GVLbEq4PTflaLX2K5yxIdQ700VrG/LWYCHHhXKdG+6OAkDiq8L9NyOTsC1ErxVaQJoMwOmjOcYqdDiARZEcVWCjaDe2Br5COZE/IYGkcNn/5qFTaQCDOoB7aIKjEyOOgjZDj1UGnqQM2JqkZ1qQrmP80Rf/M0OaECodrYH0pIounQBbZK2GsS05iKTLHfUIHzgElOs4kHpkINnnjuWxE3HDYqUvEwOyIjKHFIGhPW0HVv0RAnq1KxAMFOuEFTLreURjUrgCIf3Qc0BdjDdwLVSXMpbeBxfyOLE0eHhkna1Lyeh9Koi8MdGLcS3bD2wriyiq2QHpMaWiRV+p4vG04DdZ8TpFupqNkpElG4bRaiydnnJ8/uhEjN68+88NlSHcudUDQfk3VkaKgj+MJ8tZyt3C3EFMiw4O3p7lqEpOFtxpWelLaytSZ0GGpvpOW/y0myhqFXP1SKu6pAaTeoW5KAtAEjHkvQC/x01rzXHxNOE08lt5DGH23drTWAoTEvZ604NEJHRZWnYDYk9G2x4ZGSUL9/+j6vFCrakZ9UEu3xXfy";
const char strIcon4[] PROGMEM = "g1xJSEGgV3INjZApfrFL8s9rhgfGlwx/ysAu3nxSfJKMKineNFpv/K5Dc6km3yamdO1xLcTT8ueYmXMLjlO2190l+BMDUpoTjHO3gw1hAKGpTVEBFp6WpnxuUXOnuTcC5iP/LcL9ab5/9N70gFuSGYeHrSEF8vo+RBbiQ5rkNa9BNtBnNdLQHNMJctsJwqMOSiJyAotWpSaQWODujJOrKilBQ5hg3gbz/Ls8LimLzYezWiJKBNAiUepH7deWPWmvKZMp144xcY6avu/693HmnEc5qDsVpdnUzUjr9q6VWps7lwv69HXzadAWjLsnuMiFxmvOwGca9dppJvzvfII3OE+iPK3hFlezQchI6NYqb5s5q+RWcC47iC+nUag/KD0Jz5KNYyCm+flq2B+KKl0NxXf9WXxHTbiXvHkYhwLvV1kt66uW9eZbXf/j+ceit7X/j43N8/wq0W1rdBMXecY3yKy54K1acVwkQfqcYsR9pAXXcsWKk9NRVM2enUod15HClT6BDNukkJo5v9YY/Cf/cySax83b69dble+V6JvXVEbeU9n3HlY+vHrx0gnA5xPJoo0hFJXobdvy+RWPFDS/86B5vFSp5Oyylv38iue8W/r05s0Qbq+a1rGKt9c/7LB+Xc2z9Xq+CPxnlgo/FDc/gKinqj1Siw9GMQaVzqh3YoKS7g3KN/xfbybxqHG3nSIUIn3Z5108Bzv60Q4h37+5ekLz6d1bcLO42O9mZATz";
const char strIcon5[] PROGMEM = "iYv/9PqgifnNk4/Wsp+UFzzMU3KVWzj1lrpV3qiY/wXNfgLLEqQDdQAAAltQTFRFIW4C/////v7+/f398/Pz4eHh8vLy4uLi/Pz8q6urNzc3+/v7qampODg4+vr6+Pj4qKioNjY29/f3Ozs7rKysOjo6xcXFiYmJioqKh4eHk5OTXFxcHh4elJSUnp6eysrKhYWFPT09Xl5eW1tbT09PUFBQUVFRV1dXYGBgUlJSTExMDg4O6enps7OzDw8P6+vrU1NTi4uL9vb2ISEh5ubmm5ubQkJChoaG8fHxHR0d5eXlMTExeXl5DQ0Nzc3NoKCgaWlpRkZGhISEfHx8fn5+cnJyr6+vKCgot7e3tbW1CgoKZWVlWlpa4+PjEhISzMzMra2tMzMzgICAJycnwsLC9fX17+/vFBQU7e3tKysrGRkZFxcXGxsbICAguLi4EBAQAAAA6urq6Ojotra2y8vLpKSkqqqqZ2dnSEhIJiYmubm5srKy2dnZDAwMLy8viIiI+fn59PT03NzceHh43t7e5OTk5+fn2traYmJiIiIiHx8fFhYWCwsL7OzsLS0tpqamNTU13d3dOTk5x8fHfX19/v//+/3//f7/9fv//v7/+Pz/+f3/7Pf//P7/iM//KKn/4vP/b8b/9Pr/2O//7vj/acP/xej/Trf/zOr/wef/ltT/TrX/ptz/x+n/QLP/yun/Xbz/vOX/q93/eMf/3PH/gs3/l9X/Z8L/FZv/Oq//";
const char strIcon6[] PROGMEM = "YsH/Yrz/AZP/DJ3/W73/WLv/AJL/c8T/p9z/CJz/g83/ktP/RLT/idD/ULn/RrX/WLz/fMr/Vrv/i9H/9/z/6Pb/6fb/8vr/+v3/8Pn/5vX/7/n/+fz/9vv/7fj/UfUfIQAAAAF0Uk5TAEDm2GYAAAABYktHRACIBR1IAAAACXBIWXMAAA6/AAAOvwE4BVMkAAAAB3RJTUUH4wcTBh0MXt+7aAAAAMJJREFUSMdjYEAFCQkMRAIcKkcNGLEGJOAFOA0IghmSICKGBJqbkXli9gmEXZFQxMeBCzDbJRD2RUIRMyMCMME1M4F4JBvAlCNnCgV2TIQMSEgQwDCAud6usQQCwogwgAHTgFQdDmYIQPICIj4gLEOUeEQzIJEJOUSwG8BAqgH4ExJ1DID6m5mbFAOCEAbweaZCQH2xPhku4GBkEjKCAiEU/cQZkMMSigBGRqHIoJkYAzKQM58Wal4MThgt0kYNIMYAAFqRdovyYmCAAAAAAElFTkSuQmCC";
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
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_IMPULSE_WINDOW "\":\"%02d:%02d-%02d:%02d\"}}"), mqtt_data, mwindow.start_hour, mwindow.start_minute, mwindow.stop_hour, mwindow.stop_minute);

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
   }

  // get motion detector mode according to 'motion' parameter
  if (WebServer->hasArg(D_CMND_IMPULSE_MOTION))
  {
    WebGetArg (D_CMND_IMPULSE_MOTION, argument, IMPULSE_LABEL_BUFFER_SIZE);
    ImpulseMotionSetMode (atoi (argument)); 
   }

  // get motion detector tempo according to 'mtempo' parameter
  if (WebServer->hasArg(D_CMND_IMPULSE_MOTION_TEMPO))
  {
    WebGetArg (D_CMND_IMPULSE_MOTION_TEMPO, argument, IMPULSE_LABEL_BUFFER_SIZE);
    ImpulseMotionSetTempo ((uint8_t) atoi (argument)); 
  }

  // get push button tempo according to 'btempo' parameter
  if (WebServer->hasArg(D_CMND_IMPULSE_BUTTON_TEMPO))
  {
    WebGetArg (D_CMND_IMPULSE_BUTTON_TEMPO, argument, IMPULSE_LABEL_BUFFER_SIZE);
    ImpulseButtonSetTempo ((uint8_t) atoi (argument)); 
   }

  // get motion detector active window according to 'window' parameters
  if (WebServer->hasArg(D_CMND_IMPULSE_START))
  {
    WebGetArg (D_CMND_IMPULSE_START, argument, IMPULSE_LABEL_BUFFER_SIZE);
    strcpy (window, argument);
    strcat (window, "-");
    WebGetArg (D_CMND_IMPULSE_STOP, argument, IMPULSE_LABEL_BUFFER_SIZE);
    strcat (window, argument);
    AddLog_P(LOG_LEVEL_INFO, window);
    ImpulseMotionSetWindow (window); 
   }

  // collect data
  bmode     = ImpulseButtonGetMode ();
  mmode     = ImpulseMotionGetMode ();
  btempo    = ImpulseButtonGetTempo (false);
  mtempo    = ImpulseMotionGetTempo (false);
  bpressed  = ImpulseButtonIsPressed ();
  mdetected = ImpulseMotionIsDetected ();
  mwindow   = ImpulseMotionGetWindow ();
  
  // beginning of form
  WSContentStart_P (D_IMPULSE_CONFIGURE);
  WSContentSendStyle ();

  // form
  WSContentSend_P (PSTR ("<fieldset><legend><b>&nbsp;%s&nbsp;</b></legend><form method='get' action='%s'>"), D_IMPULSE_PARAMETERS, D_PAGE_IMPULSE);

  // push button mode
  WSContentSend_P (PSTR ("<p><b>%s</b></p>"), D_IMPULSE_BUTTON);
  
  if (bmode == IMPULSE_DISABLE) { strcpy (argument, "checked"); } else { strcpy (argument, ""); }
  WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>"), D_CMND_IMPULSE_BUTTON, IMPULSE_DISABLE, argument, D_IMPULSE_DISABLE);
  
  if (bmode == IMPULSE_ENABLE) { strcpy (argument, "checked"); } else { strcpy (argument, ""); }
  WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>"), D_CMND_IMPULSE_BUTTON, IMPULSE_ENABLE, argument, D_IMPULSE_ENABLE);

  // push button tempo
  WSContentSend_P (PSTR ("<p>%s <i>(mn)</i></p>"), D_IMPULSE_BUTTON_TEMPO);
  WSContentSend_P (PSTR ("<p><input type='number' name='%s' style='width:100px;' min='0' max='255' step='1' value='%d'></p>"), D_CMND_IMPULSE_BUTTON_TEMPO, btempo);

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
  WSContentSend_P (PSTR ("<p><input type='number' name='%s' style='width:100px;' min='0' max='255' step='1' value='%d'></p>"),  D_CMND_IMPULSE_MOTION_TEMPO, mtempo);

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
