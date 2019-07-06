/*
  xsns_98_vmc.ino - Ventilation Motor Controled support 
  for Sonoff TH, Sonoff Basic or SonOff Dual
  
  Copyright (C) 2019 Nicolas Bernaerts

  Settings are stored using weighting scale parameters :
    - Settings.weight_reference   = VMC mode
    - Settings.weight_max         = Target humidity level (%)
    - Settings.weight_calibration = Humidity thresold (%) 
    
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

#ifdef USE_VMC

/*********************************************************************************************\
 * Fil Pilote
\*********************************************************************************************/

#define XSNS_98                  98

// web configuration page
#define D_PAGE_VMC               "vmc"
#define D_CMND_VMC_MODE          "mode"
#define D_CMND_VMC_TARGET        "target"
#define D_CMND_VMC_THRESHOLD     "thres"

// JSON data
#define D_JSON_VMC                "VMC"
#define D_JSON_VMC_MODE           "Mode"
#define D_JSON_VMC_STATE          "State"
#define D_JSON_VMC_LABEL          "Label"
#define D_JSON_VMC_TARGET         "Target"
#define D_JSON_VMC_HUMIDITY       "Humidity"
#define D_JSON_VMC_THRESHOLD      "Threshold"
#define D_JSON_VMC_RELAY          "Relay"

#define VMC_COLOR_BUFFER_SIZE     8
#define VMC_LABEL_BUFFER_SIZE     16
#define VMC_MESSAGE_BUFFER_SIZE   64

