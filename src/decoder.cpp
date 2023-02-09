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

#include <event_array_codecs/decoder.h>
#include <event_array_py/decoder.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include <cstdint>
#include <iostream>

Decoder::Decoder() {}

void Decoder::decode_bytes(
  const std::string & encoding, uint16_t width, uint16_t height, uint64_t timeBase,
  pybind11::bytes events)
{
  auto decoder = decoderFactory_.getInstance(encoding, width, height);

  if (decoder) {
    decoder->setTimeBase(timeBase);
    decoder->setTimeMultiplier(1);  // report in usecs instead of nanoseconds
    delete cdEvents_;               // in case events have not been picked up
    cdEvents_ = new std::vector<EventCD>();
    delete extTrigEvents_;  // in case events have not been picked up
    extTrigEvents_ = new std::vector<EventExtTrig>();
    // TODO(Bernd): use hack here to avoid initializing the memory
    cdEvents_->reserve(maxSizeCD_);
    extTrigEvents_->reserve(maxSizeExtTrig_);
    const uint8_t * buf = reinterpret_cast<const uint8_t *>(PyBytes_AsString(events.ptr()));
    decoder->decode(buf, PyBytes_Size(events.ptr()), this);
  }
}

void Decoder::decode_array(
  const std::string & encoding, uint16_t width, uint16_t height, uint64_t timeBase,
  pybind11::array events)
{
  if (events.ndim() != 1 || !pybind11::isinstance<pybind11::array_t<uint8_t>>(events)) {
    throw std::runtime_error("Input events must be 1-D numpy array of type uint8");
  }

  auto decoder = decoderFactory_.getInstance(encoding, width, height);
  if (decoder) {
    decoder->setTimeBase(timeBase);
    decoder->setTimeMultiplier(1);  // report in usecs instead of nanoseconds
    delete cdEvents_;               // in case events have not been picked up
    cdEvents_ = new std::vector<EventCD>();
    delete extTrigEvents_;  // in case events have not been picked up
    extTrigEvents_ = new std::vector<EventExtTrig>();
    // TODO(Bernd): use hack here to avoid initializing the memory
    cdEvents_->reserve(maxSizeCD_);
    extTrigEvents_->reserve(maxSizeExtTrig_);
    const uint8_t * buf = reinterpret_cast<const uint8_t *>(events.data());
    decoder->decode(buf, events.size(), this);
  }
}

pybind11::array_t<EventCD> Decoder::get_cd_events()
{
  if (cdEvents_) {
    auto p = cdEvents_;
    auto cap =
      pybind11::capsule(p, [](void * v) { delete reinterpret_cast<std::vector<EventCD> *>(v); });
    cdEvents_ = 0;  // clear out
    return (pybind11::array_t<EventCD>(p->size(), p->data(), cap));
  }
  return (pybind11::array_t<EventCD>());
}

pybind11::array_t<EventExtTrig> Decoder::get_ext_trig_events()
{
  if (extTrigEvents_) {
    auto p = extTrigEvents_;
    auto cap = pybind11::capsule(
      p, [](void * v) { delete reinterpret_cast<std::vector<EventExtTrig> *>(v); });
    extTrigEvents_ = 0;  // clear out
    return (pybind11::array_t<EventExtTrig>(p->size(), p->data(), cap));
  }
  return (pybind11::array_t<EventExtTrig>());
}

void Decoder::eventCD(uint64_t sensor_time, uint16_t ex, uint16_t ey, uint8_t polarity)
{
  cdEvents_->push_back(EventCD(ex, ey, polarity, sensor_time));
  maxSizeCD_ = std::max(cdEvents_->size(), maxSizeCD_);
  numCDEvents_[std::min(polarity, uint8_t(1))]++;
}

void Decoder::eventExtTrigger(uint64_t sensor_time, uint8_t edge, uint8_t id)
{
  extTrigEvents_->push_back(EventExtTrig(
    static_cast<int16_t>(edge), static_cast<int64_t>(sensor_time), static_cast<int16_t>(id)));
  maxSizeExtTrig_ = std::max(extTrigEvents_->size(), maxSizeExtTrig_);
  numExtTrigEvents_[std::min(edge, uint8_t(1))]++;
}

