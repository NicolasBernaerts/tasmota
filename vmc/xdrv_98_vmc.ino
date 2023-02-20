/*
  xdrv_98_vmc.ino - Ventilation Motor Controled support for Tasmota.
    It has been tested on Sonoff TH, Sonoff Basic and ESP01
  
  Copyright (C) 2019-2023  Nicolas Bernaerts

  Version history :
    15/03/2019 - v1.0 - Creation
    03/02/2023 - v1.1 - Tasmota 12.3 compatibility
    
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

#define XDRV_98                 98

/*******************************************************\
 *                      Interface
\*******************************************************/

bool Xdrv98 (uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  {
   case FUNC_SET_DEVICE_POWER:
      result = VmcSetDevicePower ();
      break;
    case FUNC_MQTT_SUBSCRIBE:
      SensorMqttSubscribe ();
      break;
    case FUNC_MQTT_DATA:
      result = SensorMqttData ();
      break;
  }
  return result;
}

#endif // USE_VMC