// icon coded in base64
const char strIcon0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAQAAADZc7J/AAAGsHpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHjarVdr0uQmDPzPKXIEEIjHccSrKjfI8dMCxjOenW82SWVc+2FjjKRuqcWa8def0/yBH1GMJnDKscRo8QslFBLcZLt/e3Q2rL/rR+cVnm/z5npBmPIY/X6M46wXzPPzgxTOfL3Pm9TOPvls5OzNtFfLen/W5bORpz3vzrMp5zsJL+Gcf9TOto+w3p5DAhidsZ8nQ8M7b/E3qxUPD3zxgjHiL/lCjxmHJ/HBh8/Ymev2Dbzr7g07K2fe36EwNp4F8Q2jM+/4bd5fZujmkXtavr1owSb7+nvBbs6e5xw7OgkRSEVzgnqEsu6wsAJKvz6LuBL+Me7TugqujBAbGOtgs+JqxhVHQHu64LoTN91YY3MNLgYalDASNfJrLvtEhdoiJejlJiWQ0Y3PYKOBNY9punxxy25Z9prLsNwdVpLDZsrfL5f5NPlfrmujOTV1nbP5wgp+keY03FDm9C9WgRA3D6a88F2Xeckb+0KsB4O8YM4IUGzdW1R2z9zyi2ePdWyDsbs0XOpnA0AE2wxnnAcDNjrPLjqbiJJzwDGDH4Hn5ANVMOCYqTszwY33EeRkUtv4Jrm1lpj2NKQFRDCKJoEalAvICoGRPylk5JCw52CYOXLizIUl+hgixxhTVI2S5FNInGJKKaeSJPscMueYU865Z";
const char strIcon1[] PROGMEM = "ClUPCSMSyzJlFxKEYFRwdaCrwUrRCpVX0PlGmuquZYqDenTQuMWW2q5lSaduu8o/x57Mj330mW4gVQaYfCII408ypCJXJt+hskzzjTzLFMu1g6rd9bcG3PfWXOHNWUsrHXpyRqmU3ps4VROWDkDYxQcGE/KABKalDObXQikzClnthCKggmsOVZyulPGwGAYjni6i7snc195Mxz+FW/0E3NGqfs/mDNK3WHuV94+sNZldRS/CNIqVEytnxC2YUcWyqItSahmj7sxcRPhU0Dr8gNBeiyxNHuKhUcqoQbrRgYwySXuwkZLixtzwkt8lNCouszg9DvfZixUux0aljqC8FnygELEGmL1y/4MXobxo3qshrUIgLLom8rcW09loBqxtyADuDnYghso58DujJBdOGa7j9moZauRfRwrShuBwbluYQPwVZhbIIwgIhpDWjuaGYZi0kXxKQplI3X+u4HUn1EBTYfQpuiL6ZvUBGH59vV9dJMBQWauCNQFM0pVQPShWfYK7cJJt2bpQZocPvPX0ewbBlG0/eIFH/KZOorgDu6X0cCLUTbzgo7GyFWE+FzgWukx7EXfdjNvE6ga11e2pVFprBxq8x9AZm4TfKJ0iHKs8QmDzkRUFrGertTybTTvE3tkgRKjFrIi1jLk4bkFotTELPdvzMsmKKyYWeOTMWPNWQQnRheH1JM2UKUkciGKEwXbzgVvxOjXgDN+dvgaOSLfCBZ6zKcMNWZIjlZ4rM";
const char strIcon2[] PROGMEM = "VAPvJL4gTp+q4VJCyjKvVzJC+qRB2K+HD0j/CbX/Gn0dEObddiRQgoXY36Qb6WMQGnGnewsS6FEcgIwUM8Z/UE+YgDTPJz42wfapNQVSrumvcVh6wxx5sHRuPNC8EpN6C0oXOfS6PQ/Xmji0Mtmv0j6xgCULeTBq0BZhYE0Fmo8imemgvgkO1oRTdlbNO1LNXISB0+e4x1dbuRjT5oIMsCIsBSTqS1wVIrZIcw8EOHi4rRUH6m76J66LRlQDjMq3IQI3hRTlaO631WBF8TnFRHh6hjKfEvmc0NEKu76qqLx5dR65JDbI2GBOGeyhNmGxJGnm6qnpuoIJF2CZjA/eFBM4qWwmrWaP7gJKbo/lS65nGzWgE2xIHBapMtjzTBsYxWxiKxWl65+VBwOLnWo6Ehj5bxo74aT1y5DG38Lme7rpSUFKX0YMDoeCQwCkr0f5BaM2UDLh2dv1ZUhtVq6T0myHL5qJCXpuHws+9UDnlVfs8MEyBB5Ddya+4TKjpaqH7l6CM1V0kAc7xw9QeJMD9pB62i1YhjxWPUnGHikH5wzHz0dCl3FE067lfxnb5wScKtOM1XOdsb4BTtV2Vif9TNSS2tjpfuZr62PRQZkvXZmzU1wy4eZIqTMhf9FWcTEzWvsXv5nWOXg9BwOap3HVI0tA/xflZtbSJxMA6Vp3OyKmnv+8H4DhDRC3coRSUiPiScRE9gv2n9WjI4UJg++5vrWwgOOcirLY1LaknPqPo";
const char strIcon3[] PROGMEM = "laWV14CRyOoqR7qWpPMS+SNcAEk6D3b0d2SCK+ygFuVaga1XBKJAsRNSG2WF0kLHqf6W1iknZ50EPy9VvhcX5kt8DSDjnFvM3T5NGMAWKIakAAAACYktHRAD/h4/MvwAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+MGHREMLhywMuoAAAQLSURBVEjHdZVdTFRHFMf/Z+7c3WU/Li6sLCJS2JWCC1aXIkJMQZrBJrYxtiQ1fbTxoyFpX/pQCSRtUotPfepDkz40bUz6qDWhb7QqxY+CssQVK/IRkY9qYMHyId0Pdvpwl4Vl7z3zMGfm/Obck3PmzCXBQJAA6M7r0XJadgzXRgAgioGK9f3JRlktazCGYfaHu/vg/JC2vE9a1ZmGcQAEQJJQQEDfyehFVAAAQNe0jtoRpGUsZ/Z4ok02Ik5/ycNQAYCG1c63unUHDOx6Z/JLbJVXtuNHbmXs4ObpxA+ZO8pXR7+BhGA5H5DcPthsqSa41+usc9ZpFYL7XDSRTVlahQKhUDjbRJK3l7hdVY4GtY1dpQh7aMSwEcHIWb76BEayWFhe9a+uPsif60eJEWQPsGgRjMU99/mGypjxcSC2l1HUxAHWP33k0bX5d80YWma5jxE3sTpftOuKfN/MQe7f7MAy7ph4v43RlAOrCfHrgTns9FLcIL/T1hbBnfX8nHreWS+4rYWmsqjV3IBQwE6T5GfYRObxvDJXDQ2m14OOoMe3zcWM";
const char strIcon4[] PROGMEM = "o0lwoYAu0w3BfS4Ks8kNo1W4amglA1/RamzvpFcL/EKZJrjgTRwUsp4SXHBa8HoLCtUzrJv1CL759c0oBGd9JGmFfV2cX+wWXHDBHQGipbLdvrV7/qWut09tJOdu7aqe2Gm1AzLehWIAsNfH6pL+nZeqI0B/pWOuKgJcb+U06FsDErlKz2Z2/3tDn9WOxl+AXor/DADxg03fp++IaykHEUA2MnqUqtTQlvJQSpFbV3JLAePB1O5+TjcAIOdZzAMAw54XF/i4dSChY129RBS7mIontOkgIey/A+M5MgjbPj0de9y785UuWiHJ+gRnoayq3xeceli35WzBrsJCigguuO0jCqFJEVzwUk3toIUN2HbMEcwqY9Dakl5Nsod7NcHZTboMoQhub6KZDHzK49Nq2GYp72vBPB+bzmAmLOdI8o8hFC1Aq1kBT9laBHcctnyinrfXCW49tu24PmKeAgjGrhm9NiRZj54d9TPqMyF6hcLueeQJk15LvRRJP44YE7I+7GKvqsx6nV3RZ+8lrJgg6stKJp1mDty/6XNgXvnOjJEWpo6a2CYpffW832LRGLLMQjA2YpiiMEXYFUubo0GrLtrB2w2TGBYKBLO2GhjHy1yC76iwH3IeKvQK7s+l2WxKPSEUxcdeG3m2LpszQ+NfBAeAosU9z4ufF6wB7vg//YkP9f9iukqdzT9BKj4GKv1zelBWo0DvILqNouTJp0enVhNP8xIbeMnU4tVoESpTh";
const char strIcon5[] PROGMEM = "5/ws80/AgAJBr0x5V1/fBfF8h4HloY8L99LNstq+ClEw6zX8uDNEQsAIJS/HJBOy1jDKAAJAv4HUgoJVbOQaSYAAAAASUVORK5CYII=";
const char *const arrIconBase64[] PROGMEM = {strIcon0, strIcon1, strIcon2, strIcon3, strIcon4, strIcon5};

