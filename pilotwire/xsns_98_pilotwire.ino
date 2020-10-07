/*
  xsns_98_pilotwire.ino - French Pilot Wire (Fil Pilote) support (~27.5 kb)
  for Sonoff Basic or Sonoff Dual R2

  Copyright (C) 2019/2020  Theo Arends, Nicolas Bernaerts
    05/04/2019 - v1.0   - Creation
    12/04/2019 - v1.1   - Save settings in Settings.weight... variables
    10/06/2019 - v2.0   - Complete rewrite to add web management
    25/06/2019 - v2.1   - Add DHT temperature sensor and settings validity control
    05/07/2019 - v2.2   - Add embeded icon
    05/07/2019 - v3.0   - Add max power management with automatic offload
                          Save power settings in Settings.energy... variables
    12/12/2019 - v3.1   - Add public configuration page http://.../control
    30/12/2019 - v4.0   - Functions rewrite for Tasmota 8.x compatibility
    06/01/2019 - v4.1   - Handle offloading with finite state machine
    09/01/2019 - v4.2   - Separation between Offloading driver and Pilotwire sensor
    15/01/2020 - v5.0   - Separate temperature driver and add remote MQTT sensor
    05/02/2020 - v5.1   - Block relay command if not coming from a mode set
    21/02/2020 - v5.2   - Add daily temperature graph
    24/02/2020 - v5.3   - Add control button to main page
    27/02/2020 - v5.4   - Add target temperature and relay state to temperature graph
    01/03/2020 - v5.5   - Add timer management with Outside mode
    13/03/2020 - v5.6   - Add time to graph
    05/04/2020 - v5.7   - Add timezone management
    18/04/2020 - v6.0   - Handle direct connexion of heater in addition to pilotwire
    22/08/2020 - v6.1   - Handle out of range values during first flash
    24/08/2020 - v6.5   - Add status icon to Web UI 
    12/09/2020 - v6.6   - Add offload icon status 

  Settings are stored using weighting scale parameters :
    - Settings.weight_reference             = Fil pilote mode
    - Settings.weight_max                   = Target temperature  (x10 -> 192 = 19.2°C)
    - Settings.weight_calibration           = Temperature correction (0 = -5°C, 50 = 0°C, 100 = +5°C)
    - Settings.weight_item                  = Minimum temperature (x10 -> 125 = 12.5°C)
    - Settings.energy_frequency_calibration = Maximum temperature (x10 -> 240 = 24.0°C)
    - Settings.energy_voltage_calibration   = Outside mode temperature (x10 -> 25 = 2.5°C)
    - Settings.energy_current_calibration   = Device type (pilotewire or direct)

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

#ifdef USE_PILOTWIRE

#define XSNS_98                         98

/*************************************************\
 *               Constants
\*************************************************/

#define PILOTWIRE_BUFFER_SIZE           128

#define D_PAGE_PILOTWIRE_CONFIG         "config"
#define D_PAGE_PILOTWIRE_CONTROL        "control"
#define D_PAGE_PILOTWIRE_SWITCH         "mode"
#define D_PAGE_PILOTWIRE_ICON           "icon"

#define D_CMND_PILOTWIRE_ON             "on"
#define D_CMND_PILOTWIRE_OFF            "off"
#define D_CMND_PILOTWIRE_SET            "set"
#define D_CMND_PILOTWIRE_MODE           "mode"
#define D_CMND_PILOTWIRE_LANG           "lang"
#define D_CMND_PILOTWIRE_OFFLOAD        "offload"
#define D_CMND_PILOTWIRE_MIN            "min"
#define D_CMND_PILOTWIRE_MAX            "max"
#define D_CMND_PILOTWIRE_TARGET         "target"
#define D_CMND_PILOTWIRE_OUTSIDE        "outside"
#define D_CMND_PILOTWIRE_DRIFT          "drift"
#define D_CMND_PILOTWIRE_PULLUP         "pullup"
#define D_CMND_PILOTWIRE_DEVICE         "device"

#define D_JSON_PILOTWIRE                "Pilotwire"
#define D_JSON_PILOTWIRE_MODE           "Mode"
#define D_JSON_PILOTWIRE_OUTSIDE        "Outside"
#define D_JSON_PILOTWIRE_LABEL          "Label"
#define D_JSON_PILOTWIRE_MIN            "Min"
#define D_JSON_PILOTWIRE_MAX            "Max"
#define D_JSON_PILOTWIRE_TARGET         "Target"
#define D_JSON_PILOTWIRE_DRIFT          "Drift"
#define D_JSON_PILOTWIRE_TEMPERATURE    "Temperature"
#define D_JSON_PILOTWIRE_RELAY          "Relay"
#define D_JSON_PILOTWIRE_STATE          "State"

#define D_PILOTWIRE                     "Pilotwire"
#define D_PILOTWIRE_HEATER              "Heater"
#define D_PILOTWIRE_STATE               "State"
#define D_PILOTWIRE_CONFIGURE           "Configure"
#define D_PILOTWIRE_CONNEXION           "Connexion"
#define D_PILOTWIRE_MODE                "Mode"
#define D_PILOTWIRE_NORMAL              "Normal"
#define D_PILOTWIRE_OUTSIDE             "Outside"
#define D_PILOTWIRE_DISABLED            "Disabled"
#define D_PILOTWIRE_OFF                 "Off"
#define D_PILOTWIRE_COMFORT             "Comfort"
#define D_PILOTWIRE_ECO                 "Economy"
#define D_PILOTWIRE_FROST               "No Frost"
#define D_PILOTWIRE_THERMOSTAT          "Thermostat"
#define D_PILOTWIRE_TARGET              "Target"
#define D_PILOTWIRE_DROPDOWN            "Dropdown"
#define D_PILOTWIRE_DRIFT               "Sensor correction"
#define D_PILOTWIRE_SETTING             "Public settings"
#define D_PILOTWIRE_MIN                 "Minimum"
#define D_PILOTWIRE_MAX                 "Maximum"
#define D_PILOTWIRE_NOSENSOR            "No temperature sensor available"
#define D_PILOTWIRE_CONTROL             "Control"
#define D_PILOTWIRE_TEMPERATURE         "Temperature"
#define D_PILOTWIRE_RANGE               "Range"
#define D_PILOTWIRE_LOCAL               "Local"
#define D_PILOTWIRE_REMOTE              "Remote"
#define D_PILOTWIRE_DIRECT              "Direct supply"
#define D_PILOTWIRE_PULLUP              "Enable DS18B20 internal pullup"
#define D_PILOTWIRE_ENGLISH             "en"
#define D_PILOTWIRE_FRENCH              "fr"
#define D_PILOTWIRE_CHECKED             "checked"

#define PILOTWIRE_TEMP_MIN              2
#define PILOTWIRE_TEMP_MAX              50
#define PILOTWIRE_TEMP_DRIFT            5
#define PILOTWIRE_TEMP_DROP             10
#define PILOTWIRE_TEMP_DEFAULT_TARGET   18
#define PILOTWIRE_TEMP_DEFAULT_MIN      15
#define PILOTWIRE_TEMP_DEFAULT_MAX      25
#define PILOTWIRE_TEMP_DEFAULT_DRIFT    0
#define PILOTWIRE_TEMP_DEFAULT_DROP     2
#define PILOTWIRE_TEMP_STEP             0.5
#define PILOTWIRE_TEMP_DRIFT_STEP       0.1

#define PILOTWIRE_TEMP_THRESHOLD        0.25
        
#define PILOTWIRE_GRAPH_REFRESH         300         // collect temperature every 5mn
#define PILOTWIRE_GRAPH_SAMPLE          288         // 24 hours display with collect every 5 mn
#define PILOTWIRE_GRAPH_WIDTH           800      
#define PILOTWIRE_GRAPH_HEIGHT          500 
#define PILOTWIRE_GRAPH_PERCENT_START   15      
#define PILOTWIRE_GRAPH_PERCENT_STOP    85

