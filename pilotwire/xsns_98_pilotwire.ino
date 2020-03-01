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
    12/12/2019 - v3.1 - Add public configuration page http://.../control
    30/12/2019 - v4.0 - Functions rewrite for Tasmota 8.x compatibility
    06/01/2019 - v4.1 - Handle offloading with finite state machine
    09/01/2019 - v4.2 - Separation between Offloading driver and Pilotwire sensor
    15/01/2020 - v5.0 - Separate temperature driver and add remote MQTT sensor
    05/02/2020 - v5.1 - Block relay command if not coming from a mode set
    21/02/2020 - v5.2 - Add daily temperature graph
    24/02/2020 - v5.3 - Add control button to main page
    27/02/2020 - v5.4 - Add target temperature and relay state to temperature graph
    01/03/2020 - v5.5 - Add timer management with night dropdown temperature

  Settings are stored using weighting scale parameters :
    - Settings.weight_reference             = Fil pilote mode
    - Settings.weight_max                   = Target temperature  (x10 -> 192 = 19.2°C)
    - Settings.weight_calibration           = Temperature correction (0 = -5°C, 50 = 0°C, 100 = +5°C)
    - Settings.weight_item                  = Minimum temperature (x10 -> 125 = 12.5°C)
    - Settings.energy_frequency_calibration = Maximum temperature (x10 -> 240 = 24.0°C)
    - Settings.energy_voltage_calibration   = Night dropdown temperature (x10 -> 25 = 2.5°C)

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

#define D_PAGE_PILOTWIRE_CONFIG         "config"
#define D_PAGE_PILOTWIRE_CONTROL        "control"

#define D_CMND_PILOTWIRE_ON             "on"
#define D_CMND_PILOTWIRE_OFF            "off"
#define D_CMND_PILOTWIRE_SET            "set"
#define D_CMND_PILOTWIRE_MODE           "mode"
#define D_CMND_PILOTWIRE_LANG           "lang"
#define D_CMND_PILOTWIRE_OFFLOAD        "offload"
#define D_CMND_PILOTWIRE_MIN            "min"
#define D_CMND_PILOTWIRE_MAX            "max"
#define D_CMND_PILOTWIRE_TARGET         "target"
#define D_CMND_PILOTWIRE_NIGHT          "night"
#define D_CMND_PILOTWIRE_DRIFT          "drift"
#define D_CMND_PILOTWIRE_PULLUP         "pullup"

#define D_JSON_PILOTWIRE                "Pilotwire"
#define D_JSON_PILOTWIRE_MODE           "Mode"
#define D_JSON_PILOTWIRE_LABEL          "Label"
#define D_JSON_PILOTWIRE_MIN            "Min"
#define D_JSON_PILOTWIRE_MAX            "Max"
#define D_JSON_PILOTWIRE_TARGET         "Target"
#define D_JSON_PILOTWIRE_DRIFT          "Drift"
#define D_JSON_PILOTWIRE_TEMPERATURE    "Temperature"
#define D_JSON_PILOTWIRE_RELAY          "Relay"
#define D_JSON_PILOTWIRE_STATE          "State"

#define D_WEB_PILOTWIRE_CHECKED         "checked"

#define PILOTWIRE_TEMP_MIN              5
#define PILOTWIRE_TEMP_MAX              30
#define PILOTWIRE_TEMP_DEFAULT          18
#define PILOTWIRE_TEMP_STEP             0.5

#define PILOTWIRE_SHIFT_MIN             -10
#define PILOTWIRE_SHIFT_MAX             10

#define PILOTWIRE_DRIFT_DEFAULT         0
#define PILOTWIRE_DRIFT_STEP            0.1

#define PILOTWIRE_TEMP_THRESHOLD        0.3

#define PILOTWIRE_GRAPH_XMIN            5
#define PILOTWIRE_GRAPH_XMAX            95
#define PILOTWIRE_GRAPH_REFRESH         60          // refresh control page every 1mn (in seconds)
#define PILOTWIRE_GRAPH_DELAY           300         // collect temperature every 5mn (in seconds)
#define PILOTWIRE_GRAPH_SAMPLE          288         // 24 hours if every 5mn
#define PILOTWIRE_GRAPH_WIDTH           800      
#define PILOTWIRE_GRAPH_HEIGHT          500 
#define PILOTWIRE_GRAPH_PERCENT_START   15      
#define PILOTWIRE_GRAPH_PERCENT_STOP    85      

