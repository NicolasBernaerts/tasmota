/*
  xsns_98_pilotwire.ino - French Pilot Wire (Fil Pilote) support 
  for Sonoff Basic or Sonoff Dual R2
  
  Copyright (C) 2019  Nicolas Bernaerts
    05/04/2019 - v1.0 - Creation
    12/04/2019 - v1.1 - Save settings in Settings.weight... variables
    10/06/2019 - v2.0 - Complete rewrite to add web management
    25/06/2019 - v2.1 - Add DHT temperature sensor and settings validity control
    05/07/2019 - v2.2 - Add embeded icon
    05/07/2019 - v3.0 - Add max power management with automatic offload
                        Save power settings in Settings.energy... variables
                       
  Settings are stored using weighting scale parameters :
    - Settings.weight_reference             = Fil pilote mode
    - Settings.weight_max                   = Target temperature  (x10 -> 192 = 19.2°C)
    - Settings.weight_calibration           = Temperature correction (0 = -5°C, 50 = 0°C, 100 = +5°C)
    - Settings.weight_item                  = Minimum temperature (x10 -> 125 = 12.5°C)
    - Settings.energy_frequency_calibration = Maximum temperature (x10 -> 240 = 24.0°C)

     
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
 *               Variables
\*************************************************/

#define PILOTWIRE_BUFFER_SIZE           128

#define D_PAGE_PILOTWIRE_HEATER         "heater"
#define D_PAGE_PILOTWIRE_METER          "meter"
#define D_PAGE_PILOTWIRE_CONTROL        "control"

#define D_CMND_PILOTWIRE_MODE           "mode"
#define D_CMND_PILOTWIRE_OFFLOAD        "offload"
#define D_CMND_PILOTWIRE_MIN            "min"
#define D_CMND_PILOTWIRE_MAX            "max"
#define D_CMND_PILOTWIRE_TARGET         "target"
#define D_CMND_PILOTWIRE_DRIFT          "drift"
#define D_CMND_PILOTWIRE_POWER          "power"
#define D_CMND_PILOTWIRE_PRIORITY       "priority"
#define D_CMND_PILOTWIRE_CONTRACT       "contract"
#define D_CMND_PILOTWIRE_MQTTTOPIC      "topic"
#define D_CMND_PILOTWIRE_JSONKEY        "key"

#define D_JSON_PILOTWIRE                "Pilotwire"
#define D_JSON_PILOTWIRE_MODE           "Mode"
#define D_JSON_PILOTWIRE_LABEL          "Label"
#define D_JSON_PILOTWIRE_OFFLOAD        "Offload"
#define D_JSON_PILOTWIRE_MIN            "Min"
#define D_JSON_PILOTWIRE_MAX            "Max"
#define D_JSON_PILOTWIRE_TARGET         "Target"
#define D_JSON_PILOTWIRE_DRIFT          "Drift"
#define D_JSON_PILOTWIRE_TEMPERATURE    "Temperature"
#define D_JSON_PILOTWIRE_RELAY          "Relay"
#define D_JSON_PILOTWIRE_STATE          "State"

#define PILOTWIRE_COLOR_BUFFER_SIZE     8
#define PILOTWIRE_LABEL_BUFFER_SIZE     16
#define PILOTWIRE_MESSAGE_BUFFER_SIZE   64

#define PILOTWIRE_TEMP_MIN              5
#define PILOTWIRE_TEMP_MAX              30
#define PILOTWIRE_TEMP_STEP             0.5
#define PILOTWIRE_TEMP_THRESHOLD        0.2
#define PILOTWIRE_DRIFT_MIN             -5
#define PILOTWIRE_DRIFT_MAX             5
#define PILOTWIRE_DRIFT_STEP            0.1

// icon coded in base64
const char strIconHeater0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAACUAAAAgCAQAAAA/Wnk7AAAC3npUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHja7ZdBkhshDEX3nCJHQBJC4jh0A1W5QY6fD83Y8YwzVZlkkYWhDLRafEAP6HLoP76P8A2JPHJIap5LzhEplVS4ouHxSldNMa1yJdb9jh7tgbc9MkyCWq7H3Ld/hV3vHSxt+/FoD3ZuHd9C+8WboMyR52jbz7eQ8GWn/RzK7lfTL8vZv2FrzEjb6f1zMgSjKfSEA3chiSh9jiKYgRSpqDNKkek0bbNdVunPYxduzXfBa+157GLdHvIYihDzdsjvYrTtpM9jtyL064zorcmPL4bEN9AfYzeaj9Gv1dWUEakc9qLillgtOB4IpaxuGdnwU7Rt5YLsWOIJYg00D+QzUCFGtAclalRpUF/1SSemmLizoWY+WZbNxbjwuQCkmWmwAUML4mB1gprAzLe50Bq3rPFOHIEWG8GTCWKEHh9yeGb8Sr4JjTG3LtEMJtDTBZjnnsY0JrlZwgtAaOyY6orvyuGG9Z5o7cIEtxlmxwJrPC6JQ+m+t2RxnnQ1phCvo0HWtgBChLEVkyEBgZhJlDJFYzYixNHBp2LmLIkPECBVbhQG2IhkwHGeY6OP0fJl5cuMqwUgFIfGgAYHBbBSUuwfS449VFU0BVXNaupatGbJKWvO2fK8o6qJJVPLZuZWrLp4cvXs5u7Fa+EiuMK05GK";
const char strIconHeater1[] PROGMEM = "heCmlVgxaIV3Ru8Kj1oMPOdKhRz7s8KMc9cT2OdOpZz7t9LOctXGThuPfcrPQvJVWO3VspZ669tytey+9Duy1ISMNHXnY8FFGvVHbVB+p0Ttyn1OjTW0SS8vP7tRgNnuToHmd6GQGYpwIxG0SwIbmySw6pcST3GQWC+NQKIMa6YTTaBIDwdSJddCN3Z3cp9yCpj/ixr8jFya6f0EuTHSb3EduT6i1ur4osgDNUzhjGmXgYoND98pe5zfpy3X4W4GX0EvoJfQSegm9hP4XoTFGaPjLFH4CjgdTc9MWax8AAAACYktHRAD/h4/MvwAAAAlwSFlzAAAX3AAAF9wBGQRXVgAAAAd0SU1FB+MGHRcnJmCWOwMAAAF8SURBVEjH7dYxSFRxHAfwj8+TQ0RDKaihpXKwzRBzC4ISbMtZqCUEHcNBAhHMhiaX0IaWHCTMzUFxiUMiMPAw4sAhsKEWtUHJojyHe955ouD9741+3/D+/z/vffi99/68/59Crnnri5zPevDcmpxVfRiQlZM1gD6rcta8dsUpabUlHx8jWInbk5iJ2zOYLF614fJxJAXGNassVz0z6IYpTfHIuwJ1X+W5g0fuFvu3ItAQQNWj7kg/iiSWlIe6hYCXTLldTk2rDyqiyZPygcgbm0HUvm175dSgi/4HUN+0mCinqsm8nWJ7LlUVldGYVFWSe8Bz6pw6jXovrzbgzuvytszqKFH3qiikWa9P+g+pB17IBzC/vD";
const char strIconHeater2[] PROGMEM = "Tnr8irwi8wkjFsP4DaNKRXpx01nibx2rNm0ZXMF/yKC6UltZpMu2mpRO0W19iz53d8/uHx0Sm6GFDNh5Nn+7DtCqHvxk7efqzrMKpdyq4VLEirs2cZGW3S/shgWZe0fz4a8fM4dQDPsF31pJ5WwwAAAABJRU5ErkJggg==";
const char *const arrIconHeaterBase64[] PROGMEM = {strIconHeater0, strIconHeater1, strIconHeater2};