PYBIND11_MODULE(_event_array_py, m)
{
  pybind11::options options;
  options.disable_function_signatures();
  m.doc() = R"pbdoc(
        Plugin for processing event_array_msgs in python
    )pbdoc";

  PYBIND11_NUMPY_DTYPE(EventCD, x, y, p, t);
  PYBIND11_NUMPY_DTYPE(EventExtTrig, p, t, id);

  pybind11::class_<Decoder>(
    m, "Decoder",
    R"pbdoc(
        Class to decode event_array_msgs in python. The decoder
        keeps state inbetween calls to decode(). After calling decode()
        the events must be read via get_cd_events() before calling
        decode() again.
        Sample code:

        decoder = Decoder()
        for msg in msgs:
            decoder.decode(msg.encoding, msg.width, msg.height, msg.time_base, msg.events)
            cd_events = decoder.get_cd_events()
            trig_events = decoder.get_ext_trig_events()
)pbdoc")
    .def(pybind11::init<>())
    .def("decode_bytes", &Decoder::decode_bytes, R"pbdoc(
        decode_bytes(encoding, width, height, time_base, buffer) -> None

        Processes buffer of encoded events and updates state of the decoder.

        :param encoding: Encoding string (e.g. "evt3") as provided by the message.
        :type encoding: str
        :param width: sensor width in pixels
        :type width: int
        :param height: sensor height in pixels
        :type height: int
        :param time_base: Time base as provided by the message. Some codecs use it to
                          compute time stamps.
        :type time_base: int
        :param buffer: Buffer with encoded events to be processed, as provided by the message.
        :type buffer: bytes
)pbdoc")
    .def("decode_array", &Decoder::decode_array, R"pbdoc(
        decode_array(encoding, width, height, time_base, buffer) -> None

        Processes buffer of encoded events and updates state of the decoder.

        :param encoding: Encoding string (e.g. "evt3") as provided by the message.
        :type encoding: str
        :param width: sensor width in pixels
        :type width: int
        :param height: sensor height in pixels
        :type height: int
        :param time_base: Time base as provided by the message. Some codecs use it to
                          compute time stamps.
        :type time_base: int
        :param buffer: Buffer with encoded events to be processed, as provided by the message.
        :type buffer: numpy array of dtype np.uint8_t
)pbdoc")
    .def("get_cd_events", &Decoder::get_cd_events, R"pbdoc(
        get_cd_events() -> numpy.ndarray['EventCD']

        Fetches decoded change detected (CD) events. Will clear out decoded events, to be
        called only once. If not called, events will be lost next time decode() is called.

        :return: array of detected events in the same format as the metavision SDK uses.
        :rtype: numpy.ndarray[EventCD], structured numpy array with fields 'x', 'y', 't', 'p'
)pbdoc")
    .def("get_ext_trig_events", &Decoder::get_ext_trig_events, R"pbdoc(
        get_ext_trig_events() -> numpy.ndarray['EventExtTrig']

        Fetches decoded external trigger events. Will clear out decoded events, to be
        called only once. If not called, events will be lost next time decode() is called.
        :return: array of trigger events in the same format as the metavision SDK uses.
        :rtype: numpy.ndarray[EventExtTrig], structured numpy array with fields 'p', 't', 'id'
)pbdoc")
    .def("get_num_cd_on", &Decoder::get_num_cd_on, R"pbdoc(
        get_num_cd_on() -> int
        :return: cumulative number of ON events.
        :rtype: int
)pbdoc")
    .def("get_num_cd_off", &Decoder::get_num_cd_off, R"pbdoc(
        get_num_cd_off() -> int
        :return: cumulative number of OFF events.
        :rtype: int
)pbdoc")
    .def("get_num_trigger_rising", &Decoder::get_num_trigger_rising, R"pbdoc(
        get_num_trigger_rising() -> int
        :return: cumulative number of rising edge external trigger events.
        :rtype: int
)pbdoc")
    .def("get_num_trigger_falling", &Decoder::get_num_trigger_falling, R"pbdoc(
        get_num_trigger_falling() -> int
        :return: cumulative number of falling edge external trigger events.
        :rtype: int
)pbdoc");
}