// english flag icon coded in base64
const char strFlagEn0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAACAAAAAgBAMAAACBVGfHAAAMtnpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHjazZlbcuRIrkT/YxWzhHg/lhMvmN0dzPLnIMhkpjSq6uq2+zHKkpiimEGEO+BwsMz+9/+J+RdfoaVsYio1t5wtX7HF5jtvqr2+rqOz8fw8X/7+E79/OW+eP3hOBY7h+jXv+/rO+fT+QIn3+fH1vCnzXqfeCzn75dZB76zv1x3kvVDw13l3/27a/YGeP7Zzf/t5L/va1rffYwGMlVgveON3cMGen/66UyCK0ELXM+dn5UJ7v0/89OEH/MwD3Q8APu++4WdfkYU3HNdCr23lbzjd5136dj48t/FfInL+ubP/jCgNu+3n1wd+IquK7Gt3PZJHLeZ7U6+tnHdcOIAznI9lXoXvxPtyXo1Xtd1OWFtsdRg7+KU5D+LioluuO3H7HKebhBj99oWj99OHc66G4pufh5SoLye+GPhZocLEhLnAaf/E4s59m96Pm1XuvBxXesdiyuiXl/l+4p++viwkomnunK0PVsTlNb8IQ5nTn1wFIU5uTNPB15nrYL9/KbEBBtOBubLBbse1xEjunVvh8BxsMlwa7VUvrqx7ASDi3olgXIABm0lpl50t3hfnwLHCTydyH6IfMOCSSX4RpY8hZMihGrg3nynuXOuTv04jLxCRQg4FaiggyIoxxUy9VVKomxRSTCnlVFJNLfUccswp";
const char strFlagEn1[] PROGMEM = "51yy6lQvocSSSi6l1NJKr6HGmmqupdbaam++BWQsmZZbabW11js37bGzVuf6zonhRxhxpJFHGXW00SfpM+NMM88y62yzL7/CQgLMyqusutrq221Saceddt5l1912F3JNgkRJkqVIlSb9Ye1m9Str35n7PWvuZs0fovS68maN06W8lnAqJ0k5gzEfHYwXZYCE9sqZrS5Gr8wpZ7b5YEJIniiTkrOcMgaDcTufxD3cvZn7JW8GdP8ub/4n5oxS9//BnFHqPpj7b95+YG31I7fhEKRVCKYoZKD8wKR2/kmYskq8ftEe9dNR4vB7jrFHm2tL6ivvZgpvdx7DSRhNdhGWXc3tKVLKlCllsysu3lFGTY7O3B2IpTxRspaGgNsQFpLp2DJ6mGWvLFwNIDKW7qBx43g+3Hapvszhrl8tW1xnWaTWSQrL6Pu0ZtmxXtedG7a9OMbiowCyrK0hJY3WRil9iONefpS9PdBtvo3kft/F7RxaKStKY7+DLcMG0c0lJawxawi0jVUkNwGguOMoSS8r4tMy9YVylO5tP7/Zv3EcCwiBxoB98amDbmmiN1jSmzRtKTkrTgGc1iQjNCcGsQL7GPbgk/amqivHalLaZfAmpRnqlBEkhCHkRB6A8gH3hErSyM9riemVzgsVzM4yUa7ripPm3Q7vhfVIjDUIR9qVnQBIsLKre30GE3ACW8MkZKikCJrFZ80YkRhI6JokwMfcGtqm3uiKLEsN0A3XFOcP";
const char strFlagEn2[] PROGMEM = "ToBeSeHqzW9zmOPYVJ20DKriR+4R3gKbrlqgg+xI1Ewo21QKmw2vQlgk7Aq9Tei3QslGKu7KLY2ktlRWzihQAHChsinanXFLBJ7NItO2e0DRY6N3UyJz0rr/GHTzQv0fg65IIzuGjsft/erkRA6UCdcTBN82EYkf0+6xQx6rUNto2Ua4ED5HwjVZovVMHylmTs3rMvvum8/GLgSXPP3Dk1RaTWi+JzVxRq+aRXlXtHfJ3kf06IHS6Q7ox4gYzJdTRCwACDE06jRSZhXyWRtFzVrCuaMTA3ayCcqqhk/NEE1RYUNTWFuLb/bAmtlN2ZSIUCO8aCqoqsbHKYQ60n68GSHNTrLIbJQ7UaCvbecvSZHeScHhTopDhC7oG+tFI5oVV1I0usXJCdGUKIP2pKwIFGjK/FrU9K15iVpdLzlLGo2w5kRc7SZ/244rX3Tmb3Sy2YtOMpvekVyLBYYU4oUkatcI9pDpR6cD4YXYTOt8RFJFvumm+pb+conYNi6PvtsKbIWqmpumlm2/axL5BNuxNf1l3WobgbccCksJohSO1pdKLZkJ1FLs13r+dev5qfOY37ceoNZWSSs8lbC9kqjG26pSEiMsYBSwCeaQlJSkzSzplmYnWEpdRBNapieBLCVBSgI9KNI3UH3wAR0th520NMxUJIlQkYSNVBRJ0mmAY0R2gXqD4+gKO2Bq89j1gBleYHhfBet3gbFdoEBe7SGonJ2LRgU/WrqmPftjFO1P";
const char strFlagEn3[] PROGMEM = "/dJgn/I17/qlfFUF64nX14ji5AwhsrLOGZPrIGGFeXgAQy7rZAoVwfaZRUjx6rU1HcpGsKtmygs1pDGi1s7VKhnHQurce+l1v95uHxh1xjCaPANeT5VlzZUscDpwN67JqXBQjrIIlgY4NKN0TyTFJWtaca15k7g71IZ490aodYPqRNxmSHRs2CIe7E7WrPLaW7lO6Stap5Y8DlS7EWpoW8zGuPwDKUuboLlLpXrIX3Crl2Vhce3P2HyPIDmsAEGwjdCpdRPVq2mvnnv3L8K03sI00u+EqQVCMHHFFFqa28+8rNo+crLdDgWKW/8rAbn0g+rXBVmZbGVnbebG1Euy7KCysqkqf/cdPCEKRy1pz/E7eQTldDtFxGicrhcaCfmyEbdaMSBaRVpHLHWIUZl8sKN8JTm1V1QZaiBhdaR22a4fhXLYmNr3lYTvHBzRw4YvsksJRRW0V+OQHNnAwGawD2jgyT5Vm0VJkAOPxbV/4XCfozlvSL+KVXeEFeJHK/Zv/6OdOHO/ejXjQM3eyV293sz8yd2s/Fzan0fzNy3jL4//4wt9l7M/JMwqWWYo5KTS/N5M57sWUIZcH/cEYal8EnYWM39CmCbtuHUk+KiZT1s91nVoxW3KyXRRQ17VWR4bu/opnXTl9NALTzHgjhBNLtIi0Yco4ZTEKQ/NYaNpzp+5NT3liJWewVW9ljsNbQ5GzjKsTh+0ZVyFdrXjPaj+pUaL+mU87jq2UC2xMmCi";
const char strFlagEn4[] PROGMEM = "ATR4pKVSprH3VLFwNVvMxB1cn5eAMHnk25eaH6aBr76URTc9cx8jRZUkj3lfntuhQvwlqDux3ahdUNcx2ArASncLmcafINk0ITnjVkCzJ73talVT1arRbawOuqq12w/jHDJb2Rdie2n7oQcLR7O68cNBosBHZqDDukugVKuixs9VAzcCrm8yzvD8kM2iHimvDIt4mNOIhDAI0sKX5gCRYaewrNus+QyQ9Keq5rjRcLQ50RNvw6MDJCbZXgMknWWeAVIzFYGVkFa9E1IGMuqqvdyVergZ6XAo/dnzbjMgq+uMANqX1VNwZcOk0jrIBGcwi64u/nbalbaj5kvAQ8XSUlZ/Un90hL2mrOVVApfwCUP/4k6049F0WAPj0txJMf5N+aWofS9wc644/mflnuj4OpPNnEogRAhT9raGu2ObfpcFlkktStJRZB4sYWsyZRPk6U4kjwZLdIs8YGKku65aiJU+QbsvxNrUIH2Mb+0Z38yx6rvQztRIu+uhzKX/dP5wT9OYxIaVYW/M28J8qN54KGOM/FCNP/J26iTmqWIcX2AOI1Y10RC/Hqc78c/1MljrmsIqxaJ1u89DCn0SAZwT7OmaZxbGQhJoUDL21A8V0nrfJQ4G41XhbmZtSHTuqliaA+KeN4g6tYbPee7xg5RiJ+XdBNqoTVvNiU5CGhFFbxiFcCfU9TOJLB8FGz3DGW1mSm4yX+RTDe492jLmk23PaGt+KyE62r4mKZ1sddIJ";
const char strFlagEn5[] PROGMEM = "OrQMXVMnHW4BQciracfm3LMZ9ndFjEvLZx4N5LvGqj5eT+h7j9HHozJJom5F57zLepnHe8XjbM9jgvRoU/hZmjRRn+c0+HfMrYFErNpZ9JpOpC0yseJMBuUmalp4YXXDa5Z55ZaP7PEWD9zIJz7p3ghix+xQdFQYy2/XdaTStBAVj/Nc603mOGSas3sPmT8U7+XsFPtTC0qlv8bUqxQOmbnpTElfe56oIeyvB0h5WuY8aYD5Hm3Zydi/dKbmizX9OtrqEV95dHFWAHaLLYJk/3xW0POZ3sw9vp2nGScpzjT8JSfYb/K0L8QR3vf3pNCkLLjahHg4DCYDA1iwFaZ8NiXprldUov+1pdPHPq9BuC8SkXaCST7lrwDtGOYZwZW2oc8cm87Co6ennC/EBoZ96pY/MC/29fwIk/zHoJsf54E/B/31GRLyg4Dz3C7pf5x5rcOlqfd6cod3obOqWdezXvTFdMtmtatd1o/YPSK6/5mFJPUZIINx2uORBqR6MR6CIW1PdYhvaRaUhyYpaqoDpE72Oq0zks/nwZw+5Esmz/Owgk5RlIfjxc5TaUfRn2njeibN8VTZGm88mYGYV/7UH93P7e751el923kgXYrOti/jYVSauDmGQbh1OrZE/8vvVtDyuy7E4elB5s+a0I/ZTGjY4v8AUAVtuWPsRrQAAAGFaUNDUElDQyBwcm9maWxlAAB4nH2RPUjDUBSFT1OLIhVBO4g4ZKhOFkRFHbUK";
const char strFlagEn6[] PROGMEM = "RagQaoVWHUxe+gdNGpIUF0fBteDgz2LVwcVZVwdXQRD8AXFxdVJ0kRLvSwotYrzweB/n3XN47z5AqJeZZnWMAZpum6lEXMxkV8XOV4TRhwBCmJaZZcxJUhK+9XVPnVR3MZ7l3/dn9ag5iwEBkXiWGaZNvEE8tWkbnPeJI6woq8TnxKMmXZD4keuKx2+cCy4LPDNiplPzxBFisdDGShuzoqkRTxJHVU2nfCHjscp5i7NWrrLmPfkLwzl9ZZnrtIaQwCKWIEGEgipKKMNGjHadFAspOo/7+Addv0QuhVwlMHIsoAINsusH/4Pfs7XyE+NeUjgOhF4c52MY6NwFGjXH+T52nMYJEHwGrvSWv1IHZj5Jr7W06BHQuw1cXLc0ZQ+43AEGngzZlF0pSEvI54H3M/qmLNB/C3SveXNrnuP0AUjTrJI3wMEhMFKg7HWfd3e1z+3fnub8fgBK4XKXYFI09AAAABtQTFRFYQAA2QAmAFO2cobC3XN4prHT5bG16tbZ7+vp9gGK9QAAAAF0Uk5TAEDm2GYAAAABYktHRACIBR1IAAAACXBIWXMAAC4jAAAuIwF4pT92AAAAB3RJTUUH4wwSAQ8U/2QTHAAAANRJREFUKM+dkr0KAjEQhKMiWJomtUSuFw9fQVuJcKQNeHAPYGFpk7s8tjP5UTRWbpPsl8ntZvaEiBGkDOId8y3BfvPKdQK6EFNAk/KVL6BbR6CCSaBxA/OlG3wCnQpnAI31";
const char strFlagEn7[] PROGMEM = "QnDAiSaA0hPgwGjWpISAAlQGgITgCAHAom3b7kRw99juREgBkCKDScp7BvIr/gHVR6uybMxOBLc+NsbWw5VgVCa/xY4qXnE9AZ6PHYGlhH5gkwyihBZiTRbihBYKKLPJwUXXZ30Zg338HlQ9ymrYH7/DE1YIbsUViqSPAAAAAElFTkSuQmCC";
const char *const arrFlagEnBase64[] PROGMEM = {strFlagEn0, strFlagEn1, strFlagEn2, strFlagEn3, strFlagEn4, strFlagEn5, strFlagEn6, strFlagEn7};