const char strIconMeter0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAGc3pUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHjarVdZsuQoDPznFHMEJBDLcVgj5gZz/EkBdi2v3uvuibGjDJZZhDK1lBn//D3NX7iYXTBeYgo5BIvLZ5+5oJPsvnZL1q/nuvh8wvuL3NwfGCKH1u3XMM74Ark8JkR/5PVVbmI766SzENmXrZ3urP1+lDwLOd5yOu8mnwklPB3n/LidZa9jvb37CGN0wXqODQ9Hzq4n750ctHDZFTwJT+d0oN5lfVNJ+Go/c5vugwHv3pv97KWZe5hjL3QdK7zZ6chJ3uTu3oZfNCK+d+ZnjYq30T5fT/abs6c5xz5d8eBR9uEc6jrK6mFghTndmhZwR/wE/bjujDvZYhtQ6zhqNbbiJRPD4pM8dSo0aay2UYOKngdHtMyN3ZIlFzlzW6B4vWlyNMCnuwQ8GpBzEPOtC619s+6HzRJ27oSRTFgMGL/e5l3wX++XheZUmhPZdNsKerHyC2oocvrEKABC89hUln3J7Ma+X7RY6DFMzZxwwGLrXqIKPbjlFs7OisFQb7e/UOxnAZgIewuUAbs92UBOKJCNzJEIdkzAp0Bzdp4rECAxwh1asgfvAU5i3RtzIq2xLLzFCC8AQlxwEdDAgQCW9+ID/C2BQsWIEy8iQaIkyVKCCz5ICCEGjVMluuijxBBjTDHHklzySVJIMaWUU8mc";
const char strIconMeter1[] PROGMEM = "HcKYmBxyzCnnXAo2Lb5grYLxBYLK1VVfpYYaa6q5lgb6NN+khRZbarmVzt11hADTQ4899dzLoAEqDT9khBFHGnmUCa5NN/2UGWacaeZZbtQOqq+ovSP3M2p0UOMFlI6LD9QgjvFagjSciGIGxNgTEI+KAAjNiplN5D0rcoqZzeyMc8LQUhScTooYEPSDWCbd2D2Q+xY3A+v+KW78CTmj0P0fyBmF7gm5r7h9QK2XFW7dAki9EDZFhHRwv2FHKgwVEBZZe7/RYqtaduMT0lESFoKRMpOvXjRPSQRObXZtcmvFk8rF27YWKZoF31vz3Ydfto1K6DIZOoXeg2FVSEJx8PBwdGpQIR/lwugBgRe+Devm3BETVFnuHmuMEWZXdcUa9/tG+dA2WwDy4NAMkNY1gzJjGQvRCPCDfc9zvhNQyFOgsLeGNKRLIqlTsgPB89ERuWrbWAuF71udCio243ta5oCZLp10BGzFL1N+JTBPkqoW1grIYVmYefYAtqbiD+qZktDB4LVFloJGOqYnpE2vvXA2QznkQipxje+wSVzbfSszH4SCJUGVjO7q4BnWvn1rALN8NZ+R5KR2pYY0i0NkPUb1yNTXQlJG4gonQ4OIIiiSMCpP3+DRHq6l37qZ8ObShhWf0lCXD9CoBiZMKnBaRCi4YukY50uvBWGjU5s4O6jcwZoMuIGcQQNtQsQugG8ZfnOqL5UxFj7npuBYITrO53T2JjJDD6c2cqHiDJW4";
const char strIconMeter2[] PROGMEM = "79l5ecgbyPvD6XyUmO8G3ZS4Yd5f1EGpg4LH+WrdLETB/iCDLggySUUC2ML2BN7l+J8F5uMQ7agnfZEssqG/vE15hyp3G8tUVM/XSvDl0F3JiNAZx5p4U2JeoeITfe7W7I5EmPsKigoc6tNfuS68aBkgDKRA0mIUZLHgyWLiq35gTlNwu7iaoVze/oiadKTjAloX6njzMWQ1NQFyW6WE4BAa10KI8kUbcXTMUt2TF5snN3YFtkNO0rzGVSvZjGSldM5hNOfTtnhpqXzJJc3YUlEipL5jnjpZlEPmmgW5HqXzJnNOjyQC/tYw1MnWbBzZvPhEXsUGMjwCKHyHOLjD8hDkEYQ0WA0AIk9+blSH7d/YB2q6nH3aO6cvgYw0f2hkpb6wcpr2F5nIlGsFDqWhYjy58PkYO4JUhIIOu2HEUBMOJH1ZNc+aYYKXl8AJtXHYiCTHhyYrlqsiFakWdDg0Sa9J0zx5yFYtIVONxiENrUeUKnBRuwKoui0SpiaNlSIjk6DoGRp9DaKI0uGEHNiPsiR/BVXeYSeuIJzxBX8+BcUM15NxFk5KHVOUDJvXDTEB5dLrGj/m2ranluzwNysXB2vuGAwn2B6PcHnHIPD0eK9a6nI3XonmUSNoXtvDt30Y6I0hFxbP6P3cmi8flq8OxaSOiNgBH8iLxhT0CCkf/8gpLSrKSoXQSCoA6R11JynVwWgoMl0a7TfKq0dr/qi8url5fGwhB5/CP1lzWasq";
const char strIconMeter3[] PROGMEM = "+j/mnZ9bA0eZqFOz+RdFnCrqNKRqzAAAAYRpQ0NQSUNDIHByb2ZpbGUAAHicfZE9SMNAHMVfU6WiFQcLiigEqU4WREUctQpFqBBqhVYdTK6f0KQhSXFxFFwLDn4sVh1cnHV1cBUEwQ8QF1cnRRcp8X9JoUWMB8f9eHfvcfcOEGolpppt44CqWUYiFhVT6VUx8IouDKEPAoZlZupzkhSH5/i6h4+vdxGe5X3uz9GdyZoM8InEs0w3LOIN4ulNS+e8TxxiBTlDfE48ZtAFiR+5rrj8xjnvsMAzQ0YyMU8cIhbzLay0MCsYKvEUcTijapQvpFzOcN7irJYqrHFP/sJgVltZ5jrNQcSwiCVIEKGggiJKsBChVSPFRIL2ox7+AccvkUshVxGMHAsoQ4Xs+MH/4He3Zm5ywk0KRoH2F9v+GAECu0C9atvfx7ZdPwH8z8CV1vSXa8DMJ+nVphY+Anq2gYvrpqbsAZc7QP+TLhuyI/lpCrkc8H5G35QGem+BzjW3t8Y+Th+AJHUVvwEODoHRPGWve7y7o7W3f880+vsBX1Vyn9LWiKcAAAAGYktHRAD/AP8A/6C9p5MAAAAJcEhZcwAALiMAAC4jAXilP3YAAAAHdElNRQfjCx0XFCSCY/uuAAAE+UlEQVRYw62XTWwUZRjH/887s2Dlo90tB0ktu/2yCQck0YOJRinZCGmCxnDwIoFAggeCxIvBEC+CmAAXhXiAEG8mHAwHJUZMIAHx";
const char strIconMeter4[] PROGMEM = "oiYYG7/Y7ldpIKTM7BbiLnTn/XvYmeHd6WyXNbyX9um8fX/vPM//+RjJZjfZeLQIQCI2ACj/dwLAjaJzDuB2+k+l+R9fj2X6347b79vaP5umrbqFN20yAjfXY8ODh13BFxY8AFhlwtncsaLhaasLOAGIvQRczMPyM+6rnqf3gNhGYHULvLl7a2HGvQPgghI5PZJO/dQBrgBoidEAjAsxX3bHPa1PAthsuj0Et7EBXLYttX9oMPlXNxpogedKzk5P61+6hZMAiYmFhv45V3J2Gl6mEdZFGmiFF533SZ4F0NMt3LB7tObZXNE5EIEvUmzLZXIlZxfB42aM/wc8tDV5wvDEopRpUXt+xh0nefJJwUNb81Rhxh2PXsCOpprn6VMRt7sickgJ8gSgRBRJQpqMGFtIgkBGk0dFkCSb4Wh4+nMAW9oWjXzZfQ3AhPnmAjk2mk6dJrCSxKdjmf6LoqSsyU+eWm5NWZZc1+RhESk/N9R/ydP6CICVo5nUGaXkmA8PPLJ5uuy80lYDnta7o24nuBYALCW3AV4hCSW4LyJXSdQtSx4CuJKwVa2ZWvIjgFkAiuQzwTlB3dCae2FGJ5vdZAUVrnizMhcUmdYYynmAc49CKqBRgqK2n3FJANtNuL+qmYG+NYmEJQAYaqB8q7ohHg6QfMu0tbEhziaJ6JsHyU6g9+bt+eeHBpO/tWiAxPonona2/jQTj/4eT3N98KdQA+I3mCcBb3lu9IzQG8TT";
const char strIconMeter5[] PROGMEM = "QWEK+oBi0wvN3LTU5PC65MWIWGNree3BgvQsT8Bssca+cE2XnazW/M73hATeV0YY7hsxHXgcOADM3rp3qBMcAKgxEHhCCf4N9oQaEMEfjxoJNzwOfLrk7NPknk5w/603BmEQwdQiDaQH+qYAVP1bTiolSw4T+bK7ydM8YYi8LdyyFAFO+hqoDq7t/T04M+wFtqU0gAt+GEZuFO5OtIOXZytDDU+fE4ENoDdXdI7fKDqH3GrNjrvAP4W5LIBhX2MXEglLB2eaGqASOROEQZNHG56ODcODBe8jERRAPIRglSb3KoXLyd6eRhTuaQ2teSQQuBI5HR1Iwl4wkk5dA3DZD8MLxZuVD+OGibFM/56xTP9LENwhQaVk18i61LW4t8+XKwdJvOh79lJ0VIvOA9q21H4ANRFAa36cKzk72g0TIKCU7BtNp87HwXMlZwfJw75ZW2ZbB6LetIaHM8p0SbK3Z86dr98i8YbviTfd+Xpj9arlV5VqLeputV4bzaTOLHK7pzFddg+S/CwUopJ9Q+uSP0R1ZQ0PZ+zorVJ9Pdfdan2ewOu+JiYq8/XJSrVeWJNaUQgqVqqv59eo2nOlu1mnWvuKxDthIVHywWg69UWMqBF0w1i150rOTpLhgOKnUR7AtyIyJcAMpFlkCGwEOGmoHQBqlpL3RtKpLyO1Qpvt2F5ibmdhxh33J5nNka7W0ukigwdEcClhWwcyz/b92Q5ufpjEwgFgaDD5N4At02Xn";
const char strIconMeter6[] PROGMEM = "Za35LoBJAr1xcABVkY4fJiEcgLaXgpvLT7NrDxc8lGYr3wDYGoF/nxno22YWmU5wdNgYu5Y1J5n7MW6/1y08KEQS03jarfCjMhrzDs0rFg5A/gPDrRMdK7g0VQAAAABJRU5ErkJggg==";
const char *const arrIconMeterBase64[] PROGMEM = {strIconMeter0, strIconMeter1, strIconMeter2, strIconMeter3, strIconMeter4, strIconMeter5, strIconMeter6};

