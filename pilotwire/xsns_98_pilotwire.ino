/*
  xsns_98_pilotwire.ino - French Pilot Wire (Fil Pilote) support (~27.5 kb)
  for Sonoff Basic or Sonoff Dual R2
  
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
#define D_PAGE_PILOTWIRE_JSON           "json"

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

#define PILOTWIRE_TEMP_MIN              0
#define PILOTWIRE_TEMP_MAX              50
#define PILOTWIRE_TEMP_DEFAULT          18
#define PILOTWIRE_TEMP_STEP             0.5

#define PILOTWIRE_SHIFT_MIN             -10
#define PILOTWIRE_SHIFT_MAX             10

#define PILOTWIRE_DRIFT_DEFAULT         0
#define PILOTWIRE_DRIFT_STEP            0.1

#define PILOTWIRE_TEMP_THRESHOLD        0.25

#define PILOTWIRE_GRAPH_STEP            5           // collect temperature every 5mn
#define PILOTWIRE_GRAPH_SAMPLE          288         // 24 hours display with collect every 5 mn
#define PILOTWIRE_GRAPH_WIDTH           800      
#define PILOTWIRE_GRAPH_HEIGHT          400 
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
const char *const arrControlTemp[]   PROGMEM = {"Target", "Thermostat"};
const char *const arrControlSet[]    PROGMEM = {"Set to", "Définir à"};
const char *const arrControlStatus[] PROGMEM = {"Currently heating", "Radiateur en chauffe"};

// french flag icon coded in base64
const char flag_fr_0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAACAAAAAgBAMAAACBVGfHAAAGfnpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHjarVdbluMoDP3XKmYJBiEEy+Glc2YHs/y5gOOqpJOq6u7YscEEC6GrxzWN//41+geH13BQEE0xx3jgCDlkX9BJxz52646w7uvw5194vhun6w+PIUbL+zGOc37BuHy8oOEcr/fjpO2Uk05B7rhbmufKs99PJU9B7Pe4O58pny+U+Gk75+XbKfa2rYfnoDBGF8hjT36w42Pd/V6J91VwRdw9Kybi39WXdX9iP7pM98SAV+/BfsdNM/4wxxZ021Z8sNM57uRhnK9l/J1Gzl8r+88a5Xbk4/PxyX5mPZmNvbsSIsFc8dzUbSurh4kV5uT1WsSpuAR9XWfGmY5yNKDWsdVKR8VDdh4WNxdcd8WZG6ttrkHF4IeHub33zfMaSzB/9o0nBGGezrwSZ+6cgEQDcoxhf+ni1rp5rofFElbuDjO9g7CJ4t1JjwN/et4JMptu7tyRLltBLz/9C2pM5OYdswCIs9OmsuzraDfH4zGBZSAoy8wJGyxH3SKquA/f4oUzH0KYGo4dL077KQAmwtoCZRwDgSM6Fhfdod6rc7BjAj4FmnsOvgIBJyS+Q0sfmCPASX6ujXfUrble/B5GegEQgqBRQJO5AKwQJETEW4ILFRKWICJRVJJkKZFjiBJj1DjzVFHWoKJRVZNmLYlTSJJi0pRSTiX7zEhjQjlmzSnnXAoWLaFAVsH8goHqK9dQpcaqNdVcS4P7tNCkxaYttdxK9507UgD12LWnnnsZbsCVRhgy4tCRRh7";
const char flag_fr_1[] PROGMEM = "F4GvGFkwsmlqybOVC7UT1HrVH5L5GzZ2o+QXUnKcfqGFY9SbCzXQiEzMg5oMD4joRgEP7idmRXAh+IjcxO7JnYhYPLWWC091EDAiG4byYu7D7QO4lbgTr/i5u/hlyNKF7B3I0ofuE3K+4PUGtl5VueQE0oxA2RYZkhJ95KT4VX8dAP8xuKrNGvWwNRaB2XI01NzMIUoa32EyEqj20KbT2gUSwX7Mj9fRKoqU8MKVGT7W1UXuXKfWz0MjBDm7Yj1Z0hjL07Qc0sFGhTW+tjlpbHwO7951q6XNq9qMpgAKuajEid1ZrvXUdOfbSR7SaxAVxMgoi4Fh9uN/V0uPA160zeIYWXDn31Tau3EcLBFKwzJxda2uyzHr+0AJFlZHzbEN3SPI6ypS1dQzAkvSAdIP0Yw7lITVn2BwO07GhAe+BC3XFXnszx5xbrgbHQs0wuIlUC3XAN4gFhY7tBn6zVtMP0P+lpQXi5RMTQbSoaxnLgDecxkkjbgugxv7SGnMxCm11Ity4sQlD1nYfHwy2uBZdsPflisE2+EUW+AhiqYU6gsgyj6EItjZCRwBIgz7wpukfVgTuobnAPWDLZV1DBA+5x4aegfTQanf5htlq7YAttTrXTzMFZXrtOZeroGUkneGs5In/nKHZw1c/KUWXp6jJSHMKHASwi4mHP6vmmktHfqhiLZvODDLURxvdj4AIQgaK1j1Z7QgYjQir08q1svnxGqT7lkcCXKHRBm9il2DC2lNaeuHnOb/2qMtT0GYkQyR/62HKWg7ZMpLYS2W20ham5yC/IVdWBfMo0wMq0gjOOKoiBY1Qq8Om/yQJ0G8G+csYp98Mcn";
const char flag_fr_2[] PROGMEM = "3lLvQR/KvtyODwhFY7tggpLZhL08tbzWYoJLMf+3EGOfJqyuuBfoDsj4ClHyD7I2DpGcLTLWEgUIb2XSq6MhD9RQq6y0D0RQp6pcQtn2KLqF68xdObbH3Qm2w9c/ZbbN3oTbY2epOtE73J1ttGb7B1otum7SNjFR1ukZyMGPVTDlgQuz3Ctrjb6GWrhTDPhu9GekcpOmRXkb8uRbOlu7ymtgspeO7kGbsg1V2Q/CpIYGWzxqAiYUdnReJZkch6XBUJmfrMVj2gdv20Cl2Y0l9WoQtT+qYKbTgn/ai6uafp5BZ1cQtb3GKxD8KWe4Yo0JHJT6xMeqKTntzg7Rnp+1uSSX9egO7rD/1mAdolR09U+0YVj526B2i2aUQ4UYUlFqqA3RYPqZOHHF/yTPp5xvk64dDPM87ThLOpJbdKOmni4hN904m86EQEnRh+sokVvptP5M0n6uIT4eITi1KStO8o5LfxjOzWHclwdZF5g49bcwzaG7nmMZnvAN+vpz+lEL+D3xhwyxjlTqArmgYYE6jg+iq7YhqKPAlpumK6zu8MzN3fGW1/Z8j6zvBy+/QzOcx98QmBKKH/AV/yd3T3AHXYAAABhWlDQ1BJQ0MgcHJvZmlsZQAAeJx9kT1Iw0AcxV9TtVYqDnYQdchQnSyIiuimVShChVArtOpgcumH0KQhSXFxFFwLDn4sVh1cnHV1cBUEwQ8QJ0cnRRcp8X9JoUWsB8f9eHfvcfcOEKpFpllto4Cm22YyHhPTmRUx8IoODCCIaQRlZhmzkpRAy/F1Dx9f76I8q/W5P0e3mrUY4BOJZ5hh2sTrxJObtsF5nzjMCrJKfE48Y";
const char flag_fr_3[] PROGMEM = "tIFiR+5rnj8xjnvssAzw2YqOUccJhbzTaw0MSuYGvEEcUTVdMoX0h6rnLc4a8Uyq9+TvzCU1ZeXuE5zEHEsYBESRCgoYwNF2IjSqpNiIUn7sRb+ftcvkUsh1wYYOeZRggbZ9YP/we9urdz4mJcUigHtL47zMQQEdoFaxXG+jx2ndgL4n4ErveEvVYGpT9IrDS1yBPRsAxfXDU3ZAy53gL4nQzZlV/LTFHI54P2MvikD9N4CXateb/V9nD4AKeoqcQMcHALDecpea/Huzube/j1T7+8HeENyqd9jlV0AAAAPUExURfZVAAMile0pOs2ktv3//KPDeLIAAAABdFJOUwBA5thmAAAAAWJLR0QAiAUdSAAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+QGGwoZFisaT5UAAABbSURBVCjPY2AAAxcgYEAARhOQgLMCnC8IEVCCiQjCBJRgCuACClAFcAElqAKEgAJEAUJACYsAI6qAAhECgqgCSgMlQIbTCYcHZhBiBDJmNGBEFGZUYkQ2SnIAAEjVRhNKZ4XRAAAAAElFTkSuQmCC";

// english flag icon coded in base64
const char flag_en_0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAACAAAAAgBAMAAACBVGfHAAAKB3pUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHjarZlpduM6DoX/cxW9BM7Dcjie0zvo5fcHUnKcwZWk3ovLlkSTIIALXIAuNf/336X+w5+LxSofUo4lRs2fL77Yyk3W5+9cjfb7c//Z6yue342rxxeWIcfVncc4r/mV8fC2IPlrvL0fV6lfcvIlyOh3WzvZWe7HpeQlyNkzbq5nVa4FNT6Zc71tv8TeZn149glnjIA8Z5Wdzji9P+3ZyZ135R35tC4xkW/3vYw4Zz77Tz1c94UDH3cf/KdvzdybO46g26z4wU/XuAkfxt1jG/tOI2MfO9tnjYK7XfzZf2uNvNY81lUfFe6Kl1G3KfuOiQ13ur0s8kq8A/dpvwqvrKvuoDYwtSndeCjG4vFlvBmmmmXmvnbTUdHbaXG3tbZbt8cy7i+2O4HAy8ssm5QrbrgMHh3kHMP2oYvZ+xbZj80yOw/DTGsQJii+e6mPA3/7eidoLQlzY3R++Aq9rMQXaghy8sksADHr8mnY/jXqXPTHPwHWgWDYbs4YWHU7Ilowb7HlNs5OB8VUr0++mDQuAbiIvQPKGAcCOhoXTDQ6WZuMwY8ZfCqaW+dtAwETVLADLa13LgJOtrI3a5LZc22wZxh6AYhAiiSgKa4ClvfBR/ItE0JVBRd8CCGGFHIooUYXfQwxxhSFp2pyyaeQYkopp5JqdtnnkGNOOeeSa7HFQWNBlVhSyaWUWtm0+oqsyvzKQLPNNd9Ciy213EqrnfDpvocee+q5l16HHW5AAWrEkUYeZdRpJqE";
const char flag_en_1[] PROGMEM = "0/QwzzjTzLLMuYm255VdYcaWVV1n1gdqF6nvUPiL3Z9TMhZrdQMm89IYawyndIozQSRDMQMx6A+JJECCgrWCms/HeCnKCmS7WKeeCRcsg4AwjiIGgn8aGZR7YvSH3EjeFd3+Lm/0KOSXQ/RvIKYHuCbnPuH2B2qibbt0GSLIQn8KQjvRbrlSbq20jOaf3LSz41XXOWByOL52sCXOlMDU2DeVq6Yskq8Wn4rewVuowrRmb9KAQtzFStiEmP/HXWN7WYGa4xTYfZ8fZHwR/L3e4FpYD5uBrXbhv5gDKqlfjA/8isRb2rf75NbYRkZ2ICmWj8wlcMMGG24SEVnb6VrzoDgh9WwGKKFZKnwMretlWuDNd2fqr+S+nq+/m5yhOHQH+0rff97XakXS3RJebOQ4lriaw0tqeSgTm964RpwCSXMjGHEAhqBsw42PqA1US0LowG0E+TRXMSIGmediLL6X8MGUhjrxbttTlVbjcHhc5jqaYslKrizqcaTzEuHnJzrdTxsxM3Io89FBHEdOn6xU0TSgmjD5CG5igt1JjztaNxVd9lTpd6YSY7Iboxi7EWs8KgWvoVeOaPaxJX0Dh0n6RvG1GWxvOI3qbSbPPVWGkLqm7yMe0p1JxJBSVeLjp1Hv+xqeFCN5WoK8YhVNNb+JUO6Ou6oJYTJ/RfzP7mgz2w/pMFl1BzF5K9iSYfzr/5XT18/m2ERSWTwLVk1NLWKcRukBcA6iNgWt7TaUJSYy+iM6p6ZGWuA/XBTcZxFbJ9dUa/Ea34bqblRhoC6hhyJxqmT2uGuZEVsnwQiu9QZ9puTHHLPR4F7kJ75FC+zbMTXD1MkTZcK";
const char flag_en_2[] PROGMEM = "InuJxs6K0Qg9QciWgriVkjhtSH2XuyVJleJCFXySdIs6ozkb+so0rdmUvEkwNHcv2hZPVJ9F9KVs+iSTv4WAujBVdCXzXFRogvV3c6dPYZsE8ZNKuhT8GB2lk0BUeJBuI82tVMfIcnVk71jZbj9mE+VONCbYmKfLHDznRVVs76zvetb87ejVKCLYl2eKeI35meKOL25pGd2tR/zMda31TJz3KJkUy/fNL9cIY1ekUpGaF3Ecf6zUyB1oSUOjRC0uIa37dcuLXdLodEqsiiIqUJoVihr8civnZb7TyD3Z7PVT2sKgO6xht2V7R2US9HBhx0Kpac8q5roEouTmrA2exc+FTZRYaU3oVZIJmx9p1/ZC7RbCCG1InwzQhXHVu0SBfHbMZQQpO6AM0pxO/8c9yen92TqtnBctborf02St1YHZ+aJ6jCxU3++Mn+eR/FRnXdZY3IcCvSjz02uvfhgEUBMw/lPq1RHxZl8TotUpvcCIryDeUo1RPiMdSCp+DzfhHGksqYnSKJQm6rjkVqJXFl5TTFmJstTf+YCF5Rn2n0zOOtfZCepsykqiPvgIZdsb25KozTyk7PTZXzXkFrB412cjrXFf22oFzTg1W/nP9yuvrl/JfT1av5QvMX1pK/sC65KY2sJC3t9HJZmhgHSMR+GkT2irzTrplrZZsgG6RSw2GnFcfk7DuAY8iM7qgLZdF3myMKOovLkhPq1Gmt7/IrtRrt2ZweYjRYK+2dtJa95EpcS22aByQHkgkkiaPZG6lW5rZmf81ZPIRGGAyabCcdwP2F4E5x/2KJ+os1Xy5Rf7HmyyXqn5jzvES50ylRB6RfGisiB";
const char flag_en_3[] PROGMEM = "ODwMJU2OdJhQW4bw4OnkZYT9DjlrFau8k2KPKTkLQOoobtHA3CiZcMN0DwC9KiLZJ1uoyyVCJDdVMghyRkewaG6343cIgIsARE8Zg4d91T2akTei2nqh/O+naZ+te0fpqm/sOLLaeqLeV1ftaatQF+e1yicHMnMGiRhCINV0rxOVQkOGHT/6rcnq1fXbwUdxTzpaXtrc6TOI3HjNvyoRl9Ho27VDlK6I2ybvRIKo4YmMykOlJ+T4zCCRCvVhMPvbmeksfG0ADxJMzOzmts5PFJOAgd8FElanvolQPz5avXTYvWPVj8tVj9Yvc8kyFjQtJBmomp14et49boRilRJ+u4DOESXc+XwEgcnKYqcjoGkpYNwSX6L4GTZpBuEvkWslpUFn7IOPiLN4ybSdhPpadJ3v27m4DBEpjua9bEyQWSexJDnZVM6pyU15jqMPufuh9ImdL8JHVahuXqweRKqpt8XMq+HzNHhaEDD/ncKfNpffa3Aq+1fu0D9tQofNFBf+EDOPHQ9swidTt5mRrgT/Mqyi9MueRtpdPaC+6ijzlknV/l/iHOVPlTaTGn7O31R9oROlLNYzDEJry/6z7W3f6NzJXwOWQuht498fOrtqb2Ow1bJzzXhWYwZ6quysH+ueqouP6kL6lNh+LMaL0uTeq5N/6Q0qd/64vyaNXwjEKiwtRhHDpakdrd/jDqwLXrOUv74gx1X4Nm9DV3NgZcmovk0Cka4mOWEcthQqoG0dej1s/qi/qWyttSP5sF9QuuNDiQY8gZmm77U6zdEKQxOme+88fm6qwqWx+4PsvJSWU5cq0uKpQ4vkpyR/BJcPaFBx1qlFEKS";
const char flag_en_4[] PROGMEM = "wrE3+9hUKUJt7SPZ7kHVDjL5nSPvigNvfEq+p+uWWQmuMt74gBK1hnqWegvd1TjIOV+i1sAA4EfvHanfiVTHRxgEy0fIp/WS5PejrUnF1f6VIov9Uf3/nufB/WXA79gAAAGFaUNDUElDQyBwcm9maWxlAAB4nH2RPUjDQBzFX1O1VioOdhB1yFCdLIiK6KZVKEKFUCu06mBy6YfQpCFJcXEUXAsOfixWHVycdXVwFQTBDxAnRydFFynxf0mhRawHx/14d+9x9w4QqkWmWW2jgKbbZjIeE9OZFTHwig4MIIhpBGVmGbOSlEDL8XUPH1/vojyr9bk/R7eatRjgE4lnmGHaxOvEk5u2wXmfOMwKskp8Tjxi0gWJH7muePzGOe+ywDPDZio5RxwmFvNNrDQxK5ga8QRxRNV0yhfSHquctzhrxTKr35O/MJTVl5e4TnMQcSxgERJEKChjA0XYiNKqk2IhSfuxFv5+1y+RSyHXBhg55lGCBtn1g//B726t3PiYlxSKAe0vjvMxBAR2gVrFcb6PHad2AvifgSu94S9VgalP0isNLXIE9GwDF9cNTdkDLneAvidDNmVX8tMUcjng/Yy+KQP03gJdq15v9X2cPgAp6ipxAxwcAsN5yl5r8e7O5t7+PVPv7wd4Q3Kp32OVXQAAABtQTFRFYQAANDRpsRouT0p4b26UwK6+4aav5NDX/Pr3wBAfiwAAAAF0Uk5TAEDm2GYAAAABYktHRACIBR1IAAAACXBIWXMAAC4jAAAuIwF4pT92AAAAB3RJTUUH5AYbChofeeuk8gAAALdJREFUKM9lkcENwyAMRd00Us4IBojYAKU";
const char flag_en_5[] PROGMEM = "LoCxQX5pOwJkbHYGxa1wSMP3/xMd+WAaApUjQNKMpjuf5vu1s/6yB+RWgetcG//Ab+4iiwKC1HbG0WLuWAm+8ZrtXCBRga/nk3N4kuyOlCDfTMwgye8PWqJEDOvB1CQgaQHUtBM2gJTR1c5ZJaVblmjlIgyAPgjAIJiu0/geLZNLOBiYMVAomiSD1AW99EcgigeSSk3h97tQRqy4gfAGw0HAXNQXCvAAAAABJRU5ErkJggg==";

// icons
enum IntercomIcons { ICON_FRENCH, ICON_ENGLISH };
const char* pilotwire_icon[][6] PROGMEM = { 
  {flag_fr_0, flag_fr_1, flag_fr_2, flag_fr_3, NULL,      NULL      },
  {flag_en_0, flag_en_1, flag_en_2, flag_en_3, flag_en_4, flag_en_5 }
};

// enumarations
enum PilotwireDevices { PILOTWIRE_DEVICE_NORMAL, PILOTWIRE_DEVICE_DIRECT };
enum PilotwireModes { PILOTWIRE_DISABLED, PILOTWIRE_OFF, PILOTWIRE_COMFORT, PILOTWIRE_ECO, PILOTWIRE_FROST, PILOTWIRE_THERMOSTAT, PILOTWIRE_OFFLOAD };
enum PilotwireSources { PILOTWIRE_SOURCE_NONE, PILOTWIRE_SOURCE_LOCAL, PILOTWIRE_SOURCE_REMOTE };
enum PilotwireLangages { PILOTWIRE_LANGAGE_ENGLISH, PILOTWIRE_LANGAGE_FRENCH };

// fil pilote commands
enum PilotwireCommands { CMND_PILOTWIRE_MODE, CMND_PILOTWIRE_MIN, CMND_PILOTWIRE_MAX, CMND_PILOTWIRE_TARGET, CMND_PILOTWIRE_OUTSIDE, CMND_PILOTWIRE_DRIFT};
const char kPilotWireCommands[] PROGMEM = D_CMND_PILOTWIRE_MODE "|" D_CMND_PILOTWIRE_MIN "|" D_CMND_PILOTWIRE_MAX "|" D_CMND_PILOTWIRE_TARGET "|" D_CMND_PILOTWIRE_OUTSIDE "|" D_CMND_PILOTWIRE_DRIFT;

/*************************************************\
 *               Variables
\*************************************************/