// VMC modes
enum VmcModes { VMC_DISABLED, VMC_LOW, VMC_HIGH, VMC_AUTO };

// VMC commands
enum VmcCommands { CMND_VMC_MODE, CMND_VMC_TARGET, CMND_VMC_THRESHOLD };
const char kVmcCommands[] PROGMEM = D_CMND_VMC_MODE "|" D_CMND_VMC_TARGET "|" D_CMND_VMC_THRESHOLD;

/*********************************************************************************************/

// get VMC label according to state
char* VmcGetStateLabel (uint8_t state)
{
  char* label = NULL;
    
  // get label
  switch (state)
  {
   case VMC_DISABLED:          // Disabled
     label = D_VMC_DISABLED;
     break;
   case VMC_LOW:               // Forced Low speed
     label = D_VMC_LOW;
     break;
   case VMC_HIGH:              // Forced High speed
     label = D_VMC_HIGH;
     break;
   case VMC_AUTO:              // Automatic mode
     label = D_VMC_AUTO;
     break;
  }
  
  return label;
}

// get VMC state from relays state
uint8_t VmcGetRelayState ()
{
  uint8_t relay1 = 0;
  uint8_t state  = 0;
  
  // read relay states
  relay1 = bitRead (power, 0);

  // convert to pilotwire state
  if (relay1 == 0 ) state = VMC_LOW;
  else state = VMC_HIGH;
  
  return state;
}