// fil pilote modes
enum PilotWireModes { PILOTWIRE_DISABLED, PILOTWIRE_OFF, PILOTWIRE_COMFORT, PILOTWIRE_ECO, PILOTWIRE_FROST, PILOTWIRE_THERMOSTAT, PILOTWIRE_OFFLOAD };

// fil pilote commands
enum PilotWireCommands { CMND_PILOTWIRE_MODE, CMND_PILOTWIRE_OFFLOAD, CMND_PILOTWIRE_MIN, CMND_PILOTWIRE_MAX, CMND_PILOTWIRE_TARGET, CMND_PILOTWIRE_DRIFT, CMND_PILOTWIRE_POWER, CMND_PILOTWIRE_PRIORITY, CMND_PILOTWIRE_CONTRACT, CMND_PILOTWIRE_MQTTTOPIC, CMND_PILOTWIRE_JSONKEY };
const char kPilotWireCommands[] PROGMEM = D_CMND_PILOTWIRE_MODE "|" D_CMND_PILOTWIRE_OFFLOAD "|" D_CMND_PILOTWIRE_MIN "|" D_CMND_PILOTWIRE_MAX "|" D_CMND_PILOTWIRE_TARGET "|" D_CMND_PILOTWIRE_DRIFT "|" D_CMND_PILOTWIRE_POWER "|" D_CMND_PILOTWIRE_PRIORITY "|" D_CMND_PILOTWIRE_CONTRACT "|" D_CMND_PILOTWIRE_MQTTTOPIC "|" D_CMND_PILOTWIRE_JSONKEY;