// french flag icon coded in base64
const char strFlagFr0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAACAAAAAgBAMAAACBVGfHAAAHdXpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHjarVdbkisrDvzXKmYJvIRgObwUcXcwy58UlN3dbrtP3zO2w1UYlwFlJkpE67//KP0Hr+hCpsRScs3Z4ZVqqqGhUdx5nbt3aV/3K1w/4fuXfrr/ENAVcY/na17X8w39/PEHSVd//9pPMq5xyjXQ9cNtwGgz22zzWuQ1UAyn31/fqV5LavlTONcnyonk9vDj9yQAYzI6Y6CwogdYdg1npohVxBqb9exrwYPuaidcOcbv+NEduicA3lsP+Llx9ccPOM5At7DyA05Xv+fn+G2UPq/Ih/vM4fOKenPNfX59wk91FtV1omsJOqopX0HdQtktPNgB50Ej4y34MNqy3xXvgmkGgJ8ItZPr+FJ9AOLqk5++efVr34cfWGIKKwjuIYwQd1+JEmoYm5Rkb69BCPzMWMDJAHMR3eG+Fr/nrTYfJiuYeXo8GTwGM0a/vOmx42/fXwZSNZl778odK6wrmL6wDGPOrngKhHi9MOWNr6dzc48vIzaCQd4wFwTYXD9DdPYf2oqb5+iY8GhyR/Je5jUAIMLcjMX4CAZc9pF99k5CEO+BYwE/DSsPMYUOBjwTh4lVhhRjBjnYDZgb/xG/nw0cTjfSC4jgmKOAGmwgkJUSp4z9ViChRhw5MXNm4cKVW445Zc45S7Y81SRKEpYsIkWq";
const char strFlagFr1[] PROGMEM = "tBJLKlxykVJKLa2GGpHGmGquUkuttTVM2lLDWA3PN3T00GNPnXvu0kuvvQ3IZ6TBIw8ZZdTRZphxIgXQzFNmmXW25RektNLilZessupqCq1p1KSsWUWLVm131i5Wv7L2yNzPrPmLtbCJsufkgzV0i9yG8JZO2DgDYyF5MC7GAAQdjDNXfErBmDPOXA2RYuSAVbKRM70xBgbT8oHV37n7YO4lbwR0/y1v4RlzZNS9gzky6j4x9523J6zNttNt3ATZLgSmyJAR22+5VVooTVuPQ5w14U9P79hHcIKCdWubrdfouWB46hMLn2OGkuJsUxuXcMasC5nq1ZhYN3DoqzMeLKt3Wj0Lr7HmQkC6RHd6WGO0XvClp1hVpY4+el5VJp7JvfsyE+fOOU38mzEWyZRY+5wYes6sfeqokmMYS0PSNbpm0T4AWu6a7KmsUea4lp2KumRtKs3s2jp/ca+a2GwWF7t7D5LyZEw5abcWZIOpIyJryPvFa8evOtM1b3BLXzOwQaIFHexW6b5qTqB3+WRIARDVvGBec8Qz4gLIw4UeAE+zwHrQMeJqMgggA8AMzCeGSNrr8hBxDaIQZY86lwQNRudS6VLbGoDYYoO2hqw0dpzEdriwmJ/cLXuiEX0/PeEWDg4UW0Qr1dMYjcaUBamr2jkD4swdbLay4eQFx1yv59FoaMdqDZric0TkFbtggBZg3IHKNJRmL/mMOBz20I0sZ/IBSpioZtOQzKS0NTTx";
const char strFlagFr2[] PROGMEM = "zwwpQUMq8NxLRPj9iAi0mogMTI08sVN3jH3hIDFq+a4jbHssFYmr3CafBxX/TDpflEP/h3S+KIc+SWcLx86zdv0mnkeVP4iJtpreICbaX/Jeb+a4ak1u3GbN8wBTbh0WT8+IB6vZDaRP1u6R+2gipQ2HpIfIVjsakeqQn+9kP95lDGFFHt2NwWVoD5ZGAKro6ICmixyUfD0jwgfzS+VbcEgrXLGqTBMLyV4ZGUclQ2fCGFI08NaIxj5qxIw6qy5F6sKmQLIH0H1MRVbunnm1QSwtCdwlAB0FTB8TxnyQSbcOmJJyR0C2BGvAf5odbjEFMiQOtbKmIBXy0UkfBct4mdz6yrZl5TRWGFlmjYu6Fo6KjSo4g3ZDqIsbZ8TitLvXqsRGmdmu2tIiv04Sq1vWWtKK+fTgsGhetiaM5dpa1WnJHzopn3RCvxbKH3RCvxbKF50YAGKJ+K4T+kkotzv7ExrMB7bcn8ZG/k2xkf+72L7tAXJvio3mm2Ij9u/hjeRNsVF+U2yU+D28Ef8uNiQx7LimyMpXfA3pZRz3so1Meyf/xtzv3m5l6nd3p39h79/cHdnp5u+TXtn74/3YvZ0ZZxCnBtTYmMtAMdiF0FGOKpDV8rxysB2UU1ze9TsQf7Bxeubjf2Pj9MzH/8bG6dHPz2nF7BwwbjdXNjev281xfq6KVA87PxMcQ58aaDv6PI4e1rjkahYVIJxshVOY7l4BvBQK/V4pPwuFfq+Un4VC";
const char strFlagFr3[] PROGMEM = "u2HyaAxVsF7m3cy80zZvO+thx8M0bYcd7156vDsf78aBkdy30+Bn+CvAxxbj5vIfHJweLPyvHZweLPzRwX/ybzPrdAmlU91KASSfirFz7rNKBBltywSVNRu3hhymu8iTiXxXo3ttR9dd2znsfETyOn2SvunsR3/y9Fv6tAPeqRiA3i461y46wy46p1A+FUPs97Kzn7KTT9kZPsrOnqOG/GL70w8HqZ2GAEy6pV2rq3d5nrUNTNS5WHUOibJSmwOP5V9U6FfC8VDowl8qgkqWDo1IURo+9hggJIfYrFy/yO5tdgRoXJ8Ua4jtylzsZLcxsqIq7KJqKkm8QXQVVf0UVXyKKjslP1bmzxSuhG1d6X+N3icafyleCgAAAYVpQ0NQSUNDIHByb2ZpbGUAAHicfZE9SMNQFIVPU4siFUE7iDhkqE4WREUdtQpFqBBqhVYdTF76B00akhQXR8G14ODPYtXBxVlXB1dBEPwBcXF1UnSREu9LCi1ivPB4H+fdc3jvPkCol5lmdYwBmm6bqURczGRXxc5XhNGHAEKYlpllzElSEr71dU+dVHcxnuXf92f1qDmLAQGReJYZpk28QTy1aRuc94kjrCirxOfEoyZdkPiR64rHb5wLLgs8M2KmU/PEEWKx0MZKG7OiqRFPEkdVTad8IeOxynmLs1ausuY9+QvDOX1lmeu0hpDAIpYgQYSCKkoow0aMdp0UCyk6j/v4B12/RC6FXCUwciygAg2y";
const char strFlagFr4[] PROGMEM = "6wf/g9+ztfIT415SOA6EXhznYxjo3AUaNcf5PnacxgkQfAau9Ja/UgdmPkmvtbToEdC7DVxctzRlD7jcAQaeDNmUXSlIS8jngfcz+qYs0H8LdK95c2ue4/QBSNOskjfAwSEwUqDsdZ93d7XP7d+e5vx+AErhcpdgUjT0AAAAG1BMVEVQW5geR5oRS5whSZwkS57vGSApTqHwHCj4+vdcwbDgAAAAAXRSTlMAQObYZgAAAAFiS0dEAIgFHUgAAAAJcEhZcwAACxMAAAsTAQCanBgAAAAHdElNRQfjDBIBDjdEGFMvAAAAXElEQVQoz2NgAIMOIGBAAGYNkEB7AJxvDBEIhYkYwwRCIXwWhABEiQtCAKyEBVkApCQFWSAUiwAbqkAAEQJpqAKhAyVAhtMJhwdmEGIEMmY0YEQUZlRiRDZKcgAALEaZ0815i7UAAAAASUVORK5CYII=";
const char *const arrFlagFrBase64[] PROGMEM = {strFlagFr0, strFlagFr1, strFlagFr2, strFlagFr3, strFlagFr4};

