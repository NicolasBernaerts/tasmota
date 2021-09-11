/*
  xdrv_50_filesystem_extension.ino - Extensions for UFS driver

  Copyright (C) 2019-2021  Nicolas Bernaerts

  Version history :
    01/09/2021 - v1.0   - Creation

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

#ifdef USE_UFILESYS

// configuration file
#define UFS_PARTITION_CONFIG            "/config.ini"   // configuration file

// Historic data files
#define UFS_PARTITION_MIN_KB            50              // minimum free space in USF partition (kb)
#define UFS_CSV_LINE_LENGTH             128             // maximum line length in CSV files
#define UFS_CSV_COLUMN_MAX              22              // maximum number of columns in CSV files

// CSV file management structure
static struct {
  File  file;                                           // file object
  bool  opened = false;                                 // flag if file is opened
  int   idx_line = INT_MAX;                             // index of current line
  int   nb_value = 0;                                   // number of values in last line
  char* pstr_value[UFS_CSV_COLUMN_MAX];                 // array of values in last line
  char  str_line[UFS_CSV_LINE_LENGTH];                  // last read line
} ufs_csv;

/*********************************************\
 *               Functions
\*********************************************/

// read next line and return number of columns in the line
int UfsCsvNextLine (bool analyse_content)
{
  bool  read_next;
  int   index;
  char* pstr_token;
  uint32_t pos_start, pos_delta;

  // init
  ufs_csv.nb_value = 0;
  ufs_csv.idx_line = INT_MAX;

  // read next line
  pos_start = ufs_csv.file.position ();
  ufs_csv.file.readBytes (ufs_csv.str_line, sizeof (ufs_csv.str_line) - 1);

  // align string and file position on end of line
  pstr_token = strchr (ufs_csv.str_line, '\n');
  if (pstr_token != nullptr)
  {
    pos_delta = pstr_token - ufs_csv.str_line;
    ufs_csv.file.seek (pos_start + pos_delta + 1);
    pstr_token = 0;
  } 

  // if content analisys is asked
  if (analyse_content)
  {
    // loop to populate array of values
    index = 0;
    pstr_token = strtok (ufs_csv.str_line, ";");
    while (pstr_token != nullptr)
    {
      if (index < UFS_CSV_COLUMN_MAX) ufs_csv.pstr_value[index++] = pstr_token;
      pstr_token = strtok (nullptr, ";");
    }
    ufs_csv.nb_value = index;

    // read next line and set first column as line index
    if (ufs_csv.nb_value > 0) ufs_csv.idx_line = atoi (ufs_csv.pstr_value[0]);
  }

  return ufs_csv.nb_value;
}

void UfsCsvClose ()
{
  if (ufs_csv.opened)
  {
    // close current file
    ufs_csv.file.close ();

    // init file caracteristics
    ufs_csv.opened = false;
    ufs_csv.nb_value = 0;
  }
}

bool UfsCsvOpenReadOnly (const char* pstr_filename, bool skip_header)
{
  bool result;

  // close file if already opened
  UfsCsvClose ();

  // if file exists
  result = ffsp->exists (pstr_filename);
  if (result)
  {
    // open file in read mode
    ufs_csv.opened = true;
    ufs_csv.file   = ffsp->open (pstr_filename, "r");

    // if asked, skip header
    if (skip_header) UfsCsvNextLine (false);

    // read first data lined
    UfsCsvNextLine (true);
  }

  return ufs_csv.opened;
}

bool UfsCsvSeekToStart (bool skip_header)
{
  // if file is opened
  if (ufs_csv.opened)
  {
    // set to start of file
    ufs_csv.file.seek (0);

    // if asked, skip header
    if (skip_header) UfsCsvNextLine (false);

    // read first data lined
    UfsCsvNextLine (true);
  }

  return ufs_csv.opened;
}

#endif    // USE_UFILESYS