// header of publicly accessible control page
const char INPUT_HEAD_CONTROL[] PROGMEM = "<div style='text-align:left;display:inline-block;min-width:340px;'><div style='text-align:center;'><noscript>" D_NOSCRIPT "<br/></noscript><h2>%s</h2><h2 style='color:blue;'>%s °C</h2><h2 style='color:green;'>%s</h2></div>";
const char INPUT_MODE_SELECT[] PROGMEM = "<input type='radio' name='%s' id='%d' value='%d' %s>%s";
const char INPUT_FORM_START[] PROGMEM = "<form method='get' action='%s'>";
const char INPUT_FORM_STOP[] PROGMEM = "</form>";
const char INPUT_FIELDSET_START[] PROGMEM = "<fieldset><legend><b>&nbsp;%s&nbsp;</b></legend><br />";
const char INPUT_FIELDSET_STOP[] PROGMEM = "</fieldset><br />";

/**************************************************\
 *                  Accessors
\**************************************************/

// get label according to state
char* PilotwireGetStateLabel (uint8_t state)
{
  char* label = NULL;
    
  // get label
  switch (state)
  {
   case PILOTWIRE_DISABLED:         // Disabled
     label = D_PILOTWIRE_DISABLED;
     break;
   case PILOTWIRE_OFF:              // Off
     label = D_PILOTWIRE_OFF;
     break;
   case PILOTWIRE_COMFORT:          // Comfort
     label = D_PILOTWIRE_COMFORT;
     break;
   case PILOTWIRE_ECO:              // Economy
     label = D_PILOTWIRE_ECO;
     break;
   case PILOTWIRE_FROST:            // No frost
     label = D_PILOTWIRE_FROST;
     break;
   case PILOTWIRE_THERMOSTAT:       // Thermostat
     label = D_PILOTWIRE_THERMOSTAT;
     break;
   case PILOTWIRE_OFFLOAD:          // Offloaded
     label = D_PILOTWIRE_OFFLOAD;
     break;
  }
  
  return label;
}

// get pilot wire state from relays state
uint8_t PilotwireGetRelayState ()
{
  uint8_t relay1 = 0;
  uint8_t relay2 = 0;
  uint8_t state  = 0;
  
  // read relay states
  relay1 = bitRead (power, 0);
  if (devices_present > 1) relay2 = bitRead (power, 1);

  // convert to pilotwire state
  if (relay1 == 0 && relay2 == 0) state = PILOTWIRE_COMFORT;
  else if (relay1 == 0 && relay2 == 1) state = PILOTWIRE_OFF;
  else if (relay1 == 1 && relay2 == 0) state = PILOTWIRE_FROST;
  else if (relay1 == 1 && relay2 == 1) state = PILOTWIRE_ECO;

  // if one relay device, convert no frost to off
  if (devices_present == 1)
  {
    if (state == PILOTWIRE_FROST) state = PILOTWIRE_OFF;
    if (state == PILOTWIRE_ECO)   state = PILOTWIRE_COMFORT;
  }
  
  return state;
}

// set relays state
void PilotwireSetRelayState (uint8_t new_state)
{
  // handle 1 relay device state conversion
  if (devices_present == 1)
  {
    if (new_state == PILOTWIRE_ECO) new_state = PILOTWIRE_COMFORT;
    else if (new_state == PILOTWIRE_OFF) new_state = PILOTWIRE_FROST;
  }

  // pilot relays
  switch (new_state)
  {
    case PILOTWIRE_OFF:  // Set Off
      ExecuteCommandPower (1, POWER_OFF, SRC_IGNORE);
      if (devices_present > 1) ExecuteCommandPower (2, POWER_ON, SRC_IGNORE);
      break;
    case PILOTWIRE_COMFORT:  // Set Comfort
      ExecuteCommandPower (1, POWER_OFF, SRC_IGNORE);
      if (devices_present > 1) ExecuteCommandPower (2, POWER_OFF, SRC_IGNORE);
      break;
    case PILOTWIRE_ECO:  // Set Economy
      ExecuteCommandPower (1, POWER_ON, SRC_IGNORE);
      if (devices_present > 1) ExecuteCommandPower (2, POWER_ON, SRC_IGNORE);
      break;
    case PILOTWIRE_FROST:  // Set No Frost
      ExecuteCommandPower (1, POWER_ON, SRC_IGNORE);
      if (devices_present > 1) ExecuteCommandPower (2, POWER_OFF, SRC_IGNORE);
      break;
  }
}

// get pilot actual mode
uint8_t PilotwireGetMode ()
{
  uint8_t actual_mode;

  // read actual VMC mode
  actual_mode = (uint8_t) Settings.weight_reference;

  // if outvalue, set to disabled
  if (actual_mode > PILOTWIRE_OFFLOAD) actual_mode = PILOTWIRE_DISABLED;

  return actual_mode;
}

// set pilot wire mode
void PilotwireSetMode (uint8_t new_mode)
{
  // reset offloading
  pilotwire_offloaded = false;

  // handle 1 relay device state conversion
  if (devices_present == 1)
  {
    if (new_mode == PILOTWIRE_ECO) new_mode = PILOTWIRE_COMFORT;
    else if (new_mode == PILOTWIRE_FROST) new_mode = PILOTWIRE_OFF;
  }

  // if within range, set mode
  if (new_mode <= PILOTWIRE_OFFLOAD) Settings.weight_reference = (unsigned long) new_mode;
}

// set pilot wire offload mode
void PilotwireSetOffload (char* offload)
{
  // detect offload mode on
  if (strcmp (offload, "1") == 0) pilotwire_offloaded = true;
  else if (strcmp (offload, "ON") == 0) pilotwire_offloaded = true;

  // detect offload mode off
  else if (strcmp (offload, "0") == 0) pilotwire_offloaded = false;
  else if (strcmp (offload, "OFF") == 0) pilotwire_offloaded = false;
}

// set pilot wire minimum temperature
void PilotwireSetMinTemperature (float new_temperature)
{
  // if within range, save temperature correction
  if ((new_temperature >= PILOTWIRE_TEMP_MIN) && (new_temperature <= PILOTWIRE_TEMP_MAX)) Settings.weight_item = (unsigned long) (new_temperature * 10);
}

// get pilot wire minimum temperature
float PilotwireGetMinTemperature ()
{
  float min_temperature;

  // get drift temperature (/10)
  min_temperature = (float) Settings.weight_item;
  min_temperature = min_temperature / 10;
  
  // check if within range
  if (min_temperature < PILOTWIRE_TEMP_MIN) min_temperature = PILOTWIRE_TEMP_MIN;
  if (min_temperature > PILOTWIRE_TEMP_MAX) min_temperature = PILOTWIRE_TEMP_MAX;

  return min_temperature;
}

// set pilot wire maximum temperature
void PilotwireSetMaxTemperature (float new_temperature)
{
  // if within range, save temperature correction
  if ((new_temperature >= PILOTWIRE_TEMP_MIN) && (new_temperature <= PILOTWIRE_TEMP_MAX)) Settings.energy_frequency_calibration = (unsigned long) (new_temperature * 10);
}