// control page texts
const char *const arrControlTemp[]   PROGMEM = {"Target", "Thermostat"};
const char *const arrControlSet[]    PROGMEM = {"Set to", "Définir à"};
const char *const arrControlStatus[] PROGMEM = {"Currently heating", "Radiateur en chauffe"};

// enumarations
enum PilotwireModes { PILOTWIRE_DISABLED, PILOTWIRE_OFF, PILOTWIRE_COMFORT, PILOTWIRE_ECO, PILOTWIRE_FROST, PILOTWIRE_THERMOSTAT, PILOTWIRE_OFFLOAD };
enum PilotwireSources { PILOTWIRE_SOURCE_NONE, PILOTWIRE_SOURCE_LOCAL, PILOTWIRE_SOURCE_REMOTE };
enum PilotwireLangages { PILOTWIRE_LANGAGE_ENGLISH, PILOTWIRE_LANGAGE_FRENCH };

// fil pilote commands
enum PilotwireCommands { CMND_PILOTWIRE_MODE, CMND_PILOTWIRE_MIN, CMND_PILOTWIRE_MAX, CMND_PILOTWIRE_TARGET, CMND_PILOTWIRE_NIGHT, CMND_PILOTWIRE_DRIFT};
const char kPilotWireCommands[] PROGMEM = D_CMND_PILOTWIRE_MODE "|" D_CMND_PILOTWIRE_MIN "|" D_CMND_PILOTWIRE_MAX "|" D_CMND_PILOTWIRE_TARGET "|" D_CMND_PILOTWIRE_NIGHT "|" D_CMND_PILOTWIRE_DRIFT;

// header of publicly accessible control page
const char PILOTWIRE_MODE_SELECT[] PROGMEM = "<input type='radio' name='%s' id='%d' value='%d' %s>%s<br>\n";

// form topic style
const char PILOTWIRE_TOPIC_STYLE[] PROGMEM = "style='float:right;font-size:0.7rem;'";

/*************************************************\
 *               Variables
\*************************************************/

// variables
bool     pilotwire_nightmode;
uint8_t  pilotwire_temperature_source;         // temperature source
uint8_t  pilotwire_langage;
uint32_t pilotwire_graph_index;
uint32_t pilotwire_graph_counter;
float    pilotwire_current_temperature;
float    pilotwire_current_target;
uint8_t  pilotwire_current_state;
float    arr_temperature[PILOTWIRE_GRAPH_SAMPLE];
float    arr_target[PILOTWIRE_GRAPH_SAMPLE];
uint8_t  arr_state[PILOTWIRE_GRAPH_SAMPLE];

/**************************************************\
 *                  Accessors
\**************************************************/

// get state label
void PilotwireGetStateLabel (uint8_t state, String& str_label)
{
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
}

// get state color
void PilotwireGetStateColor (uint8_t state, String& str_color)
{
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
  // handle 1 relay device state conversion
  if (devices_present == 1)
  {
    if (new_mode == PILOTWIRE_ECO) new_mode = PILOTWIRE_COMFORT;
    else if (new_mode == PILOTWIRE_FROST) new_mode = PILOTWIRE_OFF;
  }

  // if within range, set mode
  if (new_mode <= PILOTWIRE_OFFLOAD) Settings.weight_reference = (unsigned long) new_mode;
}

// set pilot wire minimum temperature
void PilotwireSetMinTemperature (float new_temperature)
{
  // if within range, save temperature correction
  if ((new_temperature >= PILOTWIRE_TEMP_MIN) && (new_temperature <= PILOTWIRE_TEMP_MAX)) Settings.weight_item = (unsigned long) int (new_temperature * 10);
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

    // reset night mode
    pilotwire_nightmode = false;
  }
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

// set night dropdown temperature
void PilotwireSetNightDropdown (float new_dropdown)
{
  // save target temperature
  if (new_dropdown <= PILOTWIRE_SHIFT_MAX) Settings.energy_voltage_calibration = (unsigned long) int (new_dropdown * 10);
}

// get night dropdown temperature
float PilotwireGetNightDropdown ()
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

  // read temperature from sensor
  pilotwire_temperature_source = PILOTWIRE_SOURCE_LOCAL;
  index = ds18x20_sensor[0].index;
  if (ds18x20_sensor[index].valid) temperature = ds18x20_sensor[index].temperature;

  // if not available, read MQTT temperature
  if (isnan (temperature))
  {
    pilotwire_temperature_source = PILOTWIRE_SOURCE_REMOTE;
    temperature = TemperatureGetValue ();
  }

  // if temperature is available, apply correction
  if (!isnan (temperature)) temperature += PilotwireGetDrift ();
  else pilotwire_temperature_source = PILOTWIRE_SOURCE_NONE;

  return temperature;
}