// set relays state
void VmcSetRelayState (uint8_t new_state)
{
  // set relays
  switch (new_state)
  {
    case VMC_LOW:  // VMC low speed
      if (devices_present == 2) ExecuteCommandPower (2, POWER_ON, SRC_IGNORE);
      ExecuteCommandPower (1, POWER_OFF, SRC_IGNORE);
      break;
    case VMC_HIGH:  // VMC high speed
      if (devices_present == 2) ExecuteCommandPower (2, POWER_OFF, SRC_IGNORE);
      ExecuteCommandPower (1, POWER_ON, SRC_IGNORE);
      break;
  }
}

// get vmc actual mode
uint8_t VmcGetMode ()
{
  uint8_t actual_mode;

  // read actual VMC mode
  actual_mode = (uint8_t) Settings.weight_reference;

  // if outvalue, set to disabled
  if (actual_mode > VMC_AUTO) actual_mode = VMC_DISABLED;

  return actual_mode;
}

// set vmc mode
void VmcSetMode (uint8_t new_mode)
{
  // if within range, set mode
  if (new_mode <= VMC_AUTO) Settings.weight_reference = (unsigned long) new_mode;
}

// get current humidity level
float VmcGetHumidity ()
{
  float humidity;
  
  // get global humidity level
  humidity = global_humidity;

#ifdef USE_DHT
  // if global humidity not defined and dht sensor present, read it 
  if ((humidity == 0) && (Dht[0].h != 0)) humidity = Dht[0].h;
#endif

  return humidity;
}

// set target humidity
void VmcSetTargetHumidity (float new_target)
{
  // save target humidity level
  if ((new_target > 0) && (new_target < 100)) Settings.weight_max = (uint16_t) (new_target);
}

// get target humidity
float VmcGetTargetHumidity ()
{
  float humidity;

  // get target temperature
  humidity = (float) Settings.weight_max;

  // check if within range
  if (humidity < 0)   humidity = 0;
  if (humidity > 100) humidity = 100;
  
  return humidity;
}

// set vmc humidity threshold
void VmcSetThreshold (float threshold)
{
  // if within range, save threshold
  if ((threshold >= 0) && (threshold <= 10)) Settings.weight_calibration = (unsigned long) threshold;
}

// get vmc humidity threshold
float VmcGetThreshold ()
{
  float threshold;

  // get humidity threshold
  threshold = (float) Settings.weight_calibration;
  
  // check if within range
  if (threshold < 0)  threshold = 0;
  if (threshold > 10) threshold = 10;

  return threshold;
}