// variables
bool     pilotwire_outside_mode;                   // flag for outside mode (target temperature dropped)
bool     pilotwire_updated;                        // JSON needs update
int      pilotwire_graph_refresh;                  // graph refresh rate (in seconds)
uint8_t  pilotwire_device;                         // device type (pilotwire or direct heater)
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
String   str_pilotwire_json;                       // last pilotwire data JSON

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

// get state color
String PilotwireGetStateColor (uint8_t state)
{
  String str_color;
  
  // set color according to state
  switch (state)
  {
    case PILOTWIRE_COMFORT:
      str_color = "green";
      break;
    case PILOTWIRE_FROST:
      str_color = "grey";
      break;
    case PILOTWIRE_ECO:
      str_color = "blue";
      break;
    case PILOTWIRE_OFF:
    case PILOTWIRE_OFFLOAD:
      str_color = "red";
      break;
    default:
      str_color = "white";
  }

  return str_color;
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
  if ((device_type == PILOTWIRE_DEVICE_NORMAL) && (devices_present > 1))
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

  // if pilotwire connexion and 2 relays
  if ((device_type == PILOTWIRE_DEVICE_NORMAL) && (devices_present > 1))
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
  if ((devices_present == 1) || (device_type == PILOTWIRE_DEVICE_DIRECT))
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
  float min_temperature;

  // get drift temperature (/10)
  min_temperature = float (Settings.weight_item) / 10;
  
  // check if within range
  if (min_temperature < PILOTWIRE_TEMP_MIN) min_temperature = PILOTWIRE_TEMP_MIN;
  if (min_temperature > PILOTWIRE_TEMP_MAX) min_temperature = PILOTWIRE_TEMP_MAX;

  return min_temperature;
}

