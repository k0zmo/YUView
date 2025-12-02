/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   In addition, as a special exception, the copyright holders give
 *   permission to link the code of portions of this program with the
 *   OpenSSL library under certain conditions as described in each
 *   individual source file, and distribute linked combinations including
 *   the two.
 *
 *   You must obey the GNU General Public License in all respects for all
 *   of the code used other than OpenSSL. If you modify file(s) with this
 *   exception, you may extend this exception to your version of the
 *   file(s), but you are not obligated to do so. If you do not wish to do
 *   so, delete this exception statement from your version. If you delete
 *   this exception statement from all source files in the program, then
 *   also delete it here.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "time_code.h"

#include <parser/common/Functions.h>

namespace parser::hevc
{

using namespace reader;

SEIParsingResult time_code::parse(reader::SubByteReaderLogging &          reader,
                                  bool                                    reparse,
                                  VPSMap &                                vpsMap,
                                  SPSMap &                                spsMap,
                                  std::shared_ptr<seq_parameter_set_rbsp> associatedSPS)
{
  (void)reparse;
  (void)vpsMap;
  (void)spsMap;
  (void)associatedSPS;

  SubByteReaderLoggingSubLevel subLevel(reader, "time_code()");

  this->num_clock_ts = reader.readBits("num_clock_ts", 2);
  this->clockTimestamps.resize(this->num_clock_ts);

  for (unsigned i = 0; i < this->num_clock_ts; i++)
  {
    auto &ts = this->clockTimestamps[i];

    ts.clock_timestamp_flag = reader.readFlag(formatArray("clock_timestamp_flag", i));

    if (ts.clock_timestamp_flag)
    {
      ts.units_field_based_flag = reader.readFlag(formatArray("units_field_based_flag", i));
      ts.counting_type          = reader.readBits(formatArray("counting_type", i), 5);
      ts.full_timestamp_flag    = reader.readFlag(formatArray("full_timestamp_flag", i));
      ts.discontinuity_flag     = reader.readFlag(formatArray("discontinuity_flag", i));
      ts.cnt_dropped_flag       = reader.readFlag(formatArray("cnt_dropped_flag", i));
      ts.n_frames               = reader.readBits(formatArray("n_frames", i), 9);

      if (ts.full_timestamp_flag)
      {
        ts.seconds_value = reader.readBits(formatArray("seconds_value", i), 6);
        ts.minutes_value = reader.readBits(formatArray("minutes_value", i), 6);
        ts.hours_value   = reader.readBits(formatArray("hours_value", i), 5);
      }
      else
      {
        ts.seconds_flag = reader.readFlag(formatArray("seconds_flag", i));
        if (ts.seconds_flag)
        {
          ts.seconds_value = reader.readBits(formatArray("seconds_value", i), 6);
          ts.minutes_flag  = reader.readFlag(formatArray("minutes_flag", i));
          if (ts.minutes_flag)
          {
            ts.minutes_value = reader.readBits(formatArray("minutes_value", i), 6);
            ts.hours_flag    = reader.readFlag(formatArray("hours_flag", i));
            if (ts.hours_flag)
            {
              ts.hours_value = reader.readBits(formatArray("hours_value", i), 5);
            }
          }
        }
      }

      ts.time_offset_length = reader.readBits(formatArray("time_offset_length", i), 5);
      if (ts.time_offset_length > 0)
      {
        ts.time_offset_value =
            reader.readSBits(formatArray("time_offset_value", i), ts.time_offset_length);
      }
    }
  }

  return SEIParsingResult::OK;
}

} // namespace parser::hevc