// get pilot wire maximum temperature
float PilotwireGetMaxTemperature ()
{
  float max_temperature;

  // get drift temperature (/10)
  max_temperature = (float) Settings.energy_frequency_calibration;
  max_temperature = max_temperature / 10;
  
  // check if within range
  if (max_temperature < PILOTWIRE_TEMP_MIN) max_temperature = PILOTWIRE_TEMP_MIN;
  if (max_temperature > PILOTWIRE_TEMP_MAX) max_temperature = PILOTWIRE_TEMP_MAX;

  return max_temperature;
}

// set pilot wire drift temperature
void PilotwireSetDrift (float new_drift)
{
  // if within range, save temperature correction
  if ((new_drift >= PILOTWIRE_DRIFT_MIN) && (new_drift <= PILOTWIRE_DRIFT_MAX)) Settings.weight_calibration = (unsigned long) (50 + (new_drift * 10));
}

// get pilot wire drift temperature
float PilotwireGetDrift ()
{
  float drift;

  // get drift temperature (/10)
  drift = (float) Settings.weight_calibration;
  drift = ((drift - 50) / 10);
  
  // check if within range
  if (drift < PILOTWIRE_DRIFT_MIN) drift = PILOTWIRE_DRIFT_MIN;
  if (drift > PILOTWIRE_DRIFT_MAX) drift = PILOTWIRE_DRIFT_MAX;

  return drift;
}

// get current temperature
float PilotwireGetTemperature ()
{
  float temperature;
  
  // get global temperature
  temperature = global_temperature;

  // if global temperature not defined and ds18b20 sensor present, read it 
#ifdef USE_DS18B20
  if ((temperature == 0) && (ds18b20_temperature != 0)) temperature = ds18b20_temperature;
#endif

  // if global temperature not defined and dht sensor present, read it 
#ifdef USE_DHT
  if ((temperature == 0) && (Dht[0].t != 0)) temperature = Dht[0].t;
#endif

  return temperature;
}

// get current temperature with drift correction
float PilotwireGetTemperatureWithDrift ()
{
  float temperature;
  
  // get current temperature adding drift correction
  temperature = PilotwireGetTemperature () + PilotwireGetDrift ();

  return temperature;
}

// set pilot wire in thermostat mode
void PilotwireSetThermostat (float new_thermostat)
{
  // save target temperature
  if ((new_thermostat >= PILOTWIRE_TEMP_MIN) && (new_thermostat <= PILOTWIRE_TEMP_MAX)) Settings.weight_max = (uint16_t) (new_thermostat * 10);
}

// get target temperature
float PilotwireGetTargetTemperature ()
{
  float temperature;

  // get target temperature (/10)
  temperature = (float) Settings.weight_max;
  temperature = temperature / 10;
  
  // check if within range
  if (temperature < PILOTWIRE_TEMP_MIN) temperature = PILOTWIRE_TEMP_MIN;
  if (temperature > PILOTWIRE_TEMP_MAX) temperature = PILOTWIRE_TEMP_MAX;

  return temperature;
}

/******************************************************\
 *                         Functions
\******************************************************/

// Show JSON status (for MQTT)
void PilotwireShowJSON (bool append)
{
  float   drift_temperature;
  float   actual_temperature, target_temperature, min_temperature, max_temperature;
  uint8_t actual_mode;
  char*   actual_label;

  // start message  -->  {  or  ,
  if (append == false) snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("{"));
  else snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,"), mqtt_data);

  // get mode and temperature
  actual_mode  = PilotwireGetMode ();
  actual_label = PilotwireGetStateLabel (actual_mode);
  drift_temperature  = PilotwireGetDrift ();
  actual_temperature = PilotwireGetTemperatureWithDrift ();
  target_temperature = PilotwireGetTargetTemperature ();
  min_temperature = PilotwireGetMinTemperature ();
  max_temperature = PilotwireGetMaxTemperature ();

  // pilotwire mode  -->  "PilotWire":{"Relay":2,"Mode":5,"Label":"Thermostat",Offload:"ON","Temperature":20.5,"Target":21,"Min"=15,"Max"=25}
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_PILOTWIRE "\":{"), mqtt_data);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_PILOTWIRE_RELAY "\":%d"), mqtt_data, devices_present);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_MODE "\":%d"), mqtt_data, actual_mode);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_LABEL "\":\"%s\""), mqtt_data, actual_label);
  if (pilotwire_offloaded == true) snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_OFFLOAD "\":\"%s\""), mqtt_data, "ON");
  else snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_OFFLOAD "\":\"%s\""), mqtt_data, "OFF");
  if (actual_temperature != 0) snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_TEMPERATURE "\":%.1f"), mqtt_data, actual_temperature);
  if (actual_mode == PILOTWIRE_THERMOSTAT)
  {
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_TARGET "\":%.1f"), mqtt_data, target_temperature);
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_DRIFT "\":%.1f"), mqtt_data, drift_temperature);
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_MIN "\":%.1f"), mqtt_data, min_temperature);
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_MAX "\":%.1f"), mqtt_data, max_temperature);
  }
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);

  // if pilot wire mode is enabled
  if (actual_mode != PILOTWIRE_DISABLED)
  {
    // relay state  -->  ,"Relay":{"Mode":4,"Label":"Comfort","Number":number}
    actual_mode  = PilotwireGetRelayState ();
    actual_label = PilotwireGetStateLabel (actual_mode);
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_STATE "\":{"), mqtt_data);
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_PILOTWIRE_MODE "\":%d"), mqtt_data, actual_mode);
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_LABEL "\":\"%s\""), mqtt_data, actual_label);
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

// Handle pilot wire MQTT commands
bool PilotwireCommand ()
{
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
    case CMND_PILOTWIRE_OFFLOAD:  // set offloading
      PilotwireSetOffload (XdrvMailbox.data);
      break;
    case CMND_PILOTWIRE_TARGET:  // set target temperature 
      PilotwireSetThermostat (atof (XdrvMailbox.data));
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
  }

  // send MQTT status
  PilotwireShowJSON (false);
  
  return true;
}