// set pilot wire maximum temperature
void PilotwireSetMaxTemperature (float new_temperature)
{
  // if within range, save temperature correction
  if ((new_temperature >= PILOTWIRE_TEMP_MIN) && (new_temperature <= PILOTWIRE_TEMP_MAX)) Settings.energy_frequency_calibration = (unsigned long) int (new_temperature * 10);

  // update JSON status
  pilotwire_updated = true;
}

// get pilot wire maximum temperature
float PilotwireGetMaxTemperature ()
{
  float max_temperature;

  // get drift temperature (/10)
  max_temperature = float (Settings.energy_frequency_calibration) / 10;
  
  // check if within range
  if (max_temperature < PILOTWIRE_TEMP_MIN) max_temperature = PILOTWIRE_TEMP_MIN;
  if (max_temperature > PILOTWIRE_TEMP_MAX) max_temperature = PILOTWIRE_TEMP_MAX;

  return max_temperature;
}

// set pilot wire drift temperature
void PilotwireSetDrift (float new_drift)
{
  // if within range, save temperature correction
  if ((new_drift >= PILOTWIRE_SHIFT_MIN) && (new_drift <= PILOTWIRE_SHIFT_MAX)) Settings.weight_calibration = (unsigned long) int (50 + (new_drift * 10));

  // update JSON status
  pilotwire_updated = true;
}

