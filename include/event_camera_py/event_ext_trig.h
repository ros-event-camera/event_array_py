// -*-c++-*--------------------------------------------------------------------
// Copyright 2022 Bernd Pfrommer <bernd.pfrommer@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef EVENT_CAMERA_PY__EVENT_EXT_TRIG_H_
#define EVENT_CAMERA_PY__EVENT_EXT_TRIG_H_

#include <cstdint>

struct EventExtTrig
{
  explicit EventExtTrig(int16_t e = 0, int64_t ta = 0, int16_t ida = 0) : p(e), t(ta), id(ida) {}
  int16_t p;   // edge (rising or faling)
  int64_t t;   // time stamp
  int16_t id;  // source of trigger signal
};
#endif  // EVENT_CAMERA_PY__EVENT_EXT_TRIG_H_