// constant chains
const char str_conf_fieldset_start[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char str_conf_fieldset_stop[] PROGMEM = "</fieldset></p>\n";
const char str_conf_topic_style[] PROGMEM = "style='float:right;font-size:0.7rem;'";
const char str_conf_mode_select[] PROGMEM = "<input type='radio' name='%s' id='%d' value='%d' %s>%s<br>\n";
const char str_conf_button[] PROGMEM = "<p><form action='%s' method='get'><button>%s</button></form></p>\n";
const char str_conf_temperature[] PROGMEM = "<p>%s (°%s)<span %s>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%s' value='%s'></p>\n";
const char str_graph_separation[] PROGMEM = "<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n";
const char str_graph_temperature[] PROGMEM = "<text class='temperature' x='%d%%' y='%d%%'>%s°C</text>\n";

// control page texts
enum PilotwireLangages { PILOTWIRE_LANGAGE_ENGLISH, PILOTWIRE_LANGAGE_FRENCH };
const char *const arrControlLang[]    PROGMEM = {"en", "fr"};
const char *const arrControlLangage[] PROGMEM = {"English", "Français"};
const char *const arrControlTemp[]    PROGMEM = {"Target", "Thermostat"};
const char *const arrControlSet[]     PROGMEM = {"Set to", "Définir à"};

// enumarations
enum PilotwireDevices { PILOTWIRE_DEVICE_NORMAL, PILOTWIRE_DEVICE_DIRECT };
enum PilotwireModes { PILOTWIRE_DISABLED, PILOTWIRE_OFF, PILOTWIRE_COMFORT, PILOTWIRE_ECO, PILOTWIRE_FROST, PILOTWIRE_THERMOSTAT, PILOTWIRE_OFFLOAD };
enum PilotwireSources { PILOTWIRE_SOURCE_NONE, PILOTWIRE_SOURCE_LOCAL, PILOTWIRE_SOURCE_REMOTE };

// fil pilote commands
enum PilotwireCommands { CMND_PILOTWIRE_MODE, CMND_PILOTWIRE_MIN, CMND_PILOTWIRE_MAX, CMND_PILOTWIRE_TARGET, CMND_PILOTWIRE_OUTSIDE, CMND_PILOTWIRE_DRIFT};
const char kPilotWireCommands[] PROGMEM = D_CMND_PILOTWIRE_MODE "|" D_CMND_PILOTWIRE_MIN "|" D_CMND_PILOTWIRE_MAX "|" D_CMND_PILOTWIRE_TARGET "|" D_CMND_PILOTWIRE_OUTSIDE "|" D_CMND_PILOTWIRE_DRIFT;

/****************************************\
 *               Icons
\****************************************/

// icon : off
const char icon_off_0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAAEAAAABAAQMAAACQp+OdAAAC6HpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHja7ZddjuwoDIXfWcUsAdsYm+UQCNLdwSx/DoROVVf3HalH92EeKij8OMQ25yOoKpx//xrhL1xUJIWk5rnkHHGlkgpXdDxe19VSTKteV9qPMP5kD/cDhknQyjXM555fYdfHC7ZfoOOzPVjbfnw72g8+HMqMzOj0neR2JHzZaY9D4atT89Ny9j0al2nS43r0Ok4GMbrCn3DgU0jiqvmKJNddcTtqljQnokyLohaxr/qFW7pvBLx7L/rFtu3ykONy9LGs/KLTtpN+r99S6Tkj4jsyP2dkeof4qt/oPsZ5ra6mHCBX3ov6WMrqYSIkTbJeyyiGW9G3VQqKxxobqHUs9QjxwKAQQ/FBiTpVGnSutlFDiolPNrTMjWXZXIwLN0hPkmahwRakSAcdlgZyAjPfudCKW2Y8BHNE7oSZTHBGi+NTCa+G/1o+ORpjbnOi6LdWyIvn/kIak9ysMQtAaGxNdelL4Wri6zXBCgjqktmxwBqPy8Wh9NhbsjhL1ICpaX/SZH07gESIrUiGBARiJlHKFI3ZiKCjg09F5nPbHyBAGpQ7suQkkgHHecbGO0ZrLitfZhwvAKGSxYCmSAWslDRlfG+OLVSDiiZVzWrqWrRmySlrztnyPKeqiSVTy2bmVqy6eHL17ObuxWvhIjjGNJRcrHgppVYEranCV8X8CsPBhxzp0CMfdvhRjtqwfVpq2nKz5q202rlLxxEQeu7WvZdeTzqxlc506plPO/0sZx3Ya0NGGjrysOG";
const char icon_off_1[] PROGMEM = "jjHpT21Q/U3sl9+/UaFPjBWrOswc1mM0+XNA8TnQyAzFOBOI2CWBD82QWnVLiSW4yi4UliCgjS51wOk1iIJhOYh10s3uQ+y23AHV/yo2/Ixcmuj9BLkx0T+S+cvuGWq/ruJUFaH6F0BQnpODzgwOv7HWUIsMgFc9h/EEbfvrC29Hb0dvR29Hb0dvR29H/35HgBwT+Q4Z/AHNvk7HvJl4QAAABhWlDQ1BJQ0MgcHJvZmlsZQAAeJx9kT1Iw1AUhU9TpUUqDnYQcchQBcGCqIijVqEIFUKt0KqDyUv/oElDkuLiKLgWHPxZrDq4OOvq4CoIgj8gTo5Oii5S4n1JoUWMFx7v47x7Du/dBwiNCtOsrnFA020znUyI2dyqGHpFGAOIIoBRmVnGnCSl4Ftf99RHdRfnWf59f1avmrcYEBCJZ5lh2sQbxNObtsF5nzjKSrJKfE48ZtIFiR+5rnj8xrnossAzo2YmPU8cJRaLHax0MCuZGvEUcUzVdMoXsh6rnLc4a5Uaa92TvzCS11eWuU5rCEksYgkSRCiooYwKbMRp10mxkKbzhI9/0PVL5FLIVQYjxwKq0CC7fvA/+D1bqzA54SVFEkD3i+N8DAOhXaBZd5zvY8dpngDBZ+BKb/urDWDmk/R6W4sdAX3bwMV1W1P2gMsdYODJkE3ZlYK0hEIBeD+jb8oB/bdAz5o3t9Y5Th+ADM0qdQMcHAIjRcpe93l3uHNu//a05vcDVSdymy0nI4AAAAAGUExURQAAAP8AABv/jSIAAAABdFJOUwBA5thmAAAAAWJLR0QAiAUdSAAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAA";
const char icon_off_2[] PROGMEM = "d0SU1FB+QIGBQFJcmA7CsAAAC9SURBVCjPrdHBEYUgDATQMBw8pgRKoTQpjVIogSMHx7ibOH4P/ygjM0/BGFYRkdwlxjZu6PwU2nQm1i5D57aAShzATpxvGGEiyTrRAhnARB3rgmVUtiF4AdiH6EHU+cIiCq7AotWf/zB4ZDRUB0NAQ9UT+BAl4B9lEv+gTMS7esBoeApPhAd8kI1ACEyE2fiMO0NF5bqhUCH2M6ZU69lYtRgGv6MEW9gItpmI5uUx/M9V85d8d/zLdG/BGlcueHqo1kY2N+wAAAAASUVORK5CYII=";

// icon : comfort
const char icon_comfort_0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAAEAAAABAAQMAAACQp+OdAAAC6HpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHja7ZddstsgDIXfWUWXgCSEYDn8mJnuoMvvARMnNzft9Hb60IeYsQGBJXE+zCTu+PF9uG+4KEtwQS3FHKPHFXLIXNBI/rzOmnxYz3WFPYT+B7u7BhgmQS1nNx57foFd7y/YfoHqR7uztv2k7WgP3BzKjMxo9J3kdiR82mn3XeazUeLDcvY9Gudp0noOPfeDQYyu8Cfs+BASv558RpLzLrgTnixhTkQpa6SISPisn7ukeyHg1XrSz7dtl7scp6PbsuKTTttO+lq/pdJjRsRXZH7MyPQK8Vm/0dMYx7m6EqKDXHEv6raU1cJESBpkvRZRDLeibatklOSLb6DWsdTqfEUnE0PxQYE6FRp0rLpRQ4qBDzbUzI1l2ZIYZ26QniTMQoPNSZYOOiwN5MBD+MqFVtw84yFYQuROmMkEZ7Q4PhT3bPjb8sHRGHObE/l0aYW8eO4vpDHJzSdmAQiNrakufcmdlX++JlgBQV0yJyyw+Hq6qEr3vSWLs3h1mBr2J03WtwNIhNiKZEhAwEcSpUjemI0IOibwKch8bvsKAqROuSNLDiIRcBLP2HjHaM1l5dOM4wUgVKIY0GQpgBWChojvLWELFaeiQVWjmibNWqLEEDXGaHGeU8XEgqlFM0uWrSRJIWmKyVJKOZXMWXCMqcsxW04551IQtIQCXwXzCwyVq9RQtcZqNdVcS8P2aaFpi81aarmVzl06jgDXY7eeeu7loANb6QiHHvGwIx35KAN7bcgIQ0ccNtL";
const char icon_comfort_1[] PROGMEM = "Io1zUNtWP1J7J/Z4abWq8QM15dqcGs9nNBc3jRCczEONAIG6TADY0T2Y+UQg8yU1mPrM4EWVkqRNOp0kMBMNBrIMudndyv+TmoO5XufErcm6i+xfk3ET3QO4ztxfUelnHrSxA8yuEpjghBZ/fUCmcCtdBKr351cE5+Oe1++oLb0dvR29Hb0dvR29Hb0f/vyPBDwj8h3Q/AfkLk8YmXYvcAAABhWlDQ1BJQ0MgcHJvZmlsZQAAeJx9kT1Iw1AUhU9TpUUqDnYQcchQBcGCqIijVqEIFUKt0KqDyUv/oElDkuLiKLgWHPxZrDq4OOvq4CoIgj8gTo5Oii5S4n1JoUWMFx7v47x7Du/dBwiNCtOsrnFA020znUyI2dyqGHpFGAOIIoBRmVnGnCSl4Ftf99RHdRfnWf59f1avmrcYEBCJZ5lh2sQbxNObtsF5nzjKSrJKfE48ZtIFiR+5rnj8xrnossAzo2YmPU8cJRaLHax0MCuZGvEUcUzVdMoXsh6rnLc4a5Uaa92TvzCS11eWuU5rCEksYgkSRCiooYwKbMRp10mxkKbzhI9/0PVL5FLIVQYjxwKq0CC7fvA/+D1bqzA54SVFEkD3i+N8DAOhXaBZd5zvY8dpngDBZ+BKb/urDWDmk/R6W4sdAX3bwMV1W1P2gMsdYODJkE3ZlYK0hEIBeD+jb8oB/bdAz5o3t9Y5Th+ADM0qdQMcHAIjRcpe93l3uHNu//a05vcDVSdymy0nI4AAAAAGUExURQAAAP/lADj2iosAAAABdFJOUwBA5thmAAAAAWJLR0QAiAUdSAAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAA";
const char icon_comfort_2[] PROGMEM = "d0SU1FB+QIGBQCIhil708AAADESURBVCjPlZKxDQMhDEWNKCgZgVHYLDAao9wIlBQnfmx8+C5KkyCEnmTL/v6GiMhX0hPaBfH4FVwXSAxDIHO1U6BUclMAHAJX5ocI1Vc/GTgsiUSZW4bBkAS61K3x8FI7UDndUuQA7ekB1RoA1RoBFZSAviD3OBRaUCh1tWIgpzDXvaF0gXQycMKLAUMgzxssZMnfdR4trKnJyD2NT6km3saxAffIZoLZYkaZdaVdZm57zXC/V3AvZa/JFvfXup+fZH2bN12QjPdYccc/AAAAAElFTkSuQmCC";

// icon : economy
const char icon_eco_0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAAEAAAABAAQMAAACQp+OdAAAC53pUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHja7ZdbktwqDIbfWUWWgCSEYDmYS1V2kOXnB9Puy3RO1ZzKQx7alA3IIIn/w/SM679+DvcDF2UJLqilmGP0uEIOmQsayZ/XWZMP67musF+h/2R31wuGSVDL2Y19jy+w632C7Ql0PNud1e0nbUf7xc2hzMiMRttJbkfCp51232U+GyU+LGffo3KeJj3OV6/9YBCjKfwJO+5C4teTz0hy3gV3wpMlzIEo06Lrmb/q5y7p3gh4tV7083Xb5S7H6ei2rPii07aTvtdvqfSYEfEVmR8zMr1CfNVvtDRGP1dXQnSQK+5F3ZayWhgISYOsaRHFcCvatkpGSb74CmoNSz2cP9DJxFB8UKBGhQb1VVeqSDFwZ0PNXFmWLYlx5grRScIsNNicZGmgw1JBTmDmKxdacfOMh2AJkRthJBOc0eL4UNyr4f+WJ0djzG1O5NOlFfLiub+QxiQ3nxgFIDS2prr0JXdW/vWaYAUEdcmcsMDij9PFoXTfW7I4i1eHoWF/0mRtO4BEiK1IhgQEfCRRiuSN2YigYwKfgszntj9AgNQpN2TJQSQCTuIZG3OM1lhWPs04XgBCJYoBTZYCWCFoiPjeErZQcSoaVDWqadKsJUoMUWOMFuc5VUwsmFo0s2TZSpIUkqaYLKWUU8mcBceYuhyz5ZRzLgVBSyjwVTC+wHDwIUc49IiHHenIR6nYPjVUrbFaTTXX0rhJwxHgWmzWUsutdOrYSj107bFbTz33MrDXhowwdMRhI40";
const char icon_eco_1[] PROGMEM = "8ykVtU32m9kruv6nRpsYL1Bxnd2owm91c0DxOdDIDMQ4E4jYJYEPzZOYThcCT3GTmM4sTUUaWOuE0msRAMHRiHXSxu5P7IzcHdb/Ljd+RcxPd3yDnJroHcl+5vaHWyjpuZQGaXyE0xQkp+Px6Q3xOZSDEKN3PNn6gvlG77074OPo4+jj6OPo4+jj6OPr3HQn+gMA/fe43q6yUlabtF3AAAAGFaUNDUElDQyBwcm9maWxlAAB4nH2RPUjDUBSFT1OlRSoOdhBxyFAFwYKoiKNWoQgVQq3QqoPJS/+gSUOS4uIouBYc/FmsOrg46+rgKgiCPyBOjk6KLlLifUmhRYwXHu/jvHsO790HCI0K06yucUDTbTOdTIjZ3KoYekUYA4gigFGZWcacJKXgW1/31Ed1F+dZ/n1/Vq+atxgQEIlnmWHaxBvE05u2wXmfOMpKskp8Tjxm0gWJH7muePzGueiywDOjZiY9TxwlFosdrHQwK5ka8RRxTNV0yheyHquctzhrlRpr3ZO/MJLXV5a5TmsISSxiCRJEKKihjApsxGnXSbGQpvOEj3/Q9UvkUshVBiPHAqrQILt+8D/4PVurMDnhJUUSQPeL43wMA6FdoFl3nO9jx2meAMFn4Epv+6sNYOaT9Hpbix0BfdvAxXVbU/aAyx1g4MmQTdmVgrSEQgF4P6NvygH9t0DPmje31jlOH4AMzSp1AxwcAiNFyl73eXe4c27/9rTm9wNVJ3KbLScjgAAAAAZQTFRFAAAA/7UAgk3X3wAAAAF0Uk5TAEDm2GYAAAABYktHRACIBR1IAAAACXBIWXMAAC4jAAAuIwF4pT92AAAAB3";
const char icon_eco_2[] PROGMEM = "RJTUUH5AgYFAUJ+1iAyAAAAKBJREFUKM990sENhCAQBVAIm3i0BFuwgE0oTUqjFErw6MEwCzP/xwTMzsWXIDMf1LmX2omCpydCBlZiIyJxpBECeCJMWCasE7YXYGj8hzLhHHA8uIh7QJRKyIxEZKIgIVo33ETFubCtI9vZpc//KNpL33Y/rc5wGfqqN7SmhkLknhWIiqTDRFP2GTp3sTbOGmk2tLFtTFTxfZ4LycNPofUDV66sit3EmmYAAAAASUVORK5CYII=";

// icon : no frost
const char icon_frost_0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAYAAACqaXHeAAAC5npUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHja7ZdNktwgDIX3nCJHsCSE4Dj8VuUGOX4emHb39HRSmVQWWbQpG5CxJN4H9IzrP74P9w0XJfHOq8WQQjhw+eQTZzTicV5nTYdfz3X5/Qr9D3Z3vWCYBLWc3dD3+Ay73j+w/QGVj3ZndfuJ29F+cXMoMzKj0XaS25Hwaafdd4nPRg4P09m32HJxDX7ue4MYTWEUdtyF5FhPPiPJeWfcEU8WPweiTIsti3zWz13SvRDwaj3pd9Rtl7scp6PbtMKTTttO+lq/pdJjRsRXZH7MyPQK8Um/MVoco5+zyz44yBX2pG5TWS0MLJDzVCOgGG5F21ZJKPHIR4XwDVMt7ijoJGIoPshTo0yD+qorVaToubOhZq4syxbFOHGF9CR+FhpsTpI00GGpICcw85ULrbhpxkOwiMiNMJIJzmhxfCju2fC35YOjMeYyJzripRXy4rm+kMYkN58YBSA0tqa69CV3VsfzNcEKCOqSOWKC+Sini6J0X1uyOMuhDkP93tJkbTuARIitSIYEBI5AohToMGYjgo4RfDIyn8u+gACpU27Ikr1IAJzIMza+MVpjWfk043gBCJWATRIBKAOW9+oD9lvEEspORb2qBjWNmjQHCT5oCMHCPKeyiXlTC2YWLVmOEn3UGKLFGFPMiZPgGFOXQrIUU0o5I2j2Gb4yxmcYChcpvmgJxUosqeSK5VN91Rqq1VhTzY2bNBwBroVmLbbUcqeOpdR91x669dhTzwNrbcjwQ0cYNuJII1/";
const char icon_frost_1[] PROGMEM = "UNtWP1J7J/Z4abWq8QM1xdqcGs9nNBc3jRCczEGNPIG6TABY0T2ZHJO95kpvMjsTiRJSRpU44jSYxEPSdWAdd7O7kfsnNQd2vcuNX5NxE9y/IuYnugdxnbi+otbyOW1mA5i6EpjghBduvFK+E38qWgDXQ6sxfiz+v3Vc/eDt6O3o7ejt6O3o7ejv67x3ZwB8QCf9S/QSm6TClE701wgAAAYVpQ0NQSUNDIHByb2ZpbGUAAHicfZE9SMNQFIVPU6VFKg52EHHIUAXBgqiIo1ahCBVCrdCqg8lL/6BJQ5Li4ii4Fhz8Waw6uDjr6uAqCII/IE6OToouUuJ9SaFFjBce7+O8ew7v3QcIjQrTrK5xQNNtM51MiNncqhh6RRgDiCKAUZlZxpwkpeBbX/fUR3UX51n+fX9Wr5q3GBAQiWeZYdrEG8TTm7bBeZ84ykqySnxOPGbSBYkfua54/Ma56LLAM6NmJj1PHCUWix2sdDArmRrxFHFM1XTKF7Ieq5y3OGuVGmvdk78wktdXlrlOawhJLGIJEkQoqKGMCmzEaddJsZCm84SPf9D1S+RSyFUGI8cCqtAgu37wP/g9W6swOeElRRJA94vjfAwDoV2gWXec72PHaZ4AwWfgSm/7qw1g5pP0eluLHQF928DFdVtT9oDLHWDgyZBN2ZWCtIRCAXg/o2/KAf23QM+aN7fWOU4fgAzNKnUDHBwCI0XKXvd5d7hzbv/2tOb3A1UncpstJyOAAAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH5AgYFAcXM2HfKQAAAhFJREFUeNrtW0";
const char icon_frost_2[] PROGMEM = "GSxCAITFv+Z/+9v9sX9F63pjYGEIQkepqpiUq3CA2pwdf3z/Hm0RP35p/PyDKiHS8fm4BNwCbg3SMqC/DjO5LWKOMBDH6+HAGYAEXheuU94IwEDoAvBT9DACdI+G8+lfPdrkybAK8hAQbvQZA9aWkQzs+ViAF0JgHB+5cQQqhw8v8JIS40CIvL4VNsXTAprVZfIcCaMGWNTotFwEKoNzCKAbhgk8VP98pGSIohXJwsb+jusFSDKA7YRVN0xSJ8EnDvfsDKjOF6EN15s0giaHwWWgKodDUGE0HD/mIyugE8FDFihghpVIfCfn7a0h2DCgSpEw6uDoUYUscATJ6aVEPAGbiUDJUQ8khDUjFFR+BuQuhYQER6Y2TV22ENEbfqCN1+rPIAi4h5xBWgw9zl7wU86n5tXR7dhzhdY6QELT07DxFDJ4+gpxKUkOGVyyVEwOvKdYfiIkrESGsMGtYUK0EaXd4zcEmIMOuKrpicLWJCxFR3Nq6aqnQh4I5NUXHG6A8DriaiBae0I+FqqIRQU6guFAMPg40cESAFzqIeQSERlMYAPCAGXGaMngSYCnkbGh9asqumZ50WxawRJIP3T/UAOj+X5gEw1OWWd4wMssfFA2YbJNKOEKOvQlvs8qP8fPYbq12BGfDW0+TdCJh10WWapBcBnEbC/tPUJmATsAl49cj893iJfsMv2vSRgZoqMgoAAAAASUVORK5CYII=";

// icons associated to heater mode
const char* pilotwire_icon[][3] PROGMEM = { 
  {nullptr,        nullptr,        nullptr        },    // PILOTWIRE_DISABLED
  {icon_off_0,     icon_off_1,     icon_off_2     },    // PILOTWIRE_OFF
  {icon_comfort_0, icon_comfort_1, icon_comfort_2 },    // PILOTWIRE_COMFORT
  {icon_eco_0,     icon_eco_1,     icon_eco_2     },    // PILOTWIRE_ECO
  {icon_frost_0,   icon_frost_1,   icon_frost_2   },    // PILOTWIRE_FROST
  {nullptr,        nullptr,        nullptr        }     // PILOTWIRE_THERMOSTAT
};

/*************************************************\
 *               Variables
\*************************************************/

// variables
bool     pilotwire_outside_mode;                   // flag for outside mode (target temperature dropped)
bool     pilotwire_updated;                        // JSON needs update
uint8_t  pilotwire_devices_present;                // number of relays on the devices
uint8_t  pilotwire_temperature_source;             // temperature source (local or distant)
uint8_t  pilotwire_langage;                        // current langange for control page
float    pilotwire_temperature;                    // graph temperature
float    pilotwire_target;                         // graph target temperature
uint8_t  pilotwire_state;                          // graph pilotwire state
float    pilotwire_graph_temperature;              // graph temperature
float    pilotwire_graph_target;                   // graph target temperature
uint8_t  pilotwire_graph_state;                    // graph pilotwire state
uint32_t pilotwire_graph_index;                    // current graph index for data
uint32_t pilotwire_graph_counter;                  // graph update counter
float    arr_temperature[PILOTWIRE_GRAPH_SAMPLE];  // graph temperature array
float    arr_target[PILOTWIRE_GRAPH_SAMPLE];       // graph target temperature array
uint8_t  arr_state[PILOTWIRE_GRAPH_SAMPLE];        // graph command state array

/**************************************************\
 *                  Accessors
\**************************************************/

// get state label
String PilotwireGetStateLabel (uint8_t state)
{
  String str_label;

  // get label
  switch (state)
  {
   case PILOTWIRE_DISABLED:         // Disabled
     str_label = D_PILOTWIRE_DISABLED;
     break;
   case PILOTWIRE_OFF:              // Off
     str_label = D_PILOTWIRE_OFF;
     break;
   case PILOTWIRE_COMFORT:          // Comfort
     str_label = D_PILOTWIRE_COMFORT;
     break;
   case PILOTWIRE_ECO:              // Economy
     str_label = D_PILOTWIRE_ECO;
     break;
   case PILOTWIRE_FROST:            // No frost
     str_label = D_PILOTWIRE_FROST;
     break;
   case PILOTWIRE_THERMOSTAT:       // Thermostat
     str_label = D_PILOTWIRE_THERMOSTAT;
     break;
  }

  return str_label;
}

// get pilot wire state from relays state
uint8_t PilotwireGetRelayState ()
{
  uint8_t device_type;
  uint8_t relay1 = 0;
  uint8_t relay2 = 0;
  uint8_t state  = 0;
    
  // get device connexion type
  device_type = PilotwireGetDeviceType ();

  // read relay state
  relay1 = bitRead (power, 0);

  // if pilotwire connexion and 2 relays
  if ((device_type == PILOTWIRE_DEVICE_NORMAL) && (pilotwire_devices_present > 1))
  {
    // read second relay state and convert to pilotwire state
    relay2 = bitRead (power, 1);
    if (relay1 == 0 && relay2 == 0) state = PILOTWIRE_COMFORT;
    else if (relay1 == 0 && relay2 == 1) state = PILOTWIRE_OFF;
    else if (relay1 == 1 && relay2 == 0) state = PILOTWIRE_FROST;
    else if (relay1 == 1 && relay2 == 1) state = PILOTWIRE_ECO;
  }

  // else, if pilotwire connexion and 1 relay
  else if (device_type == PILOTWIRE_DEVICE_NORMAL)
  {
    // convert to pilotwire state
    if (relay1 == 0) state = PILOTWIRE_COMFORT;
    else state = PILOTWIRE_OFF;
  }

  // else, direct connexion
  else
  {
    // convert to pilotwire state
    if (relay1 == 0) state = PILOTWIRE_OFF;
    else state = PILOTWIRE_COMFORT;
  }

  return state;
}

// set relays state
void PilotwireSetRelayState (uint8_t new_state)
{
  uint8_t device_type;

  // get device connexion type
  device_type = PilotwireGetDeviceType ();

  // set number of relay to start command
  devices_present = pilotwire_devices_present;

  // if pilotwire connexion and 2 relays
  if ((device_type == PILOTWIRE_DEVICE_NORMAL) && (pilotwire_devices_present > 1))
  {
    // command relays for 4 modes pilotwire
    switch (new_state)
    {
      case PILOTWIRE_OFF:       // Set Off
        ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
        ExecuteCommandPower (2, POWER_ON, SRC_MAX);
        break;
      case PILOTWIRE_COMFORT:   // Set Comfort
        ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
        ExecuteCommandPower (2, POWER_OFF, SRC_MAX);
        break;
      case PILOTWIRE_ECO:       // Set Economy
        ExecuteCommandPower (1, POWER_ON, SRC_MAX);
        ExecuteCommandPower (2, POWER_ON, SRC_MAX);
        break;
      case PILOTWIRE_FROST:     // Set No Frost
        ExecuteCommandPower (1, POWER_ON, SRC_MAX);
        ExecuteCommandPower (2, POWER_OFF, SRC_MAX);
        break;
    }
  }

  // else, if pilotwire connexion and 1 relay
  else if (device_type == PILOTWIRE_DEVICE_NORMAL)
  {
    // command relay for 2 modes pilotwire
    if ((new_state == PILOTWIRE_OFF) || (new_state == PILOTWIRE_FROST)) ExecuteCommandPower (1, POWER_ON, SRC_MAX);
    else if ((new_state == PILOTWIRE_COMFORT) || (new_state == PILOTWIRE_ECO)) ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
  }

  // else direct connexion
  else
  {
    // command relay for direct connexion
    if ((new_state == PILOTWIRE_OFF) || (new_state == PILOTWIRE_FROST)) ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
    else if ((new_state == PILOTWIRE_COMFORT) || (new_state == PILOTWIRE_ECO)) ExecuteCommandPower (1, POWER_ON, SRC_MAX);
  }

  // reset number of relay
  devices_present = 0;
}

// get pilotwire device type (pilotwire or direct command)
uint8_t PilotwireGetDeviceType ()
{
  uint8_t device_type;

  // read actual VMC mode
  device_type = (uint8_t) Settings.energy_current_calibration;

  // if outvalue, set to disabled
  if (device_type > PILOTWIRE_DEVICE_DIRECT) device_type = PILOTWIRE_DEVICE_NORMAL;

  return device_type;
}

// set pilotwire device type (pilotwire or direct command)
void PilotwireSetDeviceType (uint8_t device_type)
{
  // if within range, set device type
  if (device_type <= PILOTWIRE_DEVICE_DIRECT) Settings.energy_current_calibration = (unsigned long) device_type;

  // update JSON status
  pilotwire_updated = true;
}

// get pilot actual mode
uint8_t PilotwireGetMode ()
{
  uint8_t actual_mode;

  // read actual mode and set to disabled if out of range
  actual_mode = (uint8_t) Settings.weight_reference;
  if (actual_mode > PILOTWIRE_OFFLOAD) actual_mode = PILOTWIRE_DISABLED;

  return actual_mode;
}

// set pilot wire mode
void PilotwireSetMode (uint8_t new_mode)
{
  uint8_t device_type;

  // get device connexion type
  device_type = PilotwireGetDeviceType ();

  // handle 1 relay device or direct connexion
  if ((pilotwire_devices_present == 1) || (device_type == PILOTWIRE_DEVICE_DIRECT))
  {
    if (new_mode == PILOTWIRE_ECO)   new_mode = PILOTWIRE_COMFORT;
    if (new_mode == PILOTWIRE_FROST) new_mode = PILOTWIRE_OFF;
  }

  // if within range, set mode
  if (new_mode <= PILOTWIRE_OFFLOAD) Settings.weight_reference = (unsigned long) new_mode;

  // update JSON status
  pilotwire_updated = true;
}

// set pilot wire minimum temperature
void PilotwireSetMinTemperature (float new_temperature)
{
  // if within range, save temperature correction
  if ((new_temperature >= PILOTWIRE_TEMP_MIN) && (new_temperature <= PILOTWIRE_TEMP_MAX)) Settings.weight_item = (unsigned long) int (new_temperature * 10);

  // update JSON status
  pilotwire_updated = true;
}

// get pilot wire minimum temperature
float PilotwireGetMinTemperature ()
{
  float temperature;

  // get drift temperature (/10)
  temperature = float (Settings.weight_item) / 10;
  
  // check if within range
  if (temperature < PILOTWIRE_TEMP_MIN || temperature > PILOTWIRE_TEMP_MAX) temperature = PILOTWIRE_TEMP_DEFAULT_MIN;

  return temperature;
}

// set pilot wire maximum temperature
void PilotwireSetMaxTemperature (float new_temperature)
{
  // if within range, save temperature correction
  if (new_temperature >= PILOTWIRE_TEMP_MIN && new_temperature <= PILOTWIRE_TEMP_MAX) Settings.energy_frequency_calibration = (unsigned long) int (new_temperature * 10);

  // update JSON status
  pilotwire_updated = true;
}

// get pilot wire maximum temperature
float PilotwireGetMaxTemperature ()
{
  float temperature;

  // get drift temperature (/10)
  temperature = float (Settings.energy_frequency_calibration) / 10;
  
  // check if within range
  if (temperature < PILOTWIRE_TEMP_MIN || temperature > PILOTWIRE_TEMP_MAX) temperature = PILOTWIRE_TEMP_DEFAULT_MAX;

  return temperature;
}


// set target temperature
void PilotwireSetTargetTemperature (float new_thermostat)
{
  // save target temperature
  if (new_thermostat >= PILOTWIRE_TEMP_MIN && new_thermostat <= PILOTWIRE_TEMP_MAX)
  {
    // save new target
    Settings.weight_max = (uint16_t) int (new_thermostat * 10);

    // reset outside mode
    pilotwire_outside_mode = false;
  }

  // update JSON status
  pilotwire_updated = true;
}

// get target temperature
float PilotwireGetTargetTemperature ()
{
  float temperature;

  // get target temperature (/10)
  temperature = float (Settings.weight_max) / 10;
  
  // check if within range
  if (temperature < PILOTWIRE_TEMP_MIN || temperature > PILOTWIRE_TEMP_MAX) temperature = PILOTWIRE_TEMP_DEFAULT_TARGET;

  return temperature;
}

// set outside mode dropdown temperature
void PilotwireSetOutsideDropdown (float new_dropdown)
{
  // save target temperature
  if (new_dropdown <= PILOTWIRE_TEMP_DROP) Settings.energy_voltage_calibration = (unsigned long) int (new_dropdown * 10);

  // update JSON status
  pilotwire_updated = true;
}

// get outside mode dropdown temperature
float PilotwireGetOutsideDropdown ()
{
  float temperature;

  // get target temperature (/10)
  temperature = float (Settings.energy_voltage_calibration) / 10;

  // check if within range
  if (temperature > PILOTWIRE_TEMP_DROP) temperature = PILOTWIRE_TEMP_DEFAULT_DROP;

  return temperature;
}

// set pilot wire drift temperature
void PilotwireSetDrift (float new_drift)
{
  // if within range, save temperature correction
  if (abs (new_drift) <= PILOTWIRE_TEMP_DRIFT) Settings.weight_calibration = (unsigned long) int (50 + (new_drift * 10));

  // update JSON status
  pilotwire_updated = true;
}

// get pilot wire drift temperature
float PilotwireGetDrift ()
{
  float temperature;

  // get drift temperature (/10)
  temperature = float (Settings.weight_calibration);
  temperature = ((temperature - 50) / 10);
  
  // check if within range
  if (abs (temperature) > PILOTWIRE_TEMP_DRIFT) temperature = PILOTWIRE_TEMP_DEFAULT_DRIFT;

  return temperature;
}

/******************************************************\
 *                         Functions
\******************************************************/

// get DS18B20 internal pullup state
bool PilotwireGetDS18B20Pullup ()
{
  bool flag_ds18b20 = bitRead (Settings.flag3.data, 24);

  return flag_ds18b20;
}

// set DS18B20 internal pullup state
bool PilotwireSetDS18B20Pullup (bool new_state)
{
  bool actual_state = PilotwireGetDS18B20Pullup ();

  // if not set, set pullup resistor for DS18B20 temperature sensor
  if (actual_state != new_state) bitWrite (Settings.flag3.data, 24, new_state);

  return (actual_state != new_state);
}

// get current temperature with drift correction
float PilotwireGetTemperature ()
{
  uint8_t index;
  float   temperature = NAN;

  // read temperature from local sensor
  pilotwire_temperature_source = PILOTWIRE_SOURCE_LOCAL;

#ifdef USE_DS18x20
  // read from DS18B20 sensor
  index = ds18x20_sensor[0].index;
  if (ds18x20_sensor[index].valid) temperature = ds18x20_sensor[index].temperature;
#endif // USE_DS18x20

#ifdef USE_DHT
  // read from DHT sensor
  if (isnan (temperature)) temperature = Dht[0].t;
#endif // USE_DHT

  // if not available, read MQTT temperature
  if (isnan (temperature))
  {
    pilotwire_temperature_source = PILOTWIRE_SOURCE_REMOTE;
    temperature = TemperatureGetValue ();
  }

  // if temperature is available, apply correction
  if (!isnan (temperature))
  {
    // trunc temperature to 0.2°C
    temperature = ceil (temperature * 10) / 10;

    // add correction
    temperature += PilotwireGetDrift ();
  }

  // else, no temperature source available
  else pilotwire_temperature_source = PILOTWIRE_SOURCE_NONE;

  return temperature;
}

// get current target temperature (handling outside dropdown)
float PilotwireGetCurrentTarget ()
{
  float  temp_target;

  // get target temperature
  temp_target = PilotwireGetTargetTemperature ();

  // if outside mode enabled, substract outside mode dropdown
  if (pilotwire_outside_mode == true) temp_target -= PilotwireGetOutsideDropdown ();

  return temp_target;
}

void PilotwireHandleTimer (uint8_t state)
{
  // set outside mode according to timer state (OFF = outside mode)
  pilotwire_outside_mode = (state == 0);
}

// Show JSON status (for MQTT)
void PilotwireShowJSON (bool append)
{
  uint8_t  actual_mode;
  float    actual_temperature;
  String   str_temperature, str_label, str_json, str_mqtt;

  // get temperature and mode
  actual_temperature = PilotwireGetTemperature ();
  actual_mode = PilotwireGetMode ();
  str_label   = PilotwireGetStateLabel (actual_mode);
 
  // pilotwire mode  -->  "PilotWire":{"Relay":2,"Mode":5,"Label":"Thermostat",Offload:"ON","Temperature":20.5,"Target":21,"Min"=15.5,"Max"=25}
  str_json = "\"" + String (D_JSON_PILOTWIRE) + "\":{";

  // thermostat data
  str_json += "\"" + String (D_JSON_PILOTWIRE_TARGET) + "\":" + String (PilotwireGetTargetTemperature (), 1) + ",";
  str_json += "\"" + String (D_JSON_PILOTWIRE_DRIFT) + "\":" + String (PilotwireGetDrift (), 1) + ",";
  str_json += "\"" + String (D_JSON_PILOTWIRE_MIN) + "\":" + String (PilotwireGetMinTemperature (), 1) + ",";
  str_json += "\"" + String (D_JSON_PILOTWIRE_MAX) + "\":" + String (PilotwireGetMaxTemperature (), 1) + ",";

  // outside mode
  str_json += "\"" + String (D_JSON_PILOTWIRE_OUTSIDE) + "\":";
  if (pilotwire_outside_mode == true) str_json += "\"ON\",";
  else str_json += "\"OFF\",";

  // if defined, add current temperature
  if (actual_temperature != NAN) str_json += "\"" + String (D_JSON_PILOTWIRE_TEMPERATURE) + "\":" + String (actual_temperature, 1) + ",";

  // current status
  str_json += "\"" + String (D_JSON_PILOTWIRE_RELAY) + "\":" + String (pilotwire_devices_present) + ",";
  str_json += "\"" + String (D_JSON_PILOTWIRE_MODE) + "\":" + String (actual_mode) + ",";
  str_json += "\"" + String (D_JSON_PILOTWIRE_LABEL) + "\":\"" + str_label + "\"}";

  // if pilot wire mode is enabled
  if (actual_mode != PILOTWIRE_DISABLED)
  {
    // get mode and associated label
    actual_mode = PilotwireGetRelayState ();
    str_label   = PilotwireGetStateLabel (actual_mode);

    // relay state  -->  ,"State":{"Mode":4,"Label":"Comfort"}
    str_json += ",\"" + String (D_JSON_PILOTWIRE_STATE) + "\":{";
    str_json += "\"" + String (D_JSON_PILOTWIRE_MODE) + "\":" + String (actual_mode);
    str_json += ",\"" + String (D_JSON_PILOTWIRE_LABEL) + "\":\"" + str_label + "\"}";
  }

  // generate MQTT message according to append mode
  if (append == true) str_mqtt = mqtt_data + String (",") + str_json;
  else str_mqtt = "{" + str_json + "}";

  // place JSON back to MQTT data and publish it if not in append mode
  snprintf_P (mqtt_data, sizeof(mqtt_data), str_mqtt.c_str ());
  if (append == false) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));

  // updates have been published
  pilotwire_updated = false; 
}