// get current target temperature (handling night dropdown)
float PilotwireGetCurrentTarget ()
{
  float  temp_target;

  // get target temperature
  temp_target = PilotwireGetTargetTemperature ();

  // if night time, substract night dropdown
  if (pilotwire_nightmode == true) temp_target -= PilotwireGetNightDropdown ();

  return temp_target;
}

void PilotwireHandleTimer (uint32_t state)
{
  // set night mode according to relay state (OFF = night mode)
  pilotwire_nightmode = (state == POWER_OFF);
}

// Show JSON status (for MQTT)
void PilotwireShowJSON (bool append)
{
  uint8_t  actual_mode;
  float    actual_temperature;
  String   str_json, str_temperature, str_label;

  // get temperature and mode
  actual_temperature = PilotwireGetTemperature ();
  actual_mode  = PilotwireGetMode ();
  PilotwireGetStateLabel (actual_mode, str_label);
 
  // start message  -->  {  or message,
  if (append == false) str_json = "{";
  else str_json = String (mqtt_data) + ",";

  // pilotwire mode  -->  "PilotWire":{"Relay":2,"Mode":5,"Label":"Thermostat",Offload:"ON","Temperature":20.5,"Target":21,"Min"=15,"Max"=25}
  str_json += "\"" + String (D_JSON_PILOTWIRE) + "\":{\"" + String (D_JSON_PILOTWIRE_RELAY) + "\":" + String (devices_present);
  str_json += ",\"" + String (D_JSON_PILOTWIRE_MODE) + "\":" + String (actual_mode) + ",\"" + String (D_JSON_PILOTWIRE_LABEL) + "\":\"" + str_label + "\"";

  // if defined, add current temperature
  if (actual_temperature != NAN) str_json += ",\"" + String (D_JSON_PILOTWIRE_TEMPERATURE) + "\":" + String (actual_temperature);

  // thermostat data
  if (actual_mode == PILOTWIRE_THERMOSTAT)
  {
    str_json += ",\"" + String (D_JSON_PILOTWIRE_TARGET) + "\":" + String (PilotwireGetTargetTemperature ());
    str_json += ",\"" + String (D_JSON_PILOTWIRE_DRIFT) + "\":" + String (PilotwireGetDrift ());
    str_json += ",\"" + String (D_JSON_PILOTWIRE_MIN) + "\":" + String (PilotwireGetMinTemperature ());
    str_json += ",\"" + String (D_JSON_PILOTWIRE_MAX) + "\":" + String (PilotwireGetMaxTemperature ());
  }

  str_json += "}";

  // if pilot wire mode is enabled
  if (actual_mode != PILOTWIRE_DISABLED)
  {
    // get mode and associated label
    actual_mode = PilotwireGetRelayState ();
    PilotwireGetStateLabel (actual_mode, str_label);

    // relay state  -->  ,"State":{"Mode":4,"Label":"Comfort"}
    str_json += ",\"" + String (D_JSON_PILOTWIRE_STATE) + "\":{\"" + String (D_JSON_PILOTWIRE_MODE) + "\":" + String (actual_mode);
    str_json += ",\"" + String (D_JSON_PILOTWIRE_LABEL) + "\":\"" + str_label + "\"}";
  }
  
  // if not in append mode, add last bracket 
  if (append == false) str_json += "}";

  // add json string to MQTT message
  snprintf_P (mqtt_data, sizeof(mqtt_data), str_json.c_str ());

  // if not in append mode, publish message 
  if (append == false) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
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
    case CMND_PILOTWIRE_NIGHT:  // set night dropdown 
      PilotwireSetNightDropdown (atof (XdrvMailbox.data));
      break;
    default:
      command_handled = false;
  }

  // if needed, send updated status
  if (command_handled == true) PilotwireShowJSON (false);
  
  return command_handled;
}

// update pilot wire relay states according to current status
void PilotwireUpdateStatus ()
{
  bool    is_offloaded;
  uint8_t heater_mode, heater_state, target_state;
  float   actual_temperature, target_temperature;

  // get heater mode and state
  heater_mode  = PilotwireGetMode ();
  heater_state = PilotwireGetRelayState ();

  // if pilotwire mode is enabled
  if (heater_mode != PILOTWIRE_DISABLED)
  {
    is_offloaded = OffloadingIsOffloaded ();

    // if offload mode, target state is off
    if (is_offloaded == true) target_state = PILOTWIRE_OFF;
 
    // else if thermostat mode
    else if (heater_mode == PILOTWIRE_THERMOSTAT)
    {
      // get current and target temperature
      actual_temperature = PilotwireGetTemperature ();
      target_temperature = PilotwireGetCurrentTarget ();

      // if temperature is too low, target state is on
      // else, if too high, target state is off
      target_state = heater_state;
      if (actual_temperature < (target_temperature - PILOTWIRE_TEMP_THRESHOLD)) target_state = PILOTWIRE_COMFORT;
      else if (actual_temperature > (target_temperature + PILOTWIRE_TEMP_THRESHOLD)) target_state = PILOTWIRE_OFF;
    }

    // else target mode is heater mode
    else target_state = heater_mode;

    // if target state is different than heater state, change state
    if (target_state != heater_state)
    {
      // allow relay command
      offloading_device_allowed = true;

      // set relays
      PilotwireSetRelayState (target_state);

      // disallow relay command
      offloading_device_allowed = false;
      
      // publish new state
      PilotwireShowJSON (false);
    }
  }
}

void PilotwireUpdateGraph ()
{
  // if index is ok, check index set current graph value
  arr_temperature[pilotwire_graph_index] = pilotwire_current_temperature;
  arr_target[pilotwire_graph_index] = pilotwire_current_target;
  arr_state[pilotwire_graph_index] = pilotwire_current_state;

  // init current values
  pilotwire_current_temperature = NAN;
  pilotwire_current_target      = NAN;
  pilotwire_current_state       = PILOTWIRE_OFF;

  // increase temperature data index and reset if max reached
  pilotwire_graph_index ++;
  pilotwire_graph_index = pilotwire_graph_index % PILOTWIRE_GRAPH_SAMPLE;
}

void PilotwireEverySecond ()
{
  float   temperature, target;
  uint8_t state;

  // update current values
  temperature = PilotwireGetTemperature ( );
  target = PilotwireGetCurrentTarget ( );
  state = PilotwireGetRelayState ( );

  // update current temperature
  if (isnan(temperature) == false)
  {
    if (isnan(pilotwire_current_temperature) == false) pilotwire_current_temperature = min (pilotwire_current_temperature, temperature);
    else pilotwire_current_temperature = temperature;
  }

  // update target temperature
  if (isnan(target) == false)
  {
    if (isnan(pilotwire_current_target) == false) pilotwire_current_target = min (pilotwire_current_target, target);
    else pilotwire_current_target = target;
  } 

  // update relay state
  if (pilotwire_current_state != PILOTWIRE_COMFORT) pilotwire_current_state = state;

  // increment delay counter and if delay reached, update history data
  if (pilotwire_graph_counter == 0) PilotwireUpdateGraph ();
  
  // increment and reset counter in case of overflow
  pilotwire_graph_counter ++;
  pilotwire_graph_counter = pilotwire_graph_counter % PILOTWIRE_GRAPH_DELAY;
}