// Show JSON status (for MQTT)
void VmcShowJSON (bool append)
{
  float   actual_humidity;
  float   target_humidity;
  float   target_threshold;
  uint8_t actual_state;
  uint8_t actual_mode;
  char*   actual_label;

  // start message  -->  {  or  ,
  if (append == false) snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("{"));
  else snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,"), mqtt_data);

  // get mode and humidity
  actual_mode  = VmcGetMode ();
  actual_label = VmcGetStateLabel (actual_mode);
  actual_humidity  = VmcGetHumidity ();
  target_humidity  = VmcGetTargetHumidity ();
  target_threshold = VmcGetThreshold ();

  // vmc mode  -->  "VMC":{"Relay":2,"Mode":4,"Label":"Automatic","Humidity":70,"Target":50}
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_VMC "\":{"), mqtt_data);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_VMC_RELAY "\":%d"), mqtt_data, devices_present);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_VMC_MODE "\":%d"), mqtt_data, actual_mode);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_VMC_LABEL "\":\"%s\""), mqtt_data, actual_label);

  if (actual_humidity != 0) snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_VMC_HUMIDITY "\":%.1f"), mqtt_data, actual_humidity);

  if (actual_mode == VMC_AUTO)
  {
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_VMC_TARGET "\":%.1f"), mqtt_data, target_humidity);
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_VMC_THRESHOLD "\":%.1f"), mqtt_data, target_threshold);
  }
  
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);

  // if VMC mode is enabled
  if (actual_mode != VMC_DISABLED)
  {
    // relay state  -->  ,"Relay":{"Mode":2,"Label":"On","Number":number}
    actual_state = VmcGetRelayState ();
    actual_label = VmcGetStateLabel (actual_state);
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_VMC_STATE "\":{"), mqtt_data);
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_VMC_MODE "\":%d"), mqtt_data, actual_state);
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_VMC_LABEL "\":\"%s\""), mqtt_data, actual_label);
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);
  }
  
  // if not in append mode, publish message 
  if (append == false)
  { 
    // end of message   ->  }
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);

    // publish full sensor state
    MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
  }
}

// Handle VMC MQTT commands
bool VmcCommand ()
{
  bool serviced = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kVmcCommands);

  // handle command
  switch (command_code)
  {
    case CMND_VMC_MODE:        // set mode
      VmcSetMode (XdrvMailbox.payload);
      break;
    case CMND_VMC_TARGET:     // set target humidity 
      VmcSetTargetHumidity (atof (XdrvMailbox.data));
      break;
    case CMND_VMC_THRESHOLD:  // set humidity threshold 
      VmcSetThreshold (atof (XdrvMailbox.data));
      break;
    default:
      serviced = false;
      break;
  }

  // if command processed, update JSON
  if (serviced == true) VmcShowJSON (false);

  return serviced;
}

// update VMC relay states according to current status
void VmcEvery250MSecond ()
{
  float   actual_humidity;
  float   target_humidity;
  float   target_threshold;
  uint8_t actual_state;
  uint8_t target_mode;
  uint8_t target_state;

  // if VMC mode is not disabled
  target_mode = VmcGetMode ();
  if (target_mode != VMC_DISABLED)
  {
    // get VMC status
    actual_state = VmcGetRelayState ();
    
    // if VMC mode is automatic
    if (target_mode == VMC_AUTO)
    {
      // get current and target humidity
      actual_humidity  = VmcGetHumidity ();
      target_humidity  = VmcGetTargetHumidity ();
      target_threshold = VmcGetThreshold ();

      // if humidity is low enough, target VMC state is low speed
      if (actual_humidity < (target_humidity - target_threshold)) target_state = VMC_LOW;
      
      // else, if humidity is too high, target VMC state is high speed
      else if (actual_humidity > (target_humidity + target_threshold)) target_state = VMC_HIGH;

      // else, keep current state
      else target_state = actual_state;
    }

    // else, set target mode
    else target_state = target_mode;

    // if VMC state is different than target state, change state
    if (actual_state != target_state)
    {
      // set relays
      VmcSetRelayState (target_state);

      // publish new state
      VmcShowJSON (false);
    }
  }
}

#ifdef USE_WEBSERVER

// VMC icon
void VmcWebDisplayIcon (uint8_t height)
{
  uint8_t nbrItem, index;

  // display img according to height
  WSContentSend_P ("<img height=%d src='data:image/png;base64,", height);

  // loop to display base64 segments
  nbrItem = sizeof (arrIconBase64) / sizeof (char*);
  for (index=0; index<nbrItem; index++) { WSContentSend_P (arrIconBase64[index]); }

  WSContentSend_P ("'/>");
}

