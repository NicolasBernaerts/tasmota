/*
  xdrv_01_9_webserver_extension.ino - Web server extension functions
  
  Copyright (C) 2026  Nicolas Bernaerts
    10/08/2026 v1.0 - Creation, CSS and Javascript cache management function

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

/************************************\
 *      Web Helper Functions
\************************************/


#ifdef USE_WEBSERVER

#define WEB_DEFAULT_CACHE_VALIDITY        31536000        // cache valid for 1 year

// --------------------------------
//  Page served with cache enabled
// --------------------------------

void WSContentBeginPNG        (const int code, const uint32_t duration) { WSContentBeginWithCache (code, duration, PSTR ("image/png"));       }
void WSContentBeginJPG        (const int code, const uint32_t duration) { WSContentBeginWithCache (code, duration, PSTR ("image/jpeg"));      }
void WSContentBeginCSS        (const int code, const uint32_t duration) { WSContentBeginWithCache (code, duration, PSTR ("text/css"));        }
void WSContentBeginJavascript (const int code, const uint32_t duration) { WSContentBeginWithCache (code, duration, PSTR ("text/javascript")); }
void WSContentBeginWithCache  (const int code, const uint32_t duration, const char *pstr_type)
{
  uint32_t validity;
  char str_text[40];

  Webserver->client ().flush ();

  snprintf_P (str_text, sizeof (str_text), PSTR("Tasmota/%s"), TasmotaGlobal.version);
  Webserver->sendHeader (F ("Server"), str_text);

  (duration == 0) ? validity = WEB_DEFAULT_CACHE_VALIDITY : validity = duration;
  sprintf_P (str_text, PSTR ("public,max-age=%u,immutable"), validity);
  Webserver->sendHeader (F ("Cache-Control"), str_text);

  Webserver->setContentLength (CONTENT_LENGTH_UNKNOWN);

  strcpy_P (str_text, pstr_type);
  Webserver->send (code, str_text);

  Web.chunk_buffer = "";
}

// --------------------------
//  Page arguments retrieval
// --------------------------

// retrieve web parameter as a flag, return true if different
bool WebGetArgExist (const char* pstr_key, char *pstr_value, const size_t size_value)
{
  char str_key[12];

  // check parameters
  if (pstr_key == nullptr) return false;

  // check key
  strcpy_P (str_key, pstr_key);
  if (!Webserver->hasArg (str_key)) return false;

  // retrieve key
  WebGetArg (str_key, pstr_value, size_value);
  return true;
}

// retrieve web parameter as string, return true if different
bool WebGetArgString (const char* pstr_key, char *pstr_current, const size_t size_current)
{
  bool changed = false;
  char str_arg[32];

  // check value
  if (WebGetArgExist (pstr_key, str_arg, sizeof (str_arg)))
    if ((pstr_current != nullptr) && (size_current > 0))
    {
      changed = (strcmp (str_arg, pstr_current) != 0);
      strlcpy (pstr_current, str_arg, size_current);
    }

  return changed;
}

// retrieve web parameter as a flag, return true if different
bool WebGetArgFlag (const char* pstr_key, bool &current)
{
  bool changed = false;
  bool status  = false;
  char str_arg[4];

  // check value
  if (WebGetArgExist (pstr_key, str_arg, sizeof (str_arg))) status = (strcmp_P (str_arg, PSTR ("on")) == 0);
  changed = (status != current);
  current = status;

  return changed;
}

// retrieve web parameter as uint8_t, return true if different
bool WebGetArgUint8 (const char* pstr_key, uint8_t &current, const uint8_t minimum, const uint8_t maximum)
{
  bool    changed = false;
  uint8_t value;
  char    str_arg[4];

  // check value
  if (WebGetArgExist (pstr_key, str_arg, sizeof (str_arg)))
  {
    value = (uint8_t)atoi (str_arg);
    if (value < minimum) value = current;
      else if (value > maximum) value = current;
    changed = (value != current);
    current = value;
  }

  return changed;
}

// retrieve web parameter as uint16_t, return true if different
bool WebGetArgUint16 (const char* pstr_key, uint16_t &current, const uint16_t minimum, const uint16_t maximum)
{
  bool    changed = false;
  uint8_t value;
  char    str_arg[8];

  // check value
  if (WebGetArgExist (pstr_key, str_arg, sizeof (str_arg)))
  {
    value = (uint16_t)atoi (str_arg);
    if (value < minimum) value = current;
      else if (value > maximum) value = current;
    changed = (value != current);
    current = value;
  }

  return changed;
}

// retrieve web parameter as integer, return true if different
bool WebGetArgInt (const char* pstr_key, int &current, const int minimum, const int maximum)
{
  bool changed = false;
  int  value;
  char str_arg[8];

  // check value
  if (WebGetArgExist (pstr_key, str_arg, sizeof (str_arg)))
  {
    value = atoi (str_arg);
    if ((minimum != INT_MIN) && (value < minimum)) value = current;
      else if ((maximum != INT_MAX) && (value > maximum)) value = current;
    changed = (value != current);
    current = value;
  }

  return changed;
}

// retrieve web parameter as integer, return true if different
bool WebGetArgLong (const char* pstr_key, long &current, const long minimum, const long maximum)
{
  bool changed = false;
  long value;
  char str_arg[24];

  // check value
  if (WebGetArgExist (pstr_key, str_arg, sizeof (str_arg)))
  {
    value = atol (str_arg);
    if ((minimum != LONG_MIN) && (value < minimum)) value = current;
      else if ((maximum != LONG_MAX) && (value > maximum)) value = current;
    changed = (value != current);
    current = value;
  }

  return changed;
}

// retrieve web parameter as integer, return true if different
bool WebGetArgFloat (const char* pstr_key, float &current, const float minimum, const float maximum)
{
  bool  changed = false;
  float value;
  char  str_arg[24];

  // check value
  if (WebGetArgExist (pstr_key, str_arg, sizeof (str_arg)))
  {
    value = atof (str_arg);
    if (!isnan (minimum) && (value < minimum)) value = current;
      else if (!isnan (maximum) && (value > maximum)) value = current;
    changed = (value != current);
    current = value;
  }

  return changed;
}

#endif    // USE_WEBSERVER