// Handle pilot wire MQTT commands
bool PilotwireMqttCommand ()
{
  bool command_handled = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kPilotWireCommands);

  // handle command
  switch (command_code)
  {
    case CMND_PILOTWIRE_MODE:  // set mode
      PilotwireSetMode (XdrvMailbox.payload);
      break;
    case CMND_PILOTWIRE_TARGET:  // set target temperature 
      PilotwireSetTargetTemperature (atof (XdrvMailbox.data));
      break;
    case CMND_PILOTWIRE_DRIFT:  // set temperature drift correction 
      PilotwireSetDrift (atof (XdrvMailbox.data));
      break;
    case CMND_PILOTWIRE_MIN:  // set minimum temperature 
      PilotwireSetMinTemperature (atof (XdrvMailbox.data));
      break;
    case CMND_PILOTWIRE_MAX:  // set maximum temperature 
      PilotwireSetMaxTemperature (atof (XdrvMailbox.data));
      break;
    case CMND_PILOTWIRE_OUTSIDE:  // set outside mode dropdown 
      PilotwireSetOutsideDropdown (atof (XdrvMailbox.data));
      break;
    default:
      command_handled = false;
  }

  // if needed, send updated status
  if (command_handled == true) PilotwireShowJSON (false);
  
  return command_handled;
}