// VMC mode select combo
void VmcWebSelectMode (bool autosubmit)
{
  uint8_t actual_mode;
  float   actual_humidity;
  char    argument[VMC_LABEL_BUFFER_SIZE];

  // get mode and humidity
  actual_mode     = VmcGetMode ();
  actual_humidity = VmcGetHumidity ();

  // selection : beginning
  WSContentSend_P (PSTR ("<select name='%s'"), D_CMND_VMC_MODE);
  if (autosubmit == true) WSContentSend_P (PSTR (" onchange='this.form.submit()'"));
  WSContentSend_P (PSTR (">"));
  
  // selection : disabled
  if (actual_mode == VMC_DISABLED) strcpy (argument, "selected"); 
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), VMC_DISABLED, argument, D_VMC_DISABLED);

  // selection : low speed
  if (actual_mode == VMC_LOW) strcpy (argument, "selected");
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), VMC_LOW, argument, D_VMC_LOW);

  // selection : high speed
  if (actual_mode == VMC_HIGH) strcpy (argument, "selected");
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), VMC_HIGH, argument, D_VMC_HIGH);

  // selection : automatic
  if (actual_humidity != 0) 
  {
    if (actual_mode == VMC_AUTO) strcpy (argument, "selected");
    else strcpy (argument, "");
    WSContentSend_P (PSTR("<option value='%d' %s>%s</option>"), VMC_AUTO, argument, D_VMC_AUTO);
  }

  // selection : end
  WSContentSend_P (PSTR ("</select>"));
}

// VMC configuration button
void VmcWebConfigButton ()
{
  // beginning
  WSContentSend_P (PSTR ("<table style='width:100%%;'><tr>"));

  // vmc icon
  WSContentSend_P (PSTR ("<td align='center'>"));
  VmcWebDisplayIcon (32);
  WSContentSend_P (PSTR ("</td>"));

  // button
  WSContentSend_P (PSTR ("<td><form action='%s' method='get'><button>%s</button></form></td>"), D_PAGE_VMC, D_VMC_CONFIGURE);

  // end
  WSContentSend_P (PSTR ("</tr></table>"));
}

// append VMC buttons to main page
void VmcWebMainButton ()
{
  // beginning
  WSContentSend_P (PSTR ("<table style='width:100%%;padding:5px 10px;'><tr>"));

  // vmc icon
  WSContentSend_P (PSTR ("<td>"));
  VmcWebDisplayIcon (32);
  WSContentSend_P (PSTR ("</td>"));

  // select mode
  WSContentSend_P (PSTR ("<td><form action='%s' method='get'>"), D_PAGE_VMC);
  VmcWebSelectMode (true);
  WSContentSend_P (PSTR ("</form></td>"));

  // end
  WSContentSend_P (PSTR ("</tr></table>"));
}

