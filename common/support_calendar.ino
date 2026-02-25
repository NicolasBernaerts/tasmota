/*
  support_calendar.ino - Calendar functions

  Copyright (C) 2025  Nicolas Bernaerts

  Version history :
    26/12/2025 v1.0 - Split from Teleinfo Hisot

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

enum HistoPeriod                  { CALENDAR_PERIOD_DAY, CALENDAR_PERIOD_WEEK, CALENDAR_PERIOD_MONTH, CALENDAR_PERIOD_YEAR, CALENDAR_PERIOD_MAX };
const char kHistoPeriod[] PROGMEM =      "Journée"    "|"     "Semaine"     "|"       "Mois"       "|"       "Année";                                 // units labels

// calculate number of days in given month
uint8_t CalendarGetDaysInMonth (const uint16_t year, const uint8_t month)
{
  uint8_t result;

  if ((month == 4) || (month == 6) || (month == 9 || (month == 11))) result = 30;     // months with 30 days  
  else if (month != 2)        result = 31;                                            // months with 31 days
  else if ((year % 400) == 0) result = 29;                                            // leap year
  else if ((year % 100) == 0) result = 28;                                            // not a leap year
  else if ((year % 4) == 0)   result = 29;                                            // leap year
  else result = 28;                                                                   // not a leap year
  
  return result;
}

// calculate number of days in given month
uint8_t CalendarGetDaysInMonth (const uint32_t timestamp)
{
  uint8_t result;
  TIME_T  time_dst;

  // extract timestamp data and calculate number of days in given month
  BreakTime (timestamp, time_dst);
  result = CalendarGetDaysInMonth (time_dst.year, time_dst.month);
  
  return result;
}

// get timestamp of first day of year
uint32_t CalendarGetFirstDayOfYear (const uint32_t timestamp)
{
  uint32_t time_calc;
  TIME_T   time_dst;

  // check parameter
  if (timestamp == 0) return 0;

  // calculate first day of year
  BreakTime (timestamp, time_dst);
  time_dst.month = 1;
  time_dst.day_of_month = 1;
  time_dst.day_of_week = 0;
  time_calc = MakeTime (time_dst);

  return time_calc;
}

// get timestamp of first day of month
uint32_t CalendarGetFirstDayOfMonth (const uint32_t timestamp)
{
  uint8_t  dayofmonth;
  uint32_t time_calc;
  TIME_T   time_dst;

  // check parameter
  if (timestamp == 0) return 0;

  // get current day of year
  BreakTime (timestamp, time_dst);
  dayofmonth = time_dst.day_of_month - 1;

  // shift to start of week
  time_calc = timestamp - (uint32_t)dayofmonth * 86400;

  return time_calc;
}

// get timestamp day of week
uint8_t CalendarGetDayOfWeek (const uint32_t timestamp)
{
  uint8_t dayofweek;
  TIME_T  time_dst;

  BreakTime (timestamp, time_dst);
  dayofweek = 1 + (time_dst.day_of_week + 5) % 7;

  return dayofweek;
}

// get timestamp of first day of week
uint32_t CalendarGetFirstDayOfWeek (const uint32_t timestamp)
{
  uint8_t  dayofweek;
  uint32_t time_calc;
  TIME_T   time_dst;

  // check parameter
  if (timestamp == 0) return 0;

  // get current day of year
  BreakTime (timestamp, time_dst);
  dayofweek = (time_dst.day_of_week + 5) % 7;

  // shift to start of week
  time_calc = timestamp - (uint32_t)dayofweek * 86400;

  return time_calc;
}

// get current week number
uint8_t CalendarGetWeekNumber (const uint32_t timestamp)
{
  uint16_t week, dayofyear, dayof1stweek, nbday1stweek;
  uint32_t time_calc;
  TIME_T   time_dst;

  // check parameter
  if (timestamp == 0) return 0;

  // get current day of year
  BreakTime (timestamp, time_dst);
  dayofyear = time_dst.day_of_year;

  // calculate 1st jan of same year
  time_dst.month        = 1;
  time_dst.day_of_month = 1;
  time_dst.day_of_year  = 0;
  time_dst.day_of_week  = 0;
  time_calc = MakeTime (time_dst);
  BreakTime (time_calc, time_dst);

  // calculate data for 1st week (monday = 0)
  dayof1stweek = (time_dst.day_of_week + 5) % 7;
  nbday1stweek = 7 - dayof1stweek;

  // if first week starts on monday, simple
  if (dayof1stweek == 0) week = dayofyear / 7 + 1;

  // else if day within first incomplete week
  else if (dayofyear <= nbday1stweek) week = 1;

  // else if day after first incomplete week
  else week = (dayofyear - nbday1stweek) / 7 + 2;

  return (uint8_t)week;
}

// check if week is complete (0 if complete, missing week number if incomplete)
uint8_t CalendarIsWeekComplete (const uint32_t timestamp)
{
  uint8_t  week, result;
  uint32_t time_calc;
  TIME_T   time_dst;

  // calculate week number
  week = CalendarGetWeekNumber (timestamp);

  // if week is complete
  if (week == 53) result = 1;
  else if (week == 1)
  {
    // calculate 1st of jan for current year
    BreakTime (timestamp, time_dst);
    time_dst.month        = 1;
    time_dst.day_of_month = 1;
    time_dst.day_of_week  = 0;
    time_dst.day_of_year  = 1;
    time_calc = MakeTime (time_dst);
    BreakTime (time_calc, time_dst);
  
    // if 1st is monday, week is complete else incomplete
    if (time_dst.day_of_week == 2) result = 0;
      else result = 53;
  }
  else result = 0;

  return result;
}

// calculate start timestamp according period and date
uint32_t CalendarGetPreviousTimestamp (const uint8_t period, const uint32_t timestamp, const uint32_t quantity)
{
  uint32_t result = 0;
  TIME_T   calc_dst;

  // check current timestamp
  if (period >= CALENDAR_PERIOD_MAX) return 0;
  if (timestamp == 0) return 0;
  if (quantity == 0) return 0;

  // calculate according to period
  switch (period)
  {
    case CALENDAR_PERIOD_DAY :
      result = timestamp - 86400 * quantity;
      break;

    case CALENDAR_PERIOD_WEEK :
      result = timestamp - 7 * 86400 * quantity;
      break;

    case CALENDAR_PERIOD_MONTH :
      BreakTime (timestamp, calc_dst);
      calc_dst.day_of_week = 0;
      if (calc_dst.day_of_month > 28) calc_dst.day_of_month = 28;
      if (calc_dst.month > quantity) calc_dst.month = calc_dst.month - quantity;
      else { calc_dst.month = calc_dst.month + 12 - quantity; calc_dst.year--; }
      result = MakeTime (calc_dst);
      break;

    case CALENDAR_PERIOD_YEAR :
      result = timestamp - 365 * 86400 * quantity;
      break;
  }

  return result;
}

// calculate start timestamp according period and date
uint32_t CalendarGetNextTimestamp (const uint8_t period, const uint32_t timestamp, const uint32_t quantity)
{
  uint32_t result = 0;
  TIME_T   calc_dst;

  // check current timestamp
  if (period >= CALENDAR_PERIOD_MAX) return 0;
  if (timestamp == 0) return 0;
  if (quantity == 0) return 0;

  // calculate according to period
  switch (period)
  {
    case CALENDAR_PERIOD_DAY:
      result = timestamp + 86400 * quantity;
      break;

    case CALENDAR_PERIOD_WEEK:
      result = timestamp + 7 * 86400 * quantity;
      break;

    case CALENDAR_PERIOD_MONTH:
      BreakTime (timestamp, calc_dst);
      calc_dst.day_of_week = 0;
      if (calc_dst.day_of_month > 28) calc_dst.day_of_month = 28;
      calc_dst.month = calc_dst.month + quantity;
      if (calc_dst.month > 12) { calc_dst.month = calc_dst.month - 12; calc_dst.year++; }
      result = MakeTime (calc_dst);
      break;

    case CALENDAR_PERIOD_YEAR:
      result = timestamp + 365 * 86400 * quantity;
      break;
  }

  return result;
}

// get label according to periods and timestamp limits
void CalendarGetPeriodLabel (const uint8_t period, const uint32_t timestamp, char* pstr_label, const size_t size_label)
{
  uint32_t calc_time, delta;
  TIME_T   start_dst, stop_dst;
  char     str_start[4];
  char     str_stop[4];

  // check parameters
  if (period >= CALENDAR_PERIOD_MAX) return;
  if (pstr_label == nullptr) return;
  if (size_label < 24) return;
  
  // init
  pstr_label[0] = 0;

  // calculate according to period
  switch (period)
  {
    case CALENDAR_PERIOD_DAY:
      BreakTime (timestamp, start_dst);
      strlcpy (str_start, kMonthNames + start_dst.month * 3 - 3, 4);
      sprintf_P (pstr_label, PSTR ("%u %s %u"), start_dst.day_of_month, str_start, 1970 + start_dst.year);
      break;

    case CALENDAR_PERIOD_WEEK:
      BreakTime (timestamp, start_dst);
      delta = (uint32_t)(start_dst.day_of_week + 5) % 7;
      calc_time = timestamp - delta * 86400;
      BreakTime (calc_time, start_dst);
      calc_time = calc_time + 6 * 86400;
      BreakTime (calc_time, stop_dst);
      strlcpy (str_start, kMonthNames + start_dst.month * 3 - 3, 4);
      strlcpy (str_stop,  kMonthNames + stop_dst.month  * 3 - 3, 4);
      if (start_dst.month == stop_dst.month) sprintf_P (pstr_label, PSTR ("%u - %u %s %u"), start_dst.day_of_month, stop_dst.day_of_month, str_stop, 1970 + stop_dst.year);
        else sprintf_P (pstr_label, PSTR ("%u %s - %u %s %u"), start_dst.day_of_month, str_start, stop_dst.day_of_month, str_stop, 1970 + stop_dst.year);
      break;

    case CALENDAR_PERIOD_MONTH:
      BreakTime (timestamp, start_dst);
      strlcpy (str_start, kMonthNames + start_dst.month * 3 - 3, 4);
      sprintf_P (pstr_label, PSTR ("%s %u"), str_start, 1970 + start_dst.year);
      break;

    case CALENDAR_PERIOD_YEAR:
      BreakTime (timestamp, start_dst);
      sprintf_P (pstr_label, PSTR ("%u"), 1970 + start_dst.year);
      break;
  }
}

void CalendarFileGetHeader (const uint32_t timestamp, char *pstr_header, const size_t size_header)
{
  uint8_t week, doweek;
  TIME_T  time_dst;

  // check parameters
  if (pstr_header == nullptr) return;
  if (size_header < 14) return;

  // calculate timestamp
  BreakTime (timestamp, time_dst);
  doweek = CalendarGetDayOfWeek (timestamp);
  week   = CalendarGetWeekNumber (timestamp);

  // line : beginning
  sprintf_P (pstr_header, PSTR ("%03u%02u%u%02u%02u%02u;"), time_dst.day_of_year + 1, week, doweek, time_dst.month, time_dst.day_of_month, time_dst.hour);
}