// update pilot wire relay states according to current status
void PilotwireEvery250MSecond ()
{
  float    actual_temperature, target_temperature;
  uint8_t  target_mode, heater_state, target_state;
  uint8_t  heater_priority;
  uint16_t house_contract, heater_power;
  ulong    time_now;

  // get house contract heater power and heater priority
  heater_priority = PilotwireMqttGetPriority ();
  house_contract = PilotwireMqttGetContract ();
  heater_power   = PilotwireGetHeaterPower ();

  // if contract and heater power are defined
  if ((house_contract > 0) && (heater_power > 0))
  {
    // if house power is too high, heater should be offloaded
    if (pilotwire_house_power > house_contract)
    {
      // set offload status
      pilotwire_offloaded   = true;
      pilotwire_meter_count = 0;
    }

    // else if heater is candidate to remove offload
    else if (pilotwire_meter_count >= heater_priority) pilotwire_offloaded = false;
  }

  // get target mode
  target_mode = PilotwireGetMode ();

  // if pilotwire mode is enabled
  if (target_mode != PILOTWIRE_DISABLED)
  {
    // if offload mode, target state is off
    if (pilotwire_offloaded == true) target_state = PILOTWIRE_OFF;
 
    // else if thermostat mode
    else if (target_mode == PILOTWIRE_THERMOSTAT)
    {
      // get current and target temperature
      actual_temperature = PilotwireGetTemperatureWithDrift ();
      target_temperature = PilotwireGetTargetTemperature ();

      // if temperature is too low, target state is on
      // else, if too high, target state is off
      target_state = target_mode;
      if (actual_temperature < (target_temperature - PILOTWIRE_TEMP_THRESHOLD)) target_state = PILOTWIRE_COMFORT;
      else if (actual_temperature > (target_temperature + PILOTWIRE_TEMP_THRESHOLD)) target_state = PILOTWIRE_OFF;
    }

    // else set mode if needed
    else target_state = target_mode;

    // get heater status
    heater_state = PilotwireGetRelayState ();

    // if heater state different than target state, change state
    if (heater_state != target_state)
    {
      // set relays
      PilotwireSetRelayState (target_state);

      // publish new state
      PilotwireShowJSON (false);
    }
  }
}

/*******************************************************\
 *                      Web server
\*******************************************************/

#ifdef USE_WEBSERVER

// Pilot Wire heater icon
void PilotwireWebDisplayIconHeater (uint8_t height)
{
  uint8_t nbrItem, index;

  // display img according to height
  WSContentSend_P ("<img height=%d src='data:image/png;base64,", height);

  // loop to display base64 segments
  nbrItem = sizeof (arrIconHeaterBase64) / sizeof (char*);
  for (index=0; index<nbrItem; index++) { WSContentSend_P (arrIconHeaterBase64[index]); }

  WSContentSend_P ("'/>");
}

// Pilot Wire meter icon
void PilotwireWebDisplayIconMeter (uint8_t height)
{
  uint8_t nbrItem, index;

  // display img according to height
  WSContentSend_P ("<img height=%d src='data:image/png;base64,", height);

  // loop to display base64 segments
  nbrItem = sizeof (arrIconMeterBase64) / sizeof (char*);
  for (index=0; index<nbrItem; index++) { WSContentSend_P (arrIconMeterBase64[index]); }

  WSContentSend_P ("'/>");
}

// Pilot Wire mode selection 
void PilotwireWebSelectMode (bool public_mode)
{
  uint8_t actual_mode;
  bool    temp_notavailable;
  float   actual_temperature, min_temperature, max_temperature, target_temperature;
  char    argument[PILOTWIRE_LABEL_BUFFER_SIZE];

  // get mode and temperature
  actual_mode  = PilotwireGetMode ();
  actual_temperature = PilotwireGetTemperature ();
  temp_notavailable = isnan (actual_temperature);

  // selection : disabled
  if (public_mode == false)
  {
    if (actual_mode == PILOTWIRE_DISABLED) strcpy (argument, "checked"); 
    else strcpy (argument, "");
    WSContentSend_P (INPUT_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_DISABLED, PILOTWIRE_DISABLED, argument, D_PILOTWIRE_DISABLED);
    WSContentSend_P (PSTR ("<br/>"));
  }

  // selection : off
  if (actual_mode == PILOTWIRE_OFF) strcpy (argument, "checked");
  else strcpy (argument, "");
  WSContentSend_P (INPUT_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_OFF, PILOTWIRE_OFF, argument, D_PILOTWIRE_OFF);
  WSContentSend_P (PSTR ("<br/>"));

  // if dual relay device
  if ((public_mode == false) && (devices_present > 1))
  {
    // selection : no frost
    if (actual_mode == PILOTWIRE_FROST) strcpy (argument, "checked");
    else strcpy (argument, "");
    WSContentSend_P (INPUT_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_FROST, PILOTWIRE_FROST, argument, D_PILOTWIRE_FROST);
    WSContentSend_P (PSTR ("<br/>"));

    // selection : eco
    if (actual_mode == PILOTWIRE_ECO) strcpy (argument, "checked");
    else strcpy (argument, "");
    WSContentSend_P (INPUT_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_ECO, PILOTWIRE_ECO, argument, D_PILOTWIRE_ECO);
    WSContentSend_P (PSTR ("<br/>"));
  }

  // selection : comfort
  if ((public_mode == false) || (temp_notavailable == true))
  {
    if (actual_mode == PILOTWIRE_COMFORT) strcpy (argument, "checked");
    else strcpy (argument, "");
    WSContentSend_P (INPUT_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_COMFORT, PILOTWIRE_COMFORT, argument, D_PILOTWIRE_COMFORT);
    WSContentSend_P (PSTR ("<br/>"));
  }

  // selection : thermostat
  if (temp_notavailable == false) 
  {
    if (actual_mode == PILOTWIRE_THERMOSTAT) strcpy (argument, "checked");
    else strcpy (argument, "");
    WSContentSend_P (INPUT_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_THERMOSTAT, PILOTWIRE_THERMOSTAT, argument, D_PILOTWIRE_THERMOSTAT);

    // get temperatures
    min_temperature    = PilotwireGetMinTemperature ();
    max_temperature    = PilotwireGetMaxTemperature ();
    target_temperature = PilotwireGetTargetTemperature ();

    // set temperature tooltip
    sprintf (argument, D_PILOTWIRE_TEMP_TITLE, min_temperature, max_temperature);

    // selection : target temperature
    WSContentSend_P (PSTR ("<input type='number' name='%s' min='%.2f' max='%.2f' step='%.2f' value='%.2f' title='%s'><br/>"), D_CMND_PILOTWIRE_TARGET, min_temperature, max_temperature, PILOTWIRE_TEMP_STEP, target_temperature, argument);
  }
}

// Pilot Wire configuration button
void PilotwireWebButton ()
{
  // heater icon and configuration button
  WSContentSend_P (PSTR ("<table style='width:100%%;'><tr><td align='center' style='width:20%%;'>"));
  PilotwireWebDisplayIconHeater (32);
  WSContentSend_P (PSTR ("</td><td><form action='%s' method='get'><button>%s</button></form></td></tr></table>"), D_PAGE_PILOTWIRE_HEATER, D_PILOTWIRE_CONF_HEATER);

  // meter icon and configuration button
  WSContentSend_P (PSTR ("<table style='width:100%%;'><tr><td align='center' style='width:20%%;'>"));
  PilotwireWebDisplayIconMeter (32);
  WSContentSend_P (PSTR ("</td><td><form action='%s' method='get'><button>%s</button></form></td></tr></table>"), D_PAGE_PILOTWIRE_METER, D_PILOTWIRE_CONF_METER);
}