// VMC web page
void VmcWebPage ()
{
  bool    updated = false;
  uint8_t target_mode;
  float   threshold;
  float   actual_humidity;
  float   target_humidity;
  char    argument[VMC_LABEL_BUFFER_SIZE];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // get VMC mode according to MODE parameter
  if (WebServer->hasArg(D_CMND_VMC_MODE))
  {
    WebGetArg (D_CMND_VMC_MODE, argument, VMC_LABEL_BUFFER_SIZE);
    VmcSetMode ((uint8_t)atoi (argument)); 
    updated = true;
  }

  // get VMC target humidity according to TARGET parameter
  if (WebServer->hasArg(D_CMND_VMC_TARGET))
  {
    WebGetArg (D_CMND_VMC_TARGET, argument, VMC_LABEL_BUFFER_SIZE);
    VmcSetTargetHumidity (atof (argument));
    updated = true;
  }

  // get VMC humidity threshold according to THRESHOLD parameter
  if (WebServer->hasArg(D_CMND_VMC_THRESHOLD))
  {
    WebGetArg (D_CMND_VMC_THRESHOLD, argument, VMC_LABEL_BUFFER_SIZE);
    VmcSetThreshold (atof (argument));
    updated = true;
  }

  // if parameters updated, back to main page
  if (updated == true)
  {
    WebServer->sendHeader ("Location", "/", true);
    WebServer->send ( 302, "text/plain", "");
  }
  
  // get humidity and target mode
  actual_humidity = VmcGetHumidity ();
  target_mode = VmcGetMode ();

  // beginning of form
  WSContentStart_P (D_VMC_CONFIGURE);
  WSContentSendStyle ();

  // form
  WSContentSend_P (PSTR ("<fieldset><legend><b>&nbsp;%s&nbsp;</b></legend><form method='get' action='%s'>"), D_VMC_PARAMETERS, D_PAGE_VMC);

  // selection : beginning
  WSContentSend_P (PSTR ("<p><b>%s</b><br/>"), D_VMC_MODE);

  // select mode
  VmcWebSelectMode (false);

  // selection : end
  WSContentSend_P (PSTR ("</p>"));

  // if temperature sensor is present
  if (actual_humidity != 0) 
  {
    // read target and drift temperature
    target_humidity = VmcGetTargetHumidity ();
    threshold = VmcGetThreshold ();

    // target humidity label and input
    WSContentSend_P (PSTR ("<p><b>%s</b><br/><input type='number' name='%s' min='0' max='100' step='1' value='%.1f'></p>"), D_VMC_TARGET, D_CMND_VMC_TARGET, target_humidity);

    // humidity threshold label and input
    WSContentSend_P (PSTR ("<p><b>%s</b><br/><input type='number' name='%s' min='0' max='10' step='1' value='%.1f'></p>"), D_VMC_THRESHOLD, D_CMND_VMC_THRESHOLD, threshold);
  }

  // end of form
  WSContentSend_P(HTTP_FORM_END);
  WSContentSpaceButton(BUTTON_CONFIGURATION);
  WSContentStop();
}

// append VMC state to main page
bool VmcWebState ()
{
  float   target_humidity;
  uint8_t actual_mode;
  uint8_t actual_state;
  char*   actual_label;
  char    state_color[VMC_COLOR_BUFFER_SIZE];

  // get mode
  actual_mode  = VmcGetMode ();
  actual_label = VmcGetStateLabel (actual_mode);
  
  // if VMC is in automatic mode, display target humidity
  if (actual_mode == VMC_AUTO)
  {
    // read humidity
    target_humidity = VmcGetTargetHumidity ();

    // add it to JSON
    snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><th>%s</th><td>%.1f%%</td></tr>", mqtt_data, D_VMC_TARGET, target_humidity);
  }

  // if VMC mode is enabled
  if (actual_mode != VMC_DISABLED)
  {
    // get state and label
    actual_state = VmcGetRelayState ();
    actual_label = VmcGetStateLabel (actual_state);
    
    // set color according to state
    switch (actual_state)
    {
      case VMC_HIGH:
        strcpy (state_color, "green");
        break;
      case VMC_LOW:
        strcpy (state_color, "black");
        break;
      default:
        strcpy (state_color, "black");
    }
 
    // display state
    snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><tr><td><b>%s</b></td><td style='font-weight:bold; color:%s;'>%s</td></tr>", mqtt_data, D_VMC_STATE, state_color, actual_label);
  }
}

#endif  // USE_WEBSERVER

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns98 (byte callback_id)
{
  bool result = false;

  // main callback switch
  switch (callback_id)
  {
    case FUNC_COMMAND:
      result = VmcCommand ();
      break;
    case FUNC_EVERY_250_MSECOND:
      VmcEvery250MSecond ();
      break;
    case FUNC_JSON_APPEND:
      VmcShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_VMC, VmcWebPage);
      break;
    case FUNC_WEB_APPEND:
      VmcWebState ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      VmcWebMainButton ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      VmcWebConfigButton ();
      break;
#endif  // USE_WEBSERVER

  }
  return result;
}

#endif // USE_VMC