void PilotwireInit ()
{
  int    index;
  String str_setting;

  // init default values
  pilotwire_nightmode           = false;
  pilotwire_current_temperature = NAN;
  pilotwire_current_target      = NAN;
  pilotwire_current_state       = PILOTWIRE_OFF;
  pilotwire_graph_index         = 0;
  pilotwire_graph_counter       = 0;
  pilotwire_langage             = PILOTWIRE_LANGAGE_ENGLISH;
  pilotwire_temperature_source  = PILOTWIRE_SOURCE_NONE;

  // initialise temperature graph
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    arr_temperature[index] = NAN;
    arr_target[index] = NAN;
    arr_state[index] = PILOTWIRE_OFF;
  }

  // if needed, initialise settings
  str_setting = (char*)Settings.free_f03;
  index = str_setting.indexOf ('|');
  if (index == -1)
  {
    // offloading and temperature settings
    strcpy ((char*)Settings.free_f03, ";|;");

    // default values
    PilotwireSetDrift (PILOTWIRE_DRIFT_DEFAULT);
    PilotwireSetMinTemperature (PILOTWIRE_TEMP_MIN);
    PilotwireSetMaxTemperature (PILOTWIRE_TEMP_MAX);
    PilotwireSetTargetTemperature (PILOTWIRE_TEMP_DEFAULT);
  } 
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// Pilotwire langage flag
void PilotwireWebDisplayFlag (uint8_t langage_code)
{
  uint8_t nbrItem, index;

  // display flag according to langage
  WSContentSend_P (PSTR ("<img  src='data:image/png;base64,"));
  switch (langage_code)
  {
    case PILOTWIRE_LANGAGE_ENGLISH:  // english flag
      nbrItem = sizeof (arrFlagEnBase64) / sizeof (char*);
      for (index=0; index<nbrItem; index++) { WSContentSend_P (arrFlagEnBase64[index]); }
      break;
    case PILOTWIRE_LANGAGE_FRENCH:  // french flag
      nbrItem = sizeof (arrFlagFrBase64) / sizeof (char*);
      for (index=0; index<nbrItem; index++) { WSContentSend_P (arrFlagFrBase64[index]); }
      break;
  }
  WSContentSend_P (PSTR ("' />"));
}

// Pilotwire mode selection 
void PilotwireWebSelectMode ()
{
  uint8_t actual_mode;
  float   actual_temperature;
  String  str_argument, str_label, str_temp_target;
 
  // get mode and temperature
  actual_mode = PilotwireGetMode ();
  actual_temperature = PilotwireGetTemperature ();

  str_argument = "";
  if (actual_mode == PILOTWIRE_DISABLED) str_argument = D_WEB_PILOTWIRE_CHECKED; 
  WSContentSend_P (PSTR ("<span %s>%s</span>"), PILOTWIRE_TOPIC_STYLE, D_CMND_PILOTWIRE_MODE);
  WSContentSend_P (PILOTWIRE_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_DISABLED, PILOTWIRE_DISABLED, str_argument.c_str (), D_PILOTWIRE_DISABLED);

  // selection : off
  str_argument = "";
  if (actual_mode == PILOTWIRE_OFF) str_argument = D_WEB_PILOTWIRE_CHECKED;
  WSContentSend_P (PILOTWIRE_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_OFF, PILOTWIRE_OFF, str_argument.c_str (), D_PILOTWIRE_OFF);

  // if dual relay device
  if (devices_present > 1)
  {
    // selection : no frost
    str_argument = "";
    if (actual_mode == PILOTWIRE_FROST) str_argument = D_WEB_PILOTWIRE_CHECKED;
    WSContentSend_P (PILOTWIRE_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_FROST, PILOTWIRE_FROST, str_argument.c_str (), D_PILOTWIRE_FROST);

    // selection : eco
    str_argument = "";
    if (actual_mode == PILOTWIRE_ECO) str_argument = D_WEB_PILOTWIRE_CHECKED;
    WSContentSend_P (PILOTWIRE_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_ECO, PILOTWIRE_ECO, str_argument.c_str (), D_PILOTWIRE_ECO);
  }

  // selection : comfort
  str_argument = "";
  if (actual_mode == PILOTWIRE_COMFORT) str_argument = D_WEB_PILOTWIRE_CHECKED;
  WSContentSend_P (PILOTWIRE_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_COMFORT, PILOTWIRE_COMFORT, str_argument.c_str (), D_PILOTWIRE_COMFORT);

  // selection : thermostat
  if (isnan(actual_temperature) == false) 
  {
    // selection : target temperature
    str_argument = "";
    if (actual_mode == PILOTWIRE_THERMOSTAT) str_argument = D_WEB_PILOTWIRE_CHECKED;
    WSContentSend_P (PILOTWIRE_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_THERMOSTAT, PILOTWIRE_THERMOSTAT, str_argument.c_str (), D_PILOTWIRE_THERMOSTAT); 
  }
}

// append pilotwire control button to main page
void PilotwireWebMainButton ()
{
  // heater configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_PILOTWIRE_CONTROL, D_PILOTWIRE_CONTROL);
}

// Pilot Wire configuration button
void PilotwireWebButton ()
{
  // heater configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_PILOTWIRE_CONFIG, D_PILOTWIRE_CONFIGURE);
}

// append pilot wire state to main page
bool PilotwireWebSensor ()
{
  uint8_t actual_mode, actual_state;
  float   temperature;
  String  str_title, str_text, str_label, str_color, str_target;

  // get current mode and temperature
  actual_mode = PilotwireGetMode ();
  temperature = PilotwireGetTemperature ();

  // handle command
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
  str_target = String (PilotwireGetCurrentTarget (), 1);
  if (actual_mode == PILOTWIRE_THERMOSTAT) str_text += " / " + str_target;
  str_text += "°" + String (TempUnit ());

  // display temperature
  WSContentSend_PD ("{s}%s{m}%s{e}", str_title.c_str (), str_text.c_str ());

  // display day or night night mode
  str_label = String (PilotwireGetNightDropdown (), 1);
  str_title = D_PILOTWIRE_MODE;
  if (pilotwire_nightmode == true) str_text = "<span><b>" + String (D_PILOTWIRE_NIGHT) + "</b> (-" + str_label.c_str() + "°" + String (TempUnit ()) + ")</span>";
  else str_text = "<span><b>" + String (D_PILOTWIRE_NORMAL) + "</b></span>";
  WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), str_title.c_str (), str_text.c_str ());

  // get heater state, associated label and color
  actual_state = PilotwireGetRelayState ();
  PilotwireGetStateLabel (actual_state, str_label);
  PilotwireGetStateColor (actual_state, str_color);

  // display current state
  str_title = D_PILOTWIRE_HEATER + String (" ") + D_PILOTWIRE_STATE;
  str_text  = "<span style='color:" + str_color + ";'><b>" + str_label + "</b></span>";
  WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), str_title.c_str (), str_text.c_str ());
}