void PilotwireUpdateGraph ()
{
  // if index is ok, check index set current graph value
  arr_temperature[pilotwire_graph_index] = pilotwire_graph_temperature;
  arr_target[pilotwire_graph_index] = pilotwire_graph_target;
  arr_state[pilotwire_graph_index] = pilotwire_graph_state;

  // init graph values
  pilotwire_graph_temperature = NAN;
  pilotwire_graph_target      = NAN;
  pilotwire_graph_state       = PILOTWIRE_OFF;

  // increase temperature data index and reset if max reached
  pilotwire_graph_index ++;
  pilotwire_graph_index = pilotwire_graph_index % PILOTWIRE_GRAPH_SAMPLE;
}

// update pilotwire status
void PilotwireUpdateStatus ()
{
  uint8_t heater_state, target_state, heater_mode;
  float   heater_temperature, target_temperature;

  // get heater data
  heater_mode  = PilotwireGetMode ();
  heater_state = PilotwireGetRelayState ();
  heater_temperature = PilotwireGetTemperature ();
  target_temperature = PilotwireGetCurrentTarget ();

  // update current temperature
  if (isnan(heater_temperature) == false)
  {
    // set new temperature
    pilotwire_temperature = heater_temperature;

    // update graph temperature for current slot
    if (isnan(pilotwire_graph_temperature) == false) pilotwire_graph_temperature = min (pilotwire_graph_temperature, heater_temperature);
    else pilotwire_graph_temperature = heater_temperature;
  }

  // update target temperature
  if (isnan(target_temperature) == false)
  {
    // if temperature change, data update
    if (target_temperature != pilotwire_target) pilotwire_updated = true;
    pilotwire_target = target_temperature;
    
    // update graph target temperature for current slot
    if (isnan(pilotwire_graph_target) == false) pilotwire_graph_target = min (pilotwire_graph_target, target_temperature);
    else pilotwire_graph_target = target_temperature;
  } 

  // if relay change, data update
  if (heater_state != pilotwire_state) pilotwire_updated = true;
  pilotwire_state = heater_state;

  // update graph relay state  for current slot
  if (pilotwire_graph_state != PILOTWIRE_COMFORT) pilotwire_graph_state = heater_state;

  // if pilotwire mode is enabled
  if (heater_mode != PILOTWIRE_DISABLED)
  {
    // if offload mode, target state is off
    if (OffloadIsOffloaded () == true) target_state = PILOTWIRE_OFF;
 
    // else if thermostat mode
    else if (heater_mode == PILOTWIRE_THERMOSTAT)
    {
      // if temperature is too low, target state is on
      // else, if too high, target state is off
      target_state = heater_state;
      if (heater_temperature < (target_temperature - PILOTWIRE_TEMP_THRESHOLD)) target_state = PILOTWIRE_COMFORT;
      else if (heater_temperature > (target_temperature + PILOTWIRE_TEMP_THRESHOLD)) target_state = PILOTWIRE_OFF;
    }

    // else target mode is heater mode
    else target_state = heater_mode;

    // if target state is different than heater state, change state
    if (target_state != heater_state)
    {
      // set relays
      PilotwireSetRelayState (target_state);

      // trigger state update
      pilotwire_updated = true;
    }
  }

  // if needed, publish new state
  if (pilotwire_updated == true) PilotwireShowJSON (false);
}