// Pilot Wire heater public configuration web page
void PilotwireWebPageControl ()
{
  float   target_temperature, actual_temperature, min_temperature, max_temperature;
  uint8_t relay_state;
  char*   relay_state_label;
  char    argument[PILOTWIRE_BUFFER_SIZE];

  // page comes from save button on configuration page
  if (WebServer->hasArg("save"))
  {
    // get pilot wire mode according to 'mode' parameter
    WebGetArg (D_CMND_PILOTWIRE_MODE, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetMode ((uint8_t)atoi (argument)); 

    // get target temperature according to 'target' parameter
    WebGetArg (D_CMND_PILOTWIRE_TARGET, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetThermostat (atof (argument));
  }

  // get current temperature
  actual_temperature = PilotwireGetTemperatureWithDrift ();
  dtostrf(actual_temperature, PILOTWIRE_BUFFER_SIZE, 1, argument);

  // actuel relay state
  relay_state = PilotwireGetRelayState ();
  relay_state_label = PilotwireGetStateLabel (relay_state);

  // beginning of form without authentification
  WSContentStart_P (D_PILOTWIRE_PARAM_CONTROL, false);

  // beginning of page
  WSContentSend_P(HTTP_HEAD_STYLE1);
  WSContentSend_P(HTTP_HEAD_STYLE2);
  WSContentSend_P(PSTR ("</style></head>"));
  
  WSContentSend_P(PSTR ("<body>"));
  WSContentSend_P(INPUT_HEAD_CONTROL, Settings.friendlyname[0], argument, relay_state_label);
  
  WSContentSend_P (INPUT_FORM_START, D_PAGE_PILOTWIRE_CONTROL);
 
  // mode selection with only public choices
  WSContentSend_P (INPUT_FIELDSET_START, D_PILOTWIRE_MODE);
  PilotwireWebSelectMode (true);

  // end of page
  WSContentSend_P (INPUT_FIELDSET_STOP);
  WSContentSend_P("<button name='save' type='submit' class='button bgrn'>%s</button>", D_SAVE);
  WSContentSend_P(INPUT_FORM_STOP);
  WSContentStop();
}

// Pilot Wire heater configuration web page
void PilotwireWebPageHeater ()
{
  uint8_t  target_mode;
  uint8_t  priority_heater;
  bool     temp_notavailable;
  float    drift_temperature, actual_temperature, target_temperature, min_temperature, max_temperature;
  char     argument[PILOTWIRE_BUFFER_SIZE];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // get temperature and target mode
  target_mode = PilotwireGetMode ();
  actual_temperature = PilotwireGetTemperature ();
  temp_notavailable = isnan (actual_temperature);

  // page comes from save button on configuration page
  if (WebServer->hasArg("save"))
  {
    // get pilot wire mode according to 'mode' parameter
    WebGetArg (D_CMND_PILOTWIRE_MODE, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetMode ((uint8_t)atoi (argument)); 

    // get minimum temperature according to 'min' parameter
    WebGetArg (D_CMND_PILOTWIRE_MIN, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetMinTemperature (atof (argument));

    // get maximum temperature according to 'max' parameter
    WebGetArg (D_CMND_PILOTWIRE_MAX, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetMaxTemperature (atof (argument));

    // get target temperature according to 'target' parameter
    WebGetArg (D_CMND_PILOTWIRE_TARGET, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetThermostat (atof (argument));

    // get temperature drift according to 'drift' parameter
    WebGetArg (D_CMND_PILOTWIRE_DRIFT, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetDrift (atof (argument));
  }

  // beginning of form
  WSContentStart_P (D_PILOTWIRE_CONF_HEATER);
  WSContentSendStyle ();
  WSContentSend_P (INPUT_FORM_START, D_PAGE_PILOTWIRE_HEATER);

  // mode selection
  WSContentSend_P (INPUT_FIELDSET_START, D_PILOTWIRE_MODE);
  PilotwireWebSelectMode (false);

  // if temperature sensor is present
  if (temp_notavailable == false) 
  {
    // temperature correction label and input
    drift_temperature = PilotwireGetDrift ();
    WSContentSend_P (PSTR ("<br/>"));
    WSContentSend_P (PSTR ("%s<br/>"), D_PILOTWIRE_DRIFT);
    WSContentSend_P (PSTR ("<input type='number' name='%s' min='%d' max='%d' step='%.2f' value='%.2f'><br/>"), D_CMND_PILOTWIRE_DRIFT, PILOTWIRE_DRIFT_MIN, PILOTWIRE_DRIFT_MAX, PILOTWIRE_DRIFT_STEP, drift_temperature);

    // temperature minimum label and input
    min_temperature = PilotwireGetMinTemperature ();
    WSContentSend_P (PSTR ("<br/>"));
    WSContentSend_P (PSTR ("%s<br/>"), D_PILOTWIRE_MIN);
    WSContentSend_P (PSTR ("<input type='number' name='%s' min='%d' max='%d' step='%.2f' value='%.2f'><br/>"), D_CMND_PILOTWIRE_MIN, PILOTWIRE_TEMP_MIN, PILOTWIRE_TEMP_MAX, PILOTWIRE_TEMP_STEP, min_temperature);

    // temperature maximum label and input
    max_temperature = PilotwireGetMaxTemperature ();
    WSContentSend_P (PSTR ("<br/>"));
    WSContentSend_P (PSTR ("%s<br/>"), D_PILOTWIRE_MAX);
    WSContentSend_P (PSTR ("<input type='number' name='%s' min='%d' max='%d' step='%.2f' value='%.2f'><br/>"), D_CMND_PILOTWIRE_MAX, PILOTWIRE_TEMP_MIN, PILOTWIRE_TEMP_MAX, PILOTWIRE_TEMP_STEP, max_temperature);
  }

  // end of form
  WSContentSend_P (INPUT_FIELDSET_STOP);
  WSContentSend_P("<button name='save' type='submit' class='button bgrn'>%s</button>", D_SAVE);
  WSContentSend_P(INPUT_FORM_STOP);

  // configuration button
  WSContentSpaceButton(BUTTON_CONFIGURATION);

  // end of page
  WSContentStop();
}

// Pilot Wire web page
void PilotwireWebPageMeter ()
{
  uint8_t  priority_heater;
  uint16_t power_heater, power_limit;
  char     argument[PILOTWIRE_BUFFER_SIZE];
  char*    power_topic;
  char*    json_key;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (WebServer->hasArg("save"))
  {
    // get power of heater according to 'power' parameter
    WebGetArg (D_CMND_PILOTWIRE_POWER, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetHeaterPower ((uint16_t)atoi (argument));

    // get priority of heater according to 'priority' parameter
    WebGetArg (D_CMND_PILOTWIRE_PRIORITY, argument, PILOTWIRE_BUFFER_SIZE);
    PilotwireMqttSetPriority ((uint8_t)atoi (argument));

    // get maximum power limit according to 'contract' parameter
    WebGetArg (D_CMND_PILOTWIRE_CONTRACT, argument, PILOTWIRE_BUFFER_SIZE);
    PilotwireMqttSetContract ((uint16_t)atoi (argument));

    // get MQTT topic according to 'topic' parameter
    WebGetArg (D_CMND_PILOTWIRE_MQTTTOPIC, argument, PILOTWIRE_BUFFER_SIZE);
    PilotwireMqttSetTopic (argument);

    // get JSON key according to 'key' parameter
    WebGetArg (D_CMND_PILOTWIRE_JSONKEY, argument, PILOTWIRE_BUFFER_SIZE);
    PilotwireMqttSetJsonKey (argument);
  }

  // beginning of form
  WSContentStart_P (D_PILOTWIRE_CONF_METER);
  WSContentSendStyle ();
  WSContentSend_P (INPUT_FORM_START, D_PAGE_PILOTWIRE_METER);

  // heater section  
  WSContentSend_P (INPUT_FIELDSET_START, D_PILOTWIRE_HEATER);

  // power of heater label and input
  power_heater = PilotwireGetHeaterPower ();
  WSContentSend_P (PSTR ("%s<br/>"), D_PILOTWIRE_POWER);
  WSContentSend_P (PSTR ("<input type='number' name='%s' value='%d'><br/>"), D_CMND_PILOTWIRE_POWER, power_heater);

  // priority of heater label and input
  priority_heater  = PilotwireMqttGetPriority ();
  WSContentSend_P (PSTR ("<br/>"));
  WSContentSend_P (PSTR ("%s<br/>"), D_PILOTWIRE_PRIORITY);
  WSContentSend_P (PSTR ("<input type='number' name='%s' min='1' max='5' step='1' value='%d'><br/>"), D_CMND_PILOTWIRE_PRIORITY, priority_heater);

    // house section
  WSContentSend_P (INPUT_FIELDSET_STOP);
  WSContentSend_P (INPUT_FIELDSET_START, D_PILOTWIRE_HOUSE);

  // contract power limit label and input
  power_limit = PilotwireMqttGetContract ();
  WSContentSend_P (PSTR ("%s<br/>"), D_PILOTWIRE_CONTRACT);
  WSContentSend_P (PSTR ("<input type='number' name='%s' value='%d'><br/>"), D_CMND_PILOTWIRE_CONTRACT, power_limit);

  // power mqtt topic label and input
  power_topic = PilotwireMqttGetTopic ();
  if (power_topic == NULL) strcpy (argument, "");
  else strcpy (argument, power_topic);
  WSContentSend_P (PSTR ("<br/>"));
  WSContentSend_P (PSTR ("%s<br/>"), D_PILOTWIRE_MQTTTOPIC);
  WSContentSend_P (PSTR ("<input name='%s' value='%s'><br/>"), D_CMND_PILOTWIRE_MQTTTOPIC, argument);

  // power json key label and input
  json_key = PilotwireMqttGetJsonKey ();
  if (json_key == NULL) strcpy (argument, "");
  else strcpy (argument, json_key);
  WSContentSend_P (PSTR ("<br/>"));
  WSContentSend_P (PSTR ("%s<br/>"), D_PILOTWIRE_JSONKEY);
  WSContentSend_P (PSTR ("<input name='%s' value='%s'><br/>"), D_CMND_PILOTWIRE_JSONKEY, argument);

  // end of form
  WSContentSend_P (INPUT_FIELDSET_STOP);
  WSContentSend_P("<button name='save' type='submit' class='button bgrn'>%s</button>", D_SAVE);
  WSContentSend_P(INPUT_FORM_STOP);

  // configuration button
  WSContentSpaceButton(BUTTON_CONFIGURATION);

  // end of page
  WSContentStop();
}

// append pilot wire state to main page
bool PilotwireWebState ()
{
  uint8_t  actual_mode, actual_state;
  uint16_t contract_power;
  uint32_t current_time;
  float    corrected_temperature, target_temperature;
  char     state_color[PILOTWIRE_COLOR_BUFFER_SIZE];
  char*    actual_label;
  TIME_T   current_dst;

  // if pilot wire is in thermostat mode, display target temperature
  actual_mode  = PilotwireGetMode ();
  if (actual_mode == PILOTWIRE_THERMOSTAT)
  {
    // read temperature
    corrected_temperature = PilotwireGetTemperatureWithDrift ();
    target_temperature = PilotwireGetTargetTemperature ();

    // add it to JSON
    snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><th>%s</th><td><font color='blue'>%.1f</font> / %.1f°C</td></tr>", mqtt_data, D_PILOTWIRE_BOTHTEMP, corrected_temperature, target_temperature);
  }

  // if house power is subscribed, display power
  if (pilotwire_topic_subscribed == true)
  {
    contract_power = PilotwireMqttGetContract ();
    if (pilotwire_house_power >= contract_power) strcpy (state_color, "red");
    else strcpy (state_color, "blue");
    snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><td><b>%s</b></td><td><font color='%s'>%d</font> / %dW</td></tr>", mqtt_data, D_PILOTWIRE_WATT, state_color, pilotwire_house_power, contract_power);
  }
  
  // if pilot wire mode is enabled
  if (actual_mode != PILOTWIRE_DISABLED)
  {
    // get state and label
    if (pilotwire_offloaded == true) actual_state = PILOTWIRE_OFFLOAD;
    else actual_state = PilotwireGetRelayState ();
    actual_label = PilotwireGetStateLabel (actual_state);
    
    // set color according to state
    switch (actual_state)
    {
      case PILOTWIRE_COMFORT:
        strcpy (state_color, "green");
        break;
      case PILOTWIRE_FROST:
        strcpy (state_color, "grey");
        break;
      case PILOTWIRE_ECO:
        strcpy (state_color, "blue");
        break;
      case PILOTWIRE_OFF:
      case PILOTWIRE_OFFLOAD:
        strcpy (state_color, "red");
        break;
      default:
        strcpy (state_color, "black");
    }
 
    // if pilotwire is not disabled, display current state
    if (actual_mode != PILOTWIRE_DISABLED) snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><td colspan=2 style='font-size:2em; font-weight:bold; color:%s; text-align:center;'>%s</td></tr>", mqtt_data, state_color, actual_label);
  }
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
    case FUNC_COMMAND:
      result = PilotwireCommand ();
      break;
    case FUNC_EVERY_250_MSECOND:
      PilotwireEvery250MSecond ();
      break;
    case FUNC_JSON_APPEND:
      PilotwireShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_PILOTWIRE_HEATER, PilotwireWebPageHeater);
      WebServer->on ("/" D_PAGE_PILOTWIRE_METER, PilotwireWebPageMeter);
      WebServer->on ("/" D_PAGE_PILOTWIRE_CONTROL, PilotwireWebPageControl);
      break;
    case FUNC_WEB_APPEND:
      PilotwireWebState ();
      break;
//    case FUNC_WEB_ADD_MAIN_BUTTON:
//      PilotwireWebMainButton ();
//      break;
    case FUNC_WEB_ADD_BUTTON:
      PilotwireWebButton ();
      break;
#endif  // USE_WEBSERVER

  }
  return result;
}

#endif // USE_PILOTWIRE