// get pilot wire drift temperature
float PilotwireGetDrift ()
{
  float drift;

  // get drift temperature (/10)
  drift = float (Settings.weight_calibration);
  drift = ((drift - 50) / 10);
  
  // check if within range
  if (drift < PILOTWIRE_SHIFT_MIN) drift = PILOTWIRE_SHIFT_MIN;
  if (drift > PILOTWIRE_SHIFT_MAX) drift = PILOTWIRE_SHIFT_MAX;

  return drift;
}

// set target temperature
void PilotwireSetTargetTemperature (float new_thermostat)
{
  // save target temperature
  if ((new_thermostat >= PILOTWIRE_TEMP_MIN) && (new_thermostat <= PILOTWIRE_TEMP_MAX))
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
  if (temperature < PILOTWIRE_TEMP_MIN) temperature = PILOTWIRE_TEMP_MIN;
  if (temperature > PILOTWIRE_TEMP_MAX) temperature = PILOTWIRE_TEMP_MAX;

  return temperature;
}

// set outside mode dropdown temperature
void PilotwireSetOutsideDropdown (float new_dropdown)
{
  // save target temperature
  if (new_dropdown <= PILOTWIRE_SHIFT_MAX) Settings.energy_voltage_calibration = (unsigned long) int (new_dropdown * 10);

  // update JSON status
  pilotwire_updated = true;
}