void PilotwireEverySecond ()
{
  // increment delay counter and if delay reached, update history data
  if (pilotwire_graph_counter == 0) PilotwireUpdateGraph ();
  pilotwire_graph_counter ++;
  pilotwire_graph_counter = pilotwire_graph_counter % PILOTWIRE_GRAPH_REFRESH;
}

void PilotwireInit ()
{
  int index;

  // save number of devices and set it to 0
  pilotwire_devices_present = devices_present;
  devices_present = 0;

  // offloading module is not managing the relay
  OffloadSetRelayMode (false);

  // init default values
  pilotwire_temperature_source = PILOTWIRE_SOURCE_NONE;
  pilotwire_updated            = false;
  pilotwire_outside_mode       = false;
  pilotwire_temperature        = NAN;
  pilotwire_target             = NAN;
  pilotwire_state              = PILOTWIRE_OFF;
  pilotwire_graph_temperature  = NAN;
  pilotwire_graph_target       = NAN;
  pilotwire_graph_state        = PILOTWIRE_OFF;
  pilotwire_graph_index        = 0;
  pilotwire_graph_counter      = 0;
  pilotwire_langage            = PILOTWIRE_LANGAGE_ENGLISH;

  // initialise temperature graph
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    arr_temperature[index] = NAN;
    arr_target[index] = NAN;
    arr_state[index]  = PILOTWIRE_OFF;
  }
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// Pilotwire mode selection 
void PilotwireWebSelectMode ()
{
  uint8_t actual_mode, device_type;
  float   actual_temp;
  String  str_argument;
 
  // get mode,  temperature and connexion type
  actual_mode = PilotwireGetMode ();
  actual_temp = PilotwireGetTemperature ();
  device_type = PilotwireGetDeviceType ();

  // seletion : disabled
  if (actual_mode == PILOTWIRE_DISABLED) str_argument = D_PILOTWIRE_CHECKED;
  else str_argument = "";
  WSContentSend_P (str_conf_mode_select, D_CMND_PILOTWIRE_MODE, PILOTWIRE_DISABLED, PILOTWIRE_DISABLED, str_argument.c_str (), D_PILOTWIRE_DISABLED);

  // selection : off
  if (actual_mode == PILOTWIRE_OFF) str_argument = D_PILOTWIRE_CHECKED;
  else str_argument = "";
  WSContentSend_P (str_conf_mode_select, D_CMND_PILOTWIRE_MODE, PILOTWIRE_OFF, PILOTWIRE_OFF, str_argument.c_str (), D_PILOTWIRE_OFF);

  // if dual relay device
  if ((pilotwire_devices_present > 1) && (device_type == PILOTWIRE_DEVICE_NORMAL))
  {
    // selection : no frost
    if (actual_mode == PILOTWIRE_FROST) str_argument = D_PILOTWIRE_CHECKED;
    else str_argument = "";
    WSContentSend_P (str_conf_mode_select, D_CMND_PILOTWIRE_MODE, PILOTWIRE_FROST, PILOTWIRE_FROST, str_argument.c_str (), D_PILOTWIRE_FROST);

    // selection : eco
    if (actual_mode == PILOTWIRE_ECO) str_argument = D_PILOTWIRE_CHECKED;
    else str_argument = "";
    WSContentSend_P (str_conf_mode_select, D_CMND_PILOTWIRE_MODE, PILOTWIRE_ECO, PILOTWIRE_ECO, str_argument.c_str (), D_PILOTWIRE_ECO);
  }

  // selection : comfort
  if (actual_mode == PILOTWIRE_COMFORT) str_argument = D_PILOTWIRE_CHECKED;
  else str_argument = "";
  WSContentSend_P (str_conf_mode_select, D_CMND_PILOTWIRE_MODE, PILOTWIRE_COMFORT, PILOTWIRE_COMFORT, str_argument.c_str (), D_PILOTWIRE_COMFORT);

  // selection : thermostat
  if (isnan(actual_temp) == false) 
  {
    // selection : target temperature
    if (actual_mode == PILOTWIRE_THERMOSTAT) str_argument = D_PILOTWIRE_CHECKED;
    else str_argument = "";
    WSContentSend_P (str_conf_mode_select, D_CMND_PILOTWIRE_MODE, PILOTWIRE_THERMOSTAT, PILOTWIRE_THERMOSTAT, str_argument.c_str (), D_PILOTWIRE_THERMOSTAT); 
  }
}