// Pilotwire heater configuration web page
void PilotwireWebPageConfigure ()
{
  uint8_t target_mode;
  float   actual_temperature;
  char    argument[PILOTWIRE_BUFFER_SIZE];
  String  str_temperature, str_temp_target, str_temp_step, str_drift_step, str_unit, str_pullup;

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

    // get minimum temperature according to 'min' parameter
    WebGetArg (D_CMND_PILOTWIRE_MIN, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetMinTemperature (atof (argument));

    // get maximum temperature according to 'max' parameter
    WebGetArg (D_CMND_PILOTWIRE_MAX, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetMaxTemperature (atof (argument));

    // get target temperature according to 'target' parameter
    WebGetArg (D_CMND_PILOTWIRE_TARGET, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetTargetTemperature (atof (argument));

    // get night dropdown temperature according to 'night' parameter
    WebGetArg (D_CMND_PILOTWIRE_NIGHT, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetNightDropdown (atof (argument));

    // get temperature drift according to 'drift' parameter
    WebGetArg (D_CMND_PILOTWIRE_DRIFT, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetDrift (atof (argument));

    // set ds18b20 pullup according to 'pullup' parameter
    WebGetArg (D_CMND_PILOTWIRE_PULLUP, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetDS18B20Pullup (true);
    else PilotwireSetDS18B20Pullup (false);
  }

  // beginning of form
  WSContentStart_P (D_PILOTWIRE_CONFIGURE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR("<form method='get' action='%s'>\n"), D_PAGE_PILOTWIRE_CONFIG);

  // operation mode 
  // --------------
  WSContentSend_P (PSTR("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n"), D_PILOTWIRE_MODE);

  // mode selection
  PilotwireWebSelectMode ();

  // if temperature is available
  if (isnan (actual_temperature) == false) 
  {
    // target temperature
    str_temperature = String (PilotwireGetTargetTemperature ());
    WSContentSend_P (PSTR ("<p>%s (°%s)<span %s>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%s' value='%s'></p>\n"), D_PILOTWIRE_TARGET, str_unit.c_str (), PILOTWIRE_TOPIC_STYLE, D_CMND_PILOTWIRE_TARGET, D_CMND_PILOTWIRE_TARGET, PILOTWIRE_TEMP_MIN, PILOTWIRE_TEMP_MAX, str_temp_step.c_str(), str_temperature.c_str());

    // night temperature dropdown
    str_temperature = String (PilotwireGetNightDropdown ());
    WSContentSend_P (PSTR ("<p>%s (°%s)<span %s>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%s' value='%s'></p>\n"), D_PILOTWIRE_NIGHT, str_unit.c_str (), PILOTWIRE_TOPIC_STYLE, D_CMND_PILOTWIRE_NIGHT, D_CMND_PILOTWIRE_NIGHT, 0, PILOTWIRE_SHIFT_MAX, str_temp_step.c_str(), str_temperature.c_str());

    // temperature correction label and input
    str_temperature = String (PilotwireGetDrift ());
    WSContentSend_P (PSTR ("<p>%s (°%s)<span %s>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%s' value='%s'></p>\n"), D_PILOTWIRE_DRIFT, str_unit.c_str (), PILOTWIRE_TOPIC_STYLE, D_CMND_PILOTWIRE_DRIFT, D_CMND_PILOTWIRE_DRIFT, PILOTWIRE_SHIFT_MIN, PILOTWIRE_SHIFT_MAX, str_drift_step.c_str(), str_temperature.c_str());
  }
  else WSContentSend_P (PSTR ("<p><i>%s</i></p>\n"), D_PILOTWIRE_NOSENSOR);

  WSContentSend_P (PSTR("</fieldset></p>\n"));

  // control settings  
  // --------------------

  // if temperature is available
  if (isnan (actual_temperature) == false) 
  {
    WSContentSend_P (PSTR("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n"), D_PILOTWIRE_SETTING);

    // temperature minimum label and input
    str_temperature = String (PilotwireGetMinTemperature ());
    WSContentSend_P (PSTR ("<p>%s (°%s)<span %s>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%s' value='%s'></p>\n"), D_PILOTWIRE_MIN, str_unit.c_str (), PILOTWIRE_TOPIC_STYLE, D_CMND_PILOTWIRE_MIN, D_CMND_PILOTWIRE_MIN, PILOTWIRE_TEMP_MIN, PILOTWIRE_TEMP_MAX, str_temp_step.c_str(), str_temperature.c_str());

    // temperature maximum label and input
    str_temperature = String (PilotwireGetMaxTemperature ());
    WSContentSend_P (PSTR ("<p>%s (°%s)<span %s>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%s' value='%s'></p>\n"), D_PILOTWIRE_MAX, str_unit.c_str (), PILOTWIRE_TOPIC_STYLE, D_CMND_PILOTWIRE_MAX, D_CMND_PILOTWIRE_MAX, PILOTWIRE_TEMP_MIN, PILOTWIRE_TEMP_MAX, str_temp_step.c_str(), str_temperature.c_str());

    WSContentSend_P (PSTR("</fieldset></p>\n"));
  }

  // ds18b20 sensor  
  // --------------

  WSContentSend_P (PSTR("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n"), D_PILOTWIRE_DS18B20);

  // pullup option for ds18b20 sensor
  if (PilotwireGetDS18B20Pullup ()) str_pullup = "checked";
  WSContentSend_P (PSTR ("<p><input type='checkbox' name='%s' %s>%s</p>\n"), D_CMND_PILOTWIRE_PULLUP, str_pullup.c_str(), D_PILOTWIRE_PULLUP);
  WSContentSend_P (PSTR("</fieldset></p>\n"));

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
  int     index, array_index;
  uint8_t state_current, state_previous;
  float   temp_min, temp_max, temp_scope, temp_current;
  float   graph_start, graph_stop, graph_width, graph_x, graph_y, graph_start_x;
  String str_temperature;

  // get min and max temperature
  temp_min   = PilotwireGetMinTemperature ();
  temp_max   = PilotwireGetMaxTemperature ();
  temp_scope = temp_max - temp_min;

  // start of SVG graph
  graph_start  = float (PILOTWIRE_GRAPH_PERCENT_START) / 100;
  graph_stop   = float (PILOTWIRE_GRAPH_PERCENT_STOP) / 100;
  graph_width  = graph_stop - graph_start;

  // start of SVG graph
  WSContentSend_P (PSTR ("<svg viewBox='0 0 %d %d'>\n"), PILOTWIRE_GRAPH_WIDTH, PILOTWIRE_GRAPH_HEIGHT);

  // graph curve zone
  WSContentSend_P (PSTR ("<rect x='%d%%' y='0%%' width='%d%%' height='100%%' rx='5' />\n"), PILOTWIRE_GRAPH_PERCENT_START, PILOTWIRE_GRAPH_PERCENT_STOP - PILOTWIRE_GRAPH_PERCENT_START);

  // loop for the relay state as background red color
  graph_start_x  = graph_start;
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
      graph_start_x = (graph_start + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE)) * PILOTWIRE_GRAPH_WIDTH;
    }
    
    // esle, if relay just switched off, display state
    else if ((state_previous == PILOTWIRE_COMFORT) && (state_current != PILOTWIRE_COMFORT))
    {
      // calculate end point x (from 10% to 100%)
      graph_x = (graph_start + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE)) * PILOTWIRE_GRAPH_WIDTH;

      // draw relay off line
      WSContentSend_P (PSTR ("<rect class='relay' x='%d' y='%d' width='%d' height='%d' />\n"), (int)graph_start_x, 0, (int)(graph_x - graph_start_x), PILOTWIRE_GRAPH_HEIGHT);
    }

    // update previous state
    state_previous = state_current;
  }

  // graph temperature text
  str_temperature = String (temp_max, 1);
  WSContentSend_P (PSTR ("<text class='dash' x='1%%' y='4%%'>%s°C</text>\n"), str_temperature.c_str ());
  str_temperature = String (temp_min + (temp_max - temp_min) * 0.75, 1);
  WSContentSend_P (PSTR ("<text class='dash' x='1%%' y='27%%'>%s°C</text>\n"), str_temperature.c_str ());
  str_temperature = String (temp_min + (temp_max - temp_min) * 0.5, 1);
  WSContentSend_P (PSTR ("<text class='dash' x='1%%' y='52%%'>%s°C</text>\n"), str_temperature.c_str ());
  str_temperature = String (temp_min + (temp_max - temp_min) * 0.25, 1);
  WSContentSend_P (PSTR ("<text class='dash' x='1%%' y='77%%'>%s°C</text>\n"), str_temperature.c_str ());
  str_temperature = String (temp_min, 1);
  WSContentSend_P (PSTR ("<text class='dash' x='1%%' y='100%%'>%s°C</text>\n"), str_temperature.c_str ());

  // graph separation lines
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='25%%' x2='%d%%' y2='25%%' />\n"), PILOTWIRE_GRAPH_PERCENT_START, PILOTWIRE_GRAPH_PERCENT_STOP);
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='50%%' x2='%d%%' y2='50%%' />\n"), PILOTWIRE_GRAPH_PERCENT_START, PILOTWIRE_GRAPH_PERCENT_STOP);
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='75%%' x2='%d%%' y2='75%%' />\n"), PILOTWIRE_GRAPH_PERCENT_START, PILOTWIRE_GRAPH_PERCENT_STOP);


  // loop for the target temperature graph
  WSContentSend_P (PSTR ("<polyline class='target' points='"));
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    // get target temperature value and set to minimum if not defined
    array_index = (index + pilotwire_graph_index) % PILOTWIRE_GRAPH_SAMPLE;
    temp_current = arr_target[array_index];
    if (isnan (temp_current) == true) temp_current = temp_min;

    // calculate end point x (from 10% to 100%)
    graph_x = (graph_start + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE)) * PILOTWIRE_GRAPH_WIDTH;

    // calculate y (with graph min and max)
    temp_current = min (max (temp_current, temp_min), temp_max);
    graph_y = (1 - ((temp_current - temp_min) / temp_scope)) * PILOTWIRE_GRAPH_HEIGHT;

    // add the point to the line
    WSContentSend_P (PSTR("%d,%d "), (int) graph_x, (int) graph_y);
  }
  WSContentSend_P (PSTR("'/>\n"));

  // loop for the temperature graph
  WSContentSend_P (PSTR ("<polyline class='temp' points='"));
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    // if temperature value is defined
    array_index = (index + pilotwire_graph_index) % PILOTWIRE_GRAPH_SAMPLE;
    temp_current = arr_temperature[array_index];
    if (isnan (temp_current) == false)
    {
      // calculate end point x (from 10% to 100%)
      graph_x = (graph_start + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE)) * PILOTWIRE_GRAPH_WIDTH;

      // calculate y (with graph min and max)
      temp_current = min (max (temp_current, temp_min), temp_max);
      graph_y = (1 - ((temp_current - temp_min) / temp_scope)) * PILOTWIRE_GRAPH_HEIGHT;

      // add the point to the line
      WSContentSend_P (PSTR("%d,%d "), (int) graph_x, (int) graph_y);
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

  // if thermostat mode, display target
  if (mode == PILOTWIRE_THERMOSTAT)
  {
    // get target temperature
    temp_current = PilotwireGetTargetTemperature ();

    // calculate and display target temperature line
    index = (100 * (1 - ((temp_current - temp_min) / temp_scope)));
    WSContentSend_P (PSTR ("<line id='line' class='target' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n"), PILOTWIRE_GRAPH_PERCENT_START, index, PILOTWIRE_GRAPH_PERCENT_STOP, index);

    // display target temperature text
    index = max (min (index + 2, 100), 4);
    str_temperature = String (temp_current, 1) + "°C";
    WSContentSend_P (PSTR ("<text id='text' class='target' x='%d%%' y='%d%%'>%s</text>\n"), PILOTWIRE_GRAPH_PERCENT_STOP + 1, index, str_temperature.c_str ());
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
}