// get outside mode dropdown temperature
float PilotwireGetOutsideDropdown ()
{
  float temperature;

  // get target temperature (/10)
  temperature = float (Settings.energy_voltage_calibration) / 10;
 
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
void PilotwireSetDS18B20Pullup (bool new_state)
{
  bool actual_state = PilotwireGetDS18B20Pullup ();

  // if not set, set pullup resistor for DS18B20 temperature sensor
  if (actual_state != new_state)
  {
    // update DS18B20 pullup state
    bitWrite (Settings.flag3.data, 24, new_state);

    // ask for reboot
    restart_flag = 2;
  }
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
    // trunc temperature to 1 decimal
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

  // save mqtt_data
  str_mqtt = mqtt_data;

  // get temperature and mode
  actual_temperature = PilotwireGetTemperature ();
  actual_mode = PilotwireGetMode ();
  str_label   = PilotwireGetStateLabel (actual_mode);
 
  // pilotwire mode  -->  "PilotWire":{"Relay":2,"Mode":5,"Label":"Thermostat",Offload:"ON","Temperature":20.5,"Target":21,"Min"=15.5,"Max"=25}
  str_json = "\"" + String (D_JSON_PILOTWIRE) + "\":{";
  str_json += "\"" + String (D_JSON_PILOTWIRE_RELAY) + "\":" + String (devices_present) + ",";
  str_json += "\"" + String (D_JSON_PILOTWIRE_MODE) + "\":" + String (actual_mode) + ",";
  str_json += "\"" + String (D_JSON_PILOTWIRE_LABEL) + "\":\"" + str_label + "\",";

  // add outside mode
  str_json += "\"" + String (D_JSON_PILOTWIRE_OUTSIDE) + "\":";
  if (pilotwire_outside_mode == true) str_json += "\"ON\",";
  else str_json += "\"OFF\",";

  // if defined, add current temperature
  if (actual_temperature != NAN) str_json += "\"" + String (D_JSON_PILOTWIRE_TEMPERATURE) + "\":" + String (actual_temperature, 1) + ",";

  // thermostat data
  str_json += "\"" + String (D_JSON_PILOTWIRE_TARGET) + "\":" + String (PilotwireGetTargetTemperature (), 1) + ",";
  str_json += "\"" + String (D_JSON_PILOTWIRE_DRIFT) + "\":" + String (PilotwireGetDrift (), 1) + ",";
  str_json += "\"" + String (D_JSON_PILOTWIRE_MIN) + "\":" + String (PilotwireGetMinTemperature (), 1) + ",";
  str_json += "\"" + String (D_JSON_PILOTWIRE_MAX) + "\":" + String (PilotwireGetMaxTemperature (), 1) + "}";

  // if pilot wire mode is enabled
  if (actual_mode != PILOTWIRE_DISABLED)
  {
    // get mode and associated label
    actual_mode = PilotwireGetRelayState ();
    str_label   = PilotwireGetStateLabel (actual_mode);

    // relay state  -->  ,"State":{"Mode":4,"Label":"Comfort"}
    str_json += ",\"" + String (D_JSON_PILOTWIRE_STATE) + "\":{\"" + String (D_JSON_PILOTWIRE_MODE) + "\":" + String (actual_mode);
    str_json += ",\"" + String (D_JSON_PILOTWIRE_LABEL) + "\":\"" + str_label + "\"}";
  }

  // generate JSON for offloading and temperature
  snprintf_P (mqtt_data, sizeof(mqtt_data), str_json.c_str());
  OffloadingShowJSON (true);
  TemperatureShowJSON (true);

  // save latest teleinfo JSON
  str_pilotwire_json = mqtt_data;

  // generate MQTT message according to append mode
  if (append == true) str_mqtt += "," + str_pilotwire_json;
  else str_mqtt = "{" + str_pilotwire_json + "}";

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
  bool    is_offloaded;
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
    // if temperature change, data update
    if (heater_temperature != pilotwire_temperature) pilotwire_updated = true;
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
    is_offloaded = OffloadingIsOffloaded ();

    // if offload mode, target state is off
    if (is_offloaded == true) target_state = PILOTWIRE_OFF;
 
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
  pilotwire_graph_counter = pilotwire_graph_counter % pilotwire_graph_refresh;
}

void PilotwireInit ()
{
  int    index;
  String str_setting;

  // init default values
  pilotwire_device             = PILOTWIRE_DEVICE_NORMAL;
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
  pilotwire_graph_refresh      = 60 * PILOTWIRE_GRAPH_STEP;

  // initialise temperature graph
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    arr_temperature[index] = NAN;
    arr_target[index] = NAN;
    arr_state[index] = PILOTWIRE_OFF;
  }

  // if needed, initialise offload and remote temperature MQTT data
  str_setting = (char*)Settings.free_f03;
  index = str_setting.indexOf ('|');
  if (index == -1) strcpy ((char*)Settings.free_f03, ";|;");
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// display base64 embeded icon
void PilotwireWebDisplayIcon (uint8_t icon_index)
{
  uint8_t nbrItem, index;

  // display icon
  WSContentSend_P (PSTR ("'data:image/png;base64,"));
  nbrItem = sizeof (pilotwire_icon[icon_index]) / sizeof (char*);
  for (index=0; index<nbrItem; index++) if (pilotwire_icon[icon_index][index] != NULL) WSContentSend_P (pilotwire_icon[icon_index][index]);
  WSContentSend_P (PSTR ("'"));
}

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
  if ((devices_present > 1) && (device_type == PILOTWIRE_DEVICE_NORMAL))
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
  uint8_t actual_mode, actual_state;
  float   temperature;
  String  str_title, str_text, str_label, str_color;

  // get current mode and temperature
  actual_mode = PilotwireGetMode ();
  temperature = PilotwireGetTemperature ();

  // handle sensor source
  switch (pilotwire_temperature_source)
  {
    case PILOTWIRE_SOURCE_NONE:  // no temperature source available 
      str_title = D_PILOTWIRE_TEMPERATURE;
      str_text  = "<b>---</b>";
      break;
    case PILOTWIRE_SOURCE_LOCAL:  // local temperature source used 
      str_title = D_PILOTWIRE_LOCAL + String (" ") + D_PILOTWIRE_TEMPERATURE;
      str_text  = "<b>" + String (temperature, 1) + "</b>";
      break;
    case PILOTWIRE_SOURCE_REMOTE:  // remote temperature source used 
      str_title = D_PILOTWIRE_REMOTE + String (" ") + D_PILOTWIRE_TEMPERATURE;
      str_text  = "<b>" + String (temperature, 1) + "</b>";
      break;
  }

  // add target temperature and unit
  if (actual_mode == PILOTWIRE_THERMOSTAT) str_text += " / " + String (PilotwireGetCurrentTarget (), 1);
  str_text += "°" + String (TempUnit ());

  // display temperature
  WSContentSend_PD ("{s}%s{m}%s{e}", str_title.c_str (), str_text.c_str ());

  // display day or outside mode
  temperature = PilotwireGetOutsideDropdown ();
  if (pilotwire_outside_mode == true) str_text = "<span><b>" + String (D_PILOTWIRE_OUTSIDE) + "</b> (-" + String (temperature, 1) + "°" + String (TempUnit ()) + ")</span>";
  else str_text = "<span><b>" + String (D_PILOTWIRE_NORMAL) + "</b></span>";
  WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), D_PILOTWIRE_MODE, str_text.c_str ());

  // get heater state, associated label and color
  actual_state = PilotwireGetRelayState ();
  str_label = PilotwireGetStateLabel (actual_state);
  str_color = PilotwireGetStateColor (actual_state);

  // display current state
  str_title = D_PILOTWIRE_HEATER + String (" ") + D_PILOTWIRE_STATE;
  str_text  = "<span style='color:" + str_color + ";'><b>" + str_label + "</b></span>";
  WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), str_title.c_str (), str_text.c_str ());
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
  str_drift_step = String (PILOTWIRE_DRIFT_STEP, 1);

  // page comes from save button on configuration page
  if (WebServer->hasArg("save"))
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
    if (strlen(argument) > 0) PilotwireSetDS18B20Pullup (true);
    else PilotwireSetDS18B20Pullup (false);
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
    WSContentSend_P (str_conf_temperature, str_text.c_str (), str_unit.c_str (), str_conf_topic_style, D_CMND_PILOTWIRE_OUTSIDE, D_CMND_PILOTWIRE_OUTSIDE, 0, PILOTWIRE_SHIFT_MAX, str_temp_step.c_str(), str_temperature.c_str());

    // temperature correction label and input
    str_temperature = String (PilotwireGetDrift (), 1);
    WSContentSend_P (str_conf_temperature, D_PILOTWIRE_DRIFT, str_unit.c_str (), str_conf_topic_style, D_CMND_PILOTWIRE_DRIFT, D_CMND_PILOTWIRE_DRIFT, PILOTWIRE_SHIFT_MIN, PILOTWIRE_SHIFT_MAX, str_drift_step.c_str(), str_temperature.c_str());
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
  WSContentSend_P (PSTR ("<text class='time' x='%d' y='52%%'>%sh</text>\n"), graph_x, str_hour);

  // dislay next 5 time marks (every 4 hours)
  for (index = 0; index < 5; index++)
  {
    current_dst.hour = (current_dst.hour + 4) % 24;
    graph_x += 48 * graph_width / PILOTWIRE_GRAPH_SAMPLE;
    sprintf(str_hour, "%02d", current_dst.hour);
    WSContentSend_P (PSTR ("<text class='time' x='%d' y='52%%'>%sh</text>\n"), graph_x, str_hour);
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
  if (WebServer->hasArg(D_CMND_PILOTWIRE_LANG))
  {
    // get langage according to 'lang' parameter
    WebGetArg (D_CMND_PILOTWIRE_LANG, argument, PILOTWIRE_BUFFER_SIZE);

    if (strcmp (argument, D_PILOTWIRE_ENGLISH) == 0) pilotwire_langage = PILOTWIRE_LANGAGE_ENGLISH;
    else if (strcmp (argument, D_PILOTWIRE_FRENCH) == 0) pilotwire_langage = PILOTWIRE_LANGAGE_FRENCH;
  }

  // else if heater has to be switched off, set in anti-frost mode
  else if (WebServer->hasArg(D_CMND_PILOTWIRE_OFF)) PilotwireSetMode (PILOTWIRE_FROST); 

  // else, if heater has to be switched on, set in thermostat mode
  else if (WebServer->hasArg(D_CMND_PILOTWIRE_ON)) PilotwireSetMode (PILOTWIRE_THERMOSTAT);

   // else, if target temperature has been defined
  else if (WebServer->hasArg(D_CMND_PILOTWIRE_TARGET))
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
  str_temperature = String (temperature, 1);
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
  WSContentSend_P (PSTR ("div {width:100%%;margin:auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("fieldset {border-radius:10px;}\n"));
  
  WSContentSend_P (PSTR (".title {font-size:5vh;}\n"));
  WSContentSend_P (PSTR (".temperature {font-size:24px;}\n"));
  WSContentSend_P (PSTR (".target {font-size:24px;}\n"));
  WSContentSend_P (PSTR (".time {font-size:20px;}\n"));
  WSContentSend_P (PSTR (".switch {text-align:left;font-size:24px;}\n"));
  WSContentSend_P (PSTR (".bold {font-weight:bold;}\n"));
  
  WSContentSend_P (PSTR (".flag {position:absolute;top:20px;}\n"));
  WSContentSend_P (PSTR (".fr {right:40%%;}\n"));
  WSContentSend_P (PSTR (".en {left:40%%;}\n"));

  WSContentSend_P (PSTR (".centered {width:%d%%;max-width:%dpx;}\n"), PILOTWIRE_GRAPH_PERCENT_STOP - PILOTWIRE_GRAPH_PERCENT_START, PILOTWIRE_GRAPH_WIDTH * (PILOTWIRE_GRAPH_PERCENT_STOP - PILOTWIRE_GRAPH_PERCENT_START) / 100);
  WSContentSend_P (PSTR (".graph {max-width:%dpx;}\n"), PILOTWIRE_GRAPH_WIDTH);
  WSContentSend_P (PSTR (".thermostat {height:160px;}\n"));
  WSContentSend_P (PSTR (".yellow {color:#FFFF33;}\n"));

  WSContentSend_P (PSTR (".status {color:red;font-size:18px;font-weight:bold;font-style:italic;}\n"));

  WSContentSend_P (PSTR (".button {border:1px solid white;margin:12px 0px auto;padding:4px 12px;border-radius:12px;font-size:20px;}\n"));
  WSContentSend_P (PSTR (".btn-read {color:orange;border:1px solid transparent;background-color:transparent;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".btn-set {color:orange;border-color:orange;background-color:#FFEBE8;}\n"));

  WSContentSend_P (PSTR (".slider {width:100%%;height:8px;border-radius:4px;background:#FFEBE8;outline:none;opacity:0.7;-webkit-appearance:none;}\n"));
  WSContentSend_P (PSTR (".slider::-webkit-slider-thumb {width:25px;height:25px;border-radius:50%%;background:orange;cursor:pointer;border:1px solid orange;appearance:none;-webkit-appearance:none;}\n"));
  WSContentSend_P (PSTR (".slider::-moz-range-thumb {width:25px;height:25px;border-radius:50%%;background:orange;cursor:pointer;border:1px solid orange;}\n"));
  WSContentSend_P (PSTR (".slider::-ms-thumb {width:25px;height:25px;border-radius:50%%;background:orange;cursor:pointer;border:1px solid orange;}\n"));

  WSContentSend_P (PSTR (".toggle input {display:none;}\n"));
  WSContentSend_P (PSTR (".toggle {position:relative;display:inline-block;width:140px;height:40px;margin:10px auto;}\n"));
  WSContentSend_P (PSTR (".slide-off {position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#bc2612;border:1px solid #aaa;border-radius:5px;}\n"));
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
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_PILOTWIRE_CONTROL);

  // french flag
  WSContentSend_P (PSTR ("<div class='flag fr'><a href='/control?lang=fr'><img src="));
  PilotwireWebDisplayIcon (ICON_FRENCH);
  WSContentSend_P (PSTR (" /></a></div>\n"));

  // english flag
  WSContentSend_P (PSTR ("<div class='flag en'><a href='/control?lang=en'><img src="));
  PilotwireWebDisplayIcon (ICON_ENGLISH);
  WSContentSend_P (PSTR (" /></a></div>\n"));

  // room name and current temperature
  WSContentSend_P (PSTR ("<div class='title bold'>%s</div>\n"), SettingsText(SET_FRIENDLYNAME1));
  WSContentSend_P (PSTR ("<div class='title bold yellow'>%s °C</div>\n"), str_temperature.c_str());

  // if heater is in thermostat mode, button to switch off heater, else button to switch on thermostat
  WSContentSend_P (PSTR ("<div class='centered'>"));
  if (actual_mode == PILOTWIRE_THERMOSTAT) WSContentSend_P (PSTR ("<label class='toggle'><input name='off' type='submit'/><span class='slide-on switch'></span></label>"));
  else WSContentSend_P (PSTR ("<label class='toggle'><input name='on' type='submit'/><span class='slide-off switch'></span></label>"));
  WSContentSend_P (PSTR ("</div>\n"));

  // target temperature
  WSContentSend_P (PSTR ("<div class='centered thermostat'>\n"));
  if (actual_mode == PILOTWIRE_THERMOSTAT)
  {
    // if needed, display heater status
    str_text = "";
    if (actual_state == PILOTWIRE_COMFORT) str_text = arrControlStatus[pilotwire_langage];
    WSContentSend_P (PSTR ("<div class='status'>&nbsp;%s&nbsp;</div>\n"), str_text.c_str());

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

// JSON public page
void PilotwirePageJson ()
{
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR ("{%s}\n"), str_pilotwire_json.c_str ());
  WSContentEnd();
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
      WebServer->on ("/" D_PAGE_PILOTWIRE_CONFIG,  PilotwireWebPageConfigure);
      WebServer->on ("/" D_PAGE_PILOTWIRE_CONTROL, PilotwireWebPageControl);
      WebServer->on ("/" D_PAGE_PILOTWIRE_SWITCH,  PilotwireWebPageSwitchMode);
      WebServer->on ("/" D_PAGE_PILOTWIRE_JSON, PilotwirePageJson);
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
#endif  // USE_WEBSERVER

  }
  return result;
}

#endif // USE_PILOTWIRE