// display base64 embeded icon
void PilotwireWebDisplayIcon (uint8_t icon_height)
{
  uint8_t nbrItem, index;
  uint8_t icon_index;

  // if offload is active, display offload icon, else display heater state icon
  if (OffloadIsOffloaded () == true) OffloadWebDisplayIcon (icon_height, OFFLOAD_ICON_OFFLOADED);
  else 
  {
    // get heater status
    icon_index = PilotwireGetRelayState ();

    // display icon
    WSContentSend_P (PSTR ("<img height=%d src='data:image/png;base64,"), icon_height);
    nbrItem = sizeof (pilotwire_icon[icon_index]) / sizeof (char*);
    for (index=0; index<nbrItem; index++) if (pilotwire_icon[icon_index][index] != nullptr) WSContentSend_P (pilotwire_icon[icon_index][index]);
    WSContentSend_P (PSTR ("' >"));
  }
}

// append pilotwire control button to main page
void PilotwireWebMainButton ()
{
  float  temperature;
  String str_text;

  // heater mode switch button
  temperature = PilotwireGetOutsideDropdown ();
  if (pilotwire_outside_mode == true) str_text = String (D_PILOTWIRE_NORMAL) + " " + String (D_PILOTWIRE_MODE);
  else str_text = String (D_PILOTWIRE_OUTSIDE) + " " + String (D_PILOTWIRE_MODE) + " (-" + String (temperature, 1) + "°" + String (TempUnit ()) + ")";
  WSContentSend_P (str_conf_button, D_PAGE_PILOTWIRE_SWITCH, str_text.c_str());

  // heater control page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_PILOTWIRE_CONTROL, D_PILOTWIRE_CONTROL);
}

// append pilotwire configuration button to configuration page
void PilotwireWebButton ()
{
  String str_text;

  // heater configuration button
  str_text = D_PILOTWIRE_CONFIGURE + String (" ") + D_PILOTWIRE;
  WSContentSend_P (str_conf_button, D_PAGE_PILOTWIRE_CONFIG, str_text.c_str ());
}

// append pilot wire state to main page
bool PilotwireWebSensor ()
{
  uint8_t actual_mode;
  float   temperature;
  String  str_title, str_text, str_label, str_color;

  // get current mode and temperature
  actual_mode = PilotwireGetMode ();
  temperature = PilotwireGetTemperature ();

  // handle sensor source
  str_title = D_PILOTWIRE_TEMPERATURE;
  switch (pilotwire_temperature_source)
  {
    case PILOTWIRE_SOURCE_NONE:  // no temperature source available 
      str_text  = "<b>---</b>";
      break;
    case PILOTWIRE_SOURCE_LOCAL:  // local temperature source used 
      str_title += " <small><i>(" + String (D_PILOTWIRE_LOCAL) + ")</i></small>";
      str_text  = "<b>" + String (temperature, 1) + "</b>";
      break;
    case PILOTWIRE_SOURCE_REMOTE:  // remote temperature source used 
      str_title += " <small><i>(" + String (D_PILOTWIRE_REMOTE) + ")</i></small>";
      str_text  = "<b>" + String (temperature, 1) + "</b>";
      break;
  }

  // add target temperature and unit
  if (actual_mode == PILOTWIRE_THERMOSTAT) str_text += " / " + String (PilotwireGetCurrentTarget (), 1);
  str_text += " °" + String (TempUnit ());

  // display temperature
  WSContentSend_PD ("{s}%s{m}%s{e}\n", str_title.c_str (), str_text.c_str ());

  // display day or outside mode
  temperature = PilotwireGetOutsideDropdown ();
  if (pilotwire_outside_mode == true) str_text = "<span><b>" + String (D_PILOTWIRE_OUTSIDE) + "</b> (-" + String (temperature, 1) + "°" + String (TempUnit ()) + ")</span>";
  else str_text = "<span><b>" + String (D_PILOTWIRE_NORMAL) + "</b></span>";
  WSContentSend_PD (PSTR("{s}%s{m}%s{e}\n"), D_PILOTWIRE_MODE, str_text.c_str ());

  // display heater icon status
  WSContentSend_PD (PSTR("<tr><td colspan=2 style='width:100%;text-align:center;padding:10px;'>"));
  PilotwireWebDisplayIcon (64);
  WSContentSend_PD (PSTR("</td></tr>\n"));
}