// Pilot Wire heater public configuration web page
void PilotwireWebPageControl ()
{
  uint8_t actual_mode, actual_state;
  char    argument[PILOTWIRE_BUFFER_SIZE];
  float   temp_current, temp_target, temp_min, temp_max, temp_scope;
  String  str_temp_current, str_temp_target, str_temp_min, str_temp_max, str_temp_step, str_temp_scope, str_text;

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

  // get tempreratures
  temp_current = PilotwireGetTemperature ();
  temp_target  = PilotwireGetTargetTemperature ();
  temp_min     = PilotwireGetMinTemperature ();
  temp_max     = PilotwireGetMaxTemperature ();
  temp_scope   = temp_max - temp_min;
  
  // convert temperatures to string with 1 digit
  str_temp_current = String (temp_current, 1);
  str_temp_target  = String (temp_target, 1);
  str_temp_min     = String (temp_min, 1);
  str_temp_max     = String (temp_max, 1);
  str_temp_scope   = String (temp_scope, 1);
  str_temp_step    = String (PILOTWIRE_TEMP_STEP, 1);

  // beginning of form without authentification with 60 seconds auto refresh
  WSContentStart_P (D_PILOTWIRE_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%d;URL=/%s' />\n"), PILOTWIRE_GRAPH_REFRESH, D_PAGE_PILOTWIRE_CONTROL);
  
  // page style
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR ("body {color:white;background-color:#303030;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("fieldset {border-radius:10px;}\n"));
  
  WSContentSend_P (PSTR (".title {font-size:5vh;}\n"));
  WSContentSend_P (PSTR (".dash {font-size:24px;}\n"));
  WSContentSend_P (PSTR (".target {font-size:24px;}\n"));
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
  WSContentSend_P (PSTR ("rect.relay {fill:red;fill-opacity:20%%;}\n"));
  WSContentSend_P (PSTR ("polyline.temp {fill:none;stroke:yellow;stroke-width:4;}\n"));
  WSContentSend_P (PSTR ("polyline.target {fill:none;stroke:orange;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:2;stroke-dasharray:4;}\n"));
  WSContentSend_P (PSTR ("text.dash {stroke:white;fill:white;}\n"));
  WSContentSend_P (PSTR ("line.target {stroke:orange;stroke-width:4;stroke-dasharray:2;}\n"));
  WSContentSend_P (PSTR ("text.target {stroke:orange;fill:orange;}\n"));

  WSContentSend_P (PSTR ("</style>\n</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_PILOTWIRE_CONTROL);

  // french flag
  WSContentSend_P (PSTR ("<div class='flag fr'><a href='/control?lang=fr'>"));
  PilotwireWebDisplayFlag (PILOTWIRE_LANGAGE_FRENCH);
  WSContentSend_P (PSTR ("</a></div>\n"));

  // english flag
  WSContentSend_P (PSTR ("<div class='flag en'><a href='/control?lang=en'>"));
  PilotwireWebDisplayFlag (PILOTWIRE_LANGAGE_ENGLISH);
  WSContentSend_P (PSTR ("</a></div>\n"));

  // room name and current temperature
  WSContentSend_P (PSTR ("<div class='title bold'>%s</div>\n"), SettingsText(SET_FRIENDLYNAME1));
  WSContentSend_P (PSTR ("<div class='title bold yellow'>%s°C</div>\n"), str_temp_current.c_str());

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
    WSContentSend_P (PSTR ("<input id='slider' name='target' type='range' class='slider' min='%s' max='%s' value='%s' step='%s' />\n"), str_temp_min.c_str(), str_temp_max.c_str(), str_temp_target.c_str(), str_temp_step.c_str());

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
      WebServer->on ("/" D_PAGE_PILOTWIRE_CONFIG, PilotwireWebPageConfigure);
      WebServer->on ("/" D_PAGE_PILOTWIRE_CONTROL, PilotwireWebPageControl);
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