// Pilot Wire heater mode switch page
void PilotwireWebPageSwitchMode ()
{
  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // invert mode
  pilotwire_outside_mode = !pilotwire_outside_mode;

  // page header with dark background
  WSContentStart_P (D_PILOTWIRE_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='0;URL=/' />\n"));
  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body bgcolor='#303030' >\n"));
  WSContentStop ();
}

// Pilotwire heater configuration web page
void PilotwireWebPageConfigure ()
{
  bool    is_modified;
  uint8_t target_mode, device_type;
  float   actual_temperature;
  char    argument[PILOTWIRE_BUFFER_SIZE];
  String  str_text, str_temperature, str_temp_target, str_temp_step, str_drift_step, str_unit, str_pullup, str_checked;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // get temperature and target mode
  actual_temperature = PilotwireGetTemperature ();
  target_mode = PilotwireGetMode ();
  str_unit       = String (TempUnit ());
  str_temp_step  = String (PILOTWIRE_TEMP_STEP, 1);
  str_drift_step = String (PILOTWIRE_TEMP_DRIFT_STEP, 1);

  // page comes from save button on configuration page
  if (Webserver->hasArg("save"))
  {
    // get pilot wire mode according to 'mode' parameter
    WebGetArg (D_CMND_PILOTWIRE_MODE, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetMode ((uint8_t)atoi (argument)); 

    // get pilot wire device type according to 'device' parameter
    WebGetArg (D_CMND_PILOTWIRE_DEVICE, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetDeviceType ((uint8_t)atoi (argument)); 

    // get minimum temperature according to 'min' parameter
    WebGetArg (D_CMND_PILOTWIRE_MIN, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetMinTemperature (atof (argument));

    // get maximum temperature according to 'max' parameter
    WebGetArg (D_CMND_PILOTWIRE_MAX, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetMaxTemperature (atof (argument));

    // get target temperature according to 'target' parameter
    WebGetArg (D_CMND_PILOTWIRE_TARGET, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetTargetTemperature (atof (argument));

    // get outside mode dropdown temperature according to 'outside' parameter
    WebGetArg (D_CMND_PILOTWIRE_OUTSIDE, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetOutsideDropdown (atof (argument));

    // get temperature drift according to 'drift' parameter
    WebGetArg (D_CMND_PILOTWIRE_DRIFT, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetDrift (atof (argument));

    // set ds18b20 pullup according to 'pullup' parameter
    WebGetArg (D_CMND_PILOTWIRE_PULLUP, argument, PILOTWIRE_BUFFER_SIZE);
    is_modified = PilotwireSetDS18B20Pullup (strlen(argument) > 0);
    if (is_modified == true) WebRestart (1);
  }

  // beginning of form
  WSContentStart_P (D_PILOTWIRE_CONFIGURE " " D_PILOTWIRE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR("<form method='get' action='%s'>\n"), D_PAGE_PILOTWIRE_CONFIG);

  //     Mode 
  // --------------

  WSContentSend_P (str_conf_fieldset_start, D_PILOTWIRE_HEATER);

  // mode selection
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span></p>\n"), D_PILOTWIRE_MODE, str_conf_topic_style, D_CMND_PILOTWIRE_MODE);
  PilotwireWebSelectMode ();

  // command type selection
  device_type = PilotwireGetDeviceType ();
  WSContentSend_P (PSTR ("<p>%s</p>\n"), D_PILOTWIRE_CONNEXION);
  if (device_type == PILOTWIRE_DEVICE_NORMAL) str_checked = D_PILOTWIRE_CHECKED;
  else str_checked = "";
  WSContentSend_P (PSTR ("<p><input type='radio' id='normal' name='%s' value=%d %s><label for='pilotwire'>%s</label></p>\n"), D_CMND_PILOTWIRE_DEVICE, PILOTWIRE_DEVICE_NORMAL, str_checked.c_str (), D_PILOTWIRE);
  if (device_type == PILOTWIRE_DEVICE_DIRECT) str_checked = D_PILOTWIRE_CHECKED;
  else str_checked = "";
  WSContentSend_P (PSTR ("<p><input type='radio' id='direct' name='%s' value=%d %s><label for='direct'>%s</label></p>\n"), D_CMND_PILOTWIRE_DEVICE, PILOTWIRE_DEVICE_DIRECT, str_checked.c_str (), D_PILOTWIRE_DIRECT);

  WSContentSend_P (str_conf_fieldset_stop);

  //   Thermostat 
  // --------------

  WSContentSend_P (str_conf_fieldset_start, D_PILOTWIRE_THERMOSTAT);

  // if temperature is available
  if (isnan (actual_temperature) == false) 
  {
    // target temperature
    str_temperature = String (PilotwireGetTargetTemperature (), 1);
    WSContentSend_P (str_conf_temperature, D_PILOTWIRE_TARGET, str_unit.c_str (), str_conf_topic_style, D_CMND_PILOTWIRE_TARGET, D_CMND_PILOTWIRE_TARGET, PILOTWIRE_TEMP_MIN, PILOTWIRE_TEMP_MAX, str_temp_step.c_str(), str_temperature.c_str());

    // temperature minimum label and input
    str_temperature = String (PilotwireGetMinTemperature (), 1);
    WSContentSend_P (str_conf_temperature, D_PILOTWIRE_MIN, str_unit.c_str (), str_conf_topic_style, D_CMND_PILOTWIRE_MIN, D_CMND_PILOTWIRE_MIN, PILOTWIRE_TEMP_MIN, PILOTWIRE_TEMP_MAX, str_temp_step.c_str(), str_temperature.c_str());

    // temperature maximum label and input
    str_temperature = String (PilotwireGetMaxTemperature (), 1);
    WSContentSend_P (str_conf_temperature, D_PILOTWIRE_MAX, str_unit.c_str (), str_conf_topic_style, D_CMND_PILOTWIRE_MAX, D_CMND_PILOTWIRE_MAX, PILOTWIRE_TEMP_MIN, PILOTWIRE_TEMP_MAX, str_temp_step.c_str(), str_temperature.c_str());

    // outside mode temperature dropdown
    str_temperature = String (PilotwireGetOutsideDropdown (), 1);
    str_text = D_PILOTWIRE_OUTSIDE + String (" ") + D_PILOTWIRE_MODE + String (" ") + D_PILOTWIRE_DROPDOWN;
    WSContentSend_P (str_conf_temperature, str_text.c_str (), str_unit.c_str (), str_conf_topic_style, D_CMND_PILOTWIRE_OUTSIDE, D_CMND_PILOTWIRE_OUTSIDE, 0, PILOTWIRE_TEMP_DROP, str_temp_step.c_str(), str_temperature.c_str());

    // temperature correction label and input
    str_temperature = String (PilotwireGetDrift (), 1);
    WSContentSend_P (str_conf_temperature, D_PILOTWIRE_DRIFT, str_unit.c_str (), str_conf_topic_style, D_CMND_PILOTWIRE_DRIFT, D_CMND_PILOTWIRE_DRIFT, - PILOTWIRE_TEMP_DRIFT, PILOTWIRE_TEMP_DRIFT, str_drift_step.c_str(), str_temperature.c_str());
  }
  else WSContentSend_P (PSTR ("<p><i>%s</i></p>\n"), D_PILOTWIRE_NOSENSOR);

  // pullup option for ds18b20 sensor
  if (PilotwireGetDS18B20Pullup ()) str_pullup = D_PILOTWIRE_CHECKED;
  WSContentSend_P (PSTR ("<hr>\n"));
  WSContentSend_P (PSTR ("<p><input type='checkbox' name='%s' %s>%s</p>\n"), D_CMND_PILOTWIRE_PULLUP, str_pullup.c_str(), D_PILOTWIRE_PULLUP);

  WSContentSend_P (str_conf_fieldset_stop);

  // save button
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Temperature graph
void PilotwireWebTemperatureGraph (uint8_t mode)
{
  TIME_T   current_dst;
  uint32_t current_time;
  int      index, array_index;
  int      graph_left, graph_right, graph_width, graph_x, graph_pos, graph_hour;
  uint8_t  state_current, state_previous;
  float    temp_min, temp_max, temp_scope, value, graph_y;
  char     str_hour[4];
  String   str_value;

  // start of SVG graph
  WSContentSend_P (PSTR ("<svg viewBox='0 0 %d %d'>\n"), PILOTWIRE_GRAPH_WIDTH, PILOTWIRE_GRAPH_HEIGHT);

  // graph curve zone
  WSContentSend_P (PSTR ("<rect x='%d%%' y='0%%' width='%d%%' height='100%%' rx='10' />\n"), PILOTWIRE_GRAPH_PERCENT_START, PILOTWIRE_GRAPH_PERCENT_STOP - PILOTWIRE_GRAPH_PERCENT_START);

  // loop to adjust min and max temperature
  temp_min = PilotwireGetMinTemperature ();
  temp_max = PilotwireGetMaxTemperature ();
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    // if needed, adjust minimum and maximum temperature
    value = arr_temperature[index];
    if (isnan (value) == false)
    {
      if (value < temp_min) temp_min = floor (value);
      if (value > temp_max) temp_max = ceil (value);
    }
  }
  temp_scope = temp_max - temp_min;

  // boundaries of SVG graph
  graph_left  = PILOTWIRE_GRAPH_PERCENT_START * PILOTWIRE_GRAPH_WIDTH / 100;
  graph_right = PILOTWIRE_GRAPH_PERCENT_STOP * PILOTWIRE_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // ----------------------
  //    Pilotwire state
  // ----------------------

  // loop for the pilotwire state as background red color
  graph_pos = graph_left;
  state_previous = PILOTWIRE_OFF;
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    // get target temperature value and set to minimum if not defined
    array_index = (index + pilotwire_graph_index) % PILOTWIRE_GRAPH_SAMPLE;
    state_current = arr_state[array_index];

    // last graph point, force draw
    if ((index == PILOTWIRE_GRAPH_SAMPLE - 1) && (state_previous == PILOTWIRE_COMFORT)) state_current = PILOTWIRE_OFF;

    // if relay just switched on, record point
    if ((state_previous != PILOTWIRE_COMFORT) && (state_current == PILOTWIRE_COMFORT))
    {
      // calculate start point x (from 10% to 100%)
      graph_pos = graph_left + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE);
    }
    
    // esle, if relay just switched off, display state
    else if ((state_previous == PILOTWIRE_COMFORT) && (state_current != PILOTWIRE_COMFORT))
    {
      // calculate end point x (from 10% to 100%)
      graph_x = graph_left + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE);

      // draw relay off line
      WSContentSend_P (PSTR ("<rect class='relay' x='%d' y='95%%' width='%d' height='5%%' />\n"), graph_pos, graph_x - graph_pos);
    }

    // update previous state
    state_previous = state_current;
  }

  // -----------------
  //    Graph units
  // -----------------

  // graph temperature text
  str_value = String (temp_max, 1);
  WSContentSend_P (str_graph_temperature, 1, 5, str_value.c_str ());
  str_value = String (temp_min + (temp_max - temp_min) * 0.75, 1);
  WSContentSend_P (str_graph_temperature, 1, 27, str_value.c_str ());
  str_value = String (temp_min + (temp_max - temp_min) * 0.50, 1);
  WSContentSend_P (str_graph_temperature, 1, 52, str_value.c_str ());
  str_value = String (temp_min + (temp_max - temp_min) * 0.25, 1);
  WSContentSend_P (str_graph_temperature, 1, 77, str_value.c_str ());
  str_value = String (temp_min, 1);
  WSContentSend_P (str_graph_temperature, 1, 99, str_value.c_str ());

  // graph separation lines
  WSContentSend_P (str_graph_separation, PILOTWIRE_GRAPH_PERCENT_START, 25, PILOTWIRE_GRAPH_PERCENT_STOP, 25);
  WSContentSend_P (str_graph_separation, PILOTWIRE_GRAPH_PERCENT_START, 50, PILOTWIRE_GRAPH_PERCENT_STOP, 50);
  WSContentSend_P (str_graph_separation, PILOTWIRE_GRAPH_PERCENT_START, 75, PILOTWIRE_GRAPH_PERCENT_STOP, 75);

  // ----------------------
  //   Target Temperature
  // ----------------------

  // loop for the target temperature graph
  WSContentSend_P (PSTR ("<polyline class='target' points='"));
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    // get target temperature value and set to minimum if not defined
    array_index = (index + pilotwire_graph_index) % PILOTWIRE_GRAPH_SAMPLE;
    value = arr_target[array_index];
    if (isnan (value) == true) value = temp_min;

    // calculate end point x and y
    graph_x = graph_left + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE);
    graph_y = (1 - ((value - temp_min) / temp_scope)) * PILOTWIRE_GRAPH_HEIGHT;

    // add the point to the line
    WSContentSend_P (PSTR("%d,%d "), graph_x, int (graph_y));
  }
  WSContentSend_P (PSTR("'/>\n"));

  // ----------------
  //   Temperature
  // ----------------

  // loop for the temperature graph
  WSContentSend_P (PSTR ("<polyline class='temperature' points='"));
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    // if temperature value is defined
    array_index = (index + pilotwire_graph_index) % PILOTWIRE_GRAPH_SAMPLE;
    value = arr_temperature[array_index];
    if (isnan (value) == false)
    {
      // calculate end point x and y
      graph_x = graph_left + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE);
      graph_y  = (1 - ((value - temp_min) / temp_scope)) * PILOTWIRE_GRAPH_HEIGHT;

      // add the point to the line
      WSContentSend_P (PSTR("%d,%d "), graph_x, int (graph_y));
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

  // ------------
  //     Time
  // ------------

  // get current time
  current_time = LocalTime();
  BreakTime (current_time, current_dst);

  // calculate width of remaining (minutes) till next hour
  current_dst.hour = (current_dst.hour + 1) % 24;
  graph_hour = ((60 - current_dst.minute) * graph_width / 1440) - 15; 

  // if shift is too small, shift to next hour
  if (graph_hour < 0)
  {
    current_dst.hour = (current_dst.hour + 1) % 24;
    graph_hour += graph_width / 24; 
  }

  // dislay first time mark
  graph_x = graph_left + graph_hour;
  sprintf(str_hour, "%02d", current_dst.hour);
  WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%sh</text>\n"), graph_x, 51, str_hour);

  // dislay next 5 time marks (every 4 hours)
  for (index = 0; index < 5; index++)
  {
    current_dst.hour = (current_dst.hour + 4) % 24;
    graph_x += 48 * graph_width / PILOTWIRE_GRAPH_SAMPLE;
    sprintf(str_hour, "%02d", current_dst.hour);
    WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%sh</text>\n"), graph_x, 51, str_hour);
  }

  // --------------
  //   New Target
  // --------------

  // if thermostat mode, display target
  if (mode == PILOTWIRE_THERMOSTAT)
  {
    // get target temperature
    value = PilotwireGetTargetTemperature ();

    // calculate and display target temperature line
    graph_y = (100 * (1 - ((value - temp_min) / temp_scope)));
    index   = int (graph_y);
    WSContentSend_P (PSTR ("<line id='line' class='target' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n"), PILOTWIRE_GRAPH_PERCENT_START, index, PILOTWIRE_GRAPH_PERCENT_STOP, index);

    // display target temperature text
    index = max (min (index + 2, 99), 4);
    str_value = String (value, 1) + "°C";
    WSContentSend_P (PSTR ("<text id='text' class='target' x='%d%%' y='%d%%'>%s</text>\n"), PILOTWIRE_GRAPH_PERCENT_STOP + 1, index, str_value.c_str ());
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
}

// Pilot Wire heater public configuration web page
void PilotwireWebPageControl ()
{
  int     index;
  uint8_t actual_mode, actual_state;
  char    argument[PILOTWIRE_BUFFER_SIZE];
  float   temperature, temp_target, temp_min, temp_max, temp_scope, slider_min, slider_max;
  String  str_temperature, str_temp_target, str_temp_min, str_temp_max, str_temp_step, str_temp_scope, str_slider_min, str_slider_max, str_text;

  // if langage is changed
  if (Webserver->hasArg(D_CMND_PILOTWIRE_LANG))
  {
    // get langage according to 'lang' parameter
    WebGetArg (D_CMND_PILOTWIRE_LANG, argument, PILOTWIRE_BUFFER_SIZE);

    if (strcmp (argument, D_PILOTWIRE_ENGLISH) == 0) pilotwire_langage = PILOTWIRE_LANGAGE_ENGLISH;
    else if (strcmp (argument, D_PILOTWIRE_FRENCH) == 0) pilotwire_langage = PILOTWIRE_LANGAGE_FRENCH;
  }

  // if heater has to be switched off, set in anti-frost mode
  if (Webserver->hasArg(D_CMND_PILOTWIRE_OFF)) PilotwireSetMode (PILOTWIRE_FROST); 

  // else, if heater has to be switched on, set in thermostat mode
  else if (Webserver->hasArg(D_CMND_PILOTWIRE_ON)) PilotwireSetMode (PILOTWIRE_THERMOSTAT);

   // else, if target temperature has been defined
  else if (Webserver->hasArg(D_CMND_PILOTWIRE_TARGET))
  {
    // get target temperature according to 'target' parameter
    WebGetArg (D_CMND_PILOTWIRE_TARGET, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetTargetTemperature (atof (argument));
  }

  // update heater status
  PilotwireUpdateStatus ();

  // mode
  actual_mode  = PilotwireGetMode ();
  actual_state = PilotwireGetRelayState ();

  // minimum and maximum temperature
  temp_min = PilotwireGetMinTemperature ();
  temp_max = PilotwireGetMaxTemperature ();
  slider_min = temp_min;
  slider_max = temp_max;

  // loop to adjust min and max temperature
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    // if needed, adjust minimum and maximum temperature
    temperature = arr_temperature[index];
    if (isnan (temperature) == false)
    {
      if (temperature < temp_min) temp_min = floor (temperature);
      if (temperature > temp_max) temp_max = ceil (temperature);
    }
  }
  temp_scope = temp_max - temp_min;
  
  // get tempreratures
  temperature = PilotwireGetTemperature ();
  temp_target = PilotwireGetTargetTemperature ();

  // convert temperatures to string with 1 digit
  if (!isnan (temperature)) str_temperature = String (temperature, 1);
  str_temp_target = String (temp_target, 1);
  str_temp_min    = String (temp_min, 1);
  str_temp_max    = String (temp_max, 1);
  str_temp_scope  = String (temp_scope, 1);
  str_temp_step   = String (PILOTWIRE_TEMP_STEP, 1);
  str_slider_min  = String (slider_min, 1);
  str_slider_max  = String (slider_max, 1);

  // beginning of form without authentification with 60 seconds auto refresh
  WSContentStart_P (D_PILOTWIRE_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%d;URL=/%s' />\n"), 60, D_PAGE_PILOTWIRE_CONTROL);
  
  // page style
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR ("body {color:white;background-color:#303030;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:12px auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("fieldset {border-radius:10px;}\n"));
  
  WSContentSend_P (PSTR (".title {font-size:5vh;}\n"));
  WSContentSend_P (PSTR (".value {font-size:4vh;}\n"));
  WSContentSend_P (PSTR (".lang {font-size:2vh;}\n"));
  
  WSContentSend_P (PSTR (".temperature {font-size:24px;}\n"));
  WSContentSend_P (PSTR (".target {font-size:24px;}\n"));
  WSContentSend_P (PSTR (".time {font-size:20px;}\n"));
  WSContentSend_P (PSTR (".bold {font-weight:bold;}\n"));

  WSContentSend_P (PSTR (".centered {width:%d%%;max-width:%dpx;}\n"), PILOTWIRE_GRAPH_PERCENT_STOP - PILOTWIRE_GRAPH_PERCENT_START, PILOTWIRE_GRAPH_WIDTH * (PILOTWIRE_GRAPH_PERCENT_STOP - PILOTWIRE_GRAPH_PERCENT_START) / 100);
  WSContentSend_P (PSTR (".graph {margin-top:20px;max-width:%dpx;}\n"), PILOTWIRE_GRAPH_WIDTH);
  WSContentSend_P (PSTR (".thermostat {margin-bottom:10px;}\n"));
  WSContentSend_P (PSTR (".yellow {color:#FFFF33;}\n"));

  WSContentSend_P (PSTR (".status {color:red;font-size:18px;font-weight:bold;font-style:italic;}\n"));

  WSContentSend_P (PSTR (".button {border:1px solid white;margin:12px 0px auto;padding:4px 12px;border-radius:12px;font-size:20px;}\n"));
  WSContentSend_P (PSTR (".btn-read {color:orange;border:1px solid transparent;background-color:transparent;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".btn-set {color:orange;border-color:orange;background-color:#FFEBE8;}\n"));

  WSContentSend_P (PSTR (".slider {width:100%%;height:8px;border-radius:4px;background:#FFEBE8;outline:none;opacity:0.7;-webkit-appearance:none;}\n"));
  WSContentSend_P (PSTR (".slider::-webkit-slider-thumb {width:25px;height:25px;border-radius:50%%;background:orange;cursor:pointer;border:1px solid orange;appearance:none;-webkit-appearance:none;}\n"));
  WSContentSend_P (PSTR (".slider::-moz-range-thumb {width:25px;height:25px;border-radius:50%%;background:orange;cursor:pointer;border:1px solid orange;}\n"));
  WSContentSend_P (PSTR (".slider::-ms-thumb {width:25px;height:25px;border-radius:50%%;background:orange;cursor:pointer;border:1px solid orange;}\n"));

  WSContentSend_P (PSTR (".switch {text-align:left;font-size:24px;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".toggle input {display:none;}\n"));
  WSContentSend_P (PSTR (".toggle {position:relative;display:inline-block;width:140px;height:40px;margin:10px auto;}\n"));
  WSContentSend_P (PSTR (".slide-off {position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#ff0000;border:1px solid #aaa;border-radius:5px;}\n"));
  WSContentSend_P (PSTR (".slide-off:before {position:absolute;content:'';width:64px;height:34px;left:2px;top:2px;background-color:#eee;border-radius:5px;}\n"));
  WSContentSend_P (PSTR (".slide-off:after {position:absolute;content:'Off';top:5px;right:20px;color:#fff;}\n"));
  WSContentSend_P (PSTR (".slide-on {position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#8cbc13;border:1px solid #aaa;border-radius:5px;}\n"));
  WSContentSend_P (PSTR (".slide-on:before {position:absolute;content:'';width:64px;height:34px;left:72px;top:2px;background-color:#eee;border-radius:5px;}\n"));
  WSContentSend_P (PSTR (".slide-on:after {position:absolute;content:'On';top:5px;left:15px;color:#fff;}\n"));

  WSContentSend_P (PSTR ("rect.graph {stroke:grey;stroke-dasharray:1;fill:#101010;}\n"));
  WSContentSend_P (PSTR ("rect.relay {fill:red;fill-opacity:50%%;}\n"));
  WSContentSend_P (PSTR ("polyline.temperature {fill:none;stroke:yellow;stroke-width:4;}\n"));
  WSContentSend_P (PSTR ("polyline.target {fill:none;stroke:orange;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("line.target {stroke:orange;stroke-width:2;stroke-dasharray:4;}\n"));
  WSContentSend_P (PSTR ("text.temperature {stroke:yellow;fill:yellow;}\n"));
  WSContentSend_P (PSTR ("text.target {stroke:orange;fill:orange;}\n"));
  WSContentSend_P (PSTR ("text.time {stroke:white;fill:white;}\n"));

  WSContentSend_P (PSTR ("</style>\n</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<form name='control' method='get' action='%s'>\n"), D_PAGE_PILOTWIRE_CONTROL);

  // room name
  WSContentSend_P (PSTR ("<div class='title bold'>%s</div>\n"), SettingsText(SET_FRIENDLYNAME1));

  // current temperature
  if (str_temperature != "") WSContentSend_P (PSTR ("<div class='value bold yellow'>%s °C</div>\n"), str_temperature.c_str());

  // status icon
  WSContentSend_P (PSTR ("<div class='status'>"));
  PilotwireWebDisplayIcon (64);
  WSContentSend_P (PSTR ("</div>\n"));

  // if heater is in thermostat mode, button to switch off heater, else button to switch on thermostat
  WSContentSend_P (PSTR ("<div class='centered'>"));
  if (actual_mode == PILOTWIRE_THERMOSTAT) WSContentSend_P (PSTR ("<label class='toggle'><input name='off' type='submit'/><span class='slide-on switch'></span></label>"));
  else WSContentSend_P (PSTR ("<label class='toggle'><input name='on' type='submit'/><span class='slide-off switch'></span></label>"));
  WSContentSend_P (PSTR ("</div>\n"));

  // target temperature
  WSContentSend_P (PSTR ("<div class='centered thermostat'>\n"));
  if (actual_mode == PILOTWIRE_THERMOSTAT)
  {
    // start of fieldset
    WSContentSend_P (PSTR ("<fieldset><legend>&nbsp;%s&nbsp;</legend>\n"), arrControlTemp[pilotwire_langage]);

    // temperature selection slider
    WSContentSend_P (PSTR ("<input id='slider' name='target' type='range' class='slider' min='%s' max='%s' value='%s' step='%s' />\n"), str_slider_min.c_str(), str_slider_max.c_str(), str_temp_target.c_str(), str_temp_step.c_str());

    // button to set target temperature
    WSContentSend_P (PSTR ("<button id='button' name='temp' type='submit' class='button btn-read'>%s°C</button>\n"), str_temp_target.c_str());

    // end of fieldset
    WSContentSend_P (PSTR ("</fieldset>\n"));
  }
  WSContentSend_P (PSTR ("</div>\n"));

  // display temperature graph
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  PilotwireWebTemperatureGraph (actual_mode);
  WSContentSend_P (PSTR ("</div>\n"));

  // display langage selection
  WSContentSend_P (PSTR ("<div class='centered'>\n"));
  WSContentSend_P (PSTR ("<select name=lang class='lang' size=1 onChange='control.submit();'>\n"));
  for (index = 0; index <= PILOTWIRE_LANGAGE_FRENCH; index++)
  {
    if (index == pilotwire_langage) str_text = "selected";
    else str_text = "";
    WSContentSend_P (PSTR ("<option value='%s' %s>%s\n"), arrControlLang[index], str_text.c_str (), arrControlLangage[index]);
  }
  WSContentSend_P (PSTR ("</select>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  // if heater is in thermostat mode, enable the scripts   
  if (actual_mode == PILOTWIRE_THERMOSTAT)
  {
    // script to update target value
    WSContentSend_P (PSTR ("<script>\n"));

    WSContentSend_P (PSTR ("var slider=document.getElementById('slider');\n"));
    WSContentSend_P (PSTR ("var button=document.getElementById('button');\n"));
    WSContentSend_P (PSTR ("var line=document.getElementById('line');\n"));
    WSContentSend_P (PSTR ("var text=document.getElementById('text');\n"));

    WSContentSend_P (PSTR ("line.style.display='none';"));

    WSContentSend_P (PSTR ("slider.oninput=function() {\n"));

    WSContentSend_P (PSTR ("var posline=100 * (1- ((Number(this.value) - %s) / %s));\n"), str_temp_min.c_str(), str_temp_scope.c_str());
    WSContentSend_P (PSTR ("var postext=Math.max (Math.min (posline+2, 100), 4);\n"));

    WSContentSend_P (PSTR ("line.setAttribute('y1', posline + '%%');\n"));
    WSContentSend_P (PSTR ("line.setAttribute('y2', posline + '%%');\n"));
    WSContentSend_P (PSTR ("text.setAttribute('y', postext + '%%');\n"));
    WSContentSend_P (PSTR ("text.innerHTML=Number(this.value).toFixed(1) + '°C';\n"));

    WSContentSend_P (PSTR ("if (this.value == %s) { "), str_temp_target.c_str());
    WSContentSend_P (PSTR ("button.innerHTML=Number(this.value).toFixed(1) + '°C'; button.setAttribute('class', 'button btn-read'); "));
    WSContentSend_P (PSTR ("line.style.display='none';"));
    WSContentSend_P (PSTR ("} else { "));
    WSContentSend_P (PSTR ("button.innerHTML='%s ' + Number(this.value).toFixed(1) + '°C'; button.setAttribute('class', 'button btn-set'); "), arrControlSet[pilotwire_langage]);
    WSContentSend_P (PSTR ("line.style.display='block';"));
    WSContentSend_P (PSTR ("}\n"));

    WSContentSend_P (PSTR ("}\n"));

    WSContentSend_P (PSTR ("</script>\n"));
  }

  // end of page
  WSContentSend_P (PSTR ("</form>\n"));
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xsns98 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  {
    case FUNC_INIT:
      PilotwireInit ();
      break;
    case FUNC_COMMAND:
      result = PilotwireMqttCommand ();
      break;
    case FUNC_EVERY_250_MSECOND:
      PilotwireUpdateStatus ();
      break;
    case FUNC_EVERY_SECOND:
      PilotwireEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      PilotwireShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on ("/" D_PAGE_PILOTWIRE_CONFIG,  PilotwireWebPageConfigure);
      Webserver->on ("/" D_PAGE_PILOTWIRE_CONTROL, PilotwireWebPageControl);
      Webserver->on ("/" D_PAGE_PILOTWIRE_SWITCH,  PilotwireWebPageSwitchMode);
      break;
    case FUNC_WEB_SENSOR:
      PilotwireWebSensor ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      PilotwireWebMainButton ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      PilotwireWebButton ();
      break;
#endif  // USE_Webserver

  }
  return result;
}

#endif // USE_PILOTWIRE
