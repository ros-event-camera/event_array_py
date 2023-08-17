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

#ifndef EVENT_CAMERA_PY__DECODER_H_
#define EVENT_CAMERA_PY__DECODER_H_

#include <event_camera_codecs/decoder.h>
#include <event_camera_codecs/decoder_factory.h>
#include <event_camera_codecs/event_packet.h>
#include <event_camera_py/event_cd.h>
#include <event_camera_py/event_ext_trig.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include <string>
#include <tuple>
#include <vector>

template <class A>
class Decoder
{
public:
  Decoder() = default;
  void decode(pybind11::object msg)
  {
    pybind11::array_t<uint8_t> events = get_attr<pybind11::array_t<uint8_t>>(msg, "events");
    do_full_decode(
      get_attr<std::string>(msg, "encoding"), get_attr<uint32_t>(msg, "width"),
      get_attr<uint32_t>(msg, "height"), get_attr<uint64_t>(msg, "time_base"), events.data(),
      events.size());
  }

  std::tuple<bool, uint64_t> decode_until(pybind11::object msg, uint64_t untilTime)
  {
    auto decoder = initialize_decoder(
      get_attr<std::string>(msg, "encoding"), get_attr<uint32_t>(msg, "width"),
      get_attr<uint32_t>(msg, "height"));
    const uint64_t timeBase = get_attr<uint64_t>(msg, "time_base");
    accumulator_.reset_stored_events();
    pybind11::array_t<uint8_t> events = get_attr<pybind11::array_t<uint8_t>>(msg, "events");
    uint64_t nextTime{0};
    const bool reachedTimeLimit = decoder->decodeUntil(
      events.data(), events.size(), &accumulator_, untilTime, timeBase, &nextTime);
    return (std::tuple<bool, uint64_t>({reachedTimeLimit, nextTime}));
  }

  void decode_bytes(
    const std::string & encoding, uint16_t width, uint16_t height, uint64_t timeBase,
    pybind11::bytes events)
  {
    const uint8_t * buf = reinterpret_cast<const uint8_t *>(PyBytes_AsString(events.ptr()));
    do_full_decode(encoding, width, height, timeBase, buf, PyBytes_Size(events.ptr()));
  }

  void decode_array(
    const std::string & encoding, uint16_t width, uint16_t height, uint64_t timeBase,
    pybind11::array_t<uint8_t> events)
  {
    if (events.ndim() != 1 || !pybind11::isinstance<pybind11::array_t<uint8_t>>(events)) {
      throw std::runtime_error("Input events must be 1-D numpy array of type uint8");
    }
    const uint8_t * buf = reinterpret_cast<const uint8_t *>(events.data());
    do_full_decode(encoding, width, height, timeBase, buf, events.size());
  }
  pybind11::array_t<EventCD> get_cd_events() { return (accumulator_.get_cd_events()); }
  pybind11::array_t<EventExtTrig> get_ext_trig_events()
  {
    return (accumulator_.get_ext_trig_events());
  };
  pybind11::list get_cd_event_packets() { return (accumulator_.get_cd_event_packets()); }
  pybind11::list get_ext_trig_event_packets()
  {
    return (accumulator_.get_ext_trig_event_packets());
  }

  size_t get_num_cd_off() const { return (accumulator_.get_num_cd_off()); }
  size_t get_num_cd_on() const { return (accumulator_.get_num_cd_on()); }
  size_t get_num_trigger_rising() const { return (accumulator_.get_num_trigger_rising()); }
  size_t get_num_trigger_falling() const { return (accumulator_.get_num_trigger_falling()); }

private:
  using DecoderType = event_camera_codecs::Decoder<event_camera_codecs::EventPacket, A>;
  template <class T>
  static T get_attr(pybind11::object msg, const std::string & name)
  {
    if (!pybind11::hasattr(msg, name.c_str())) {
      throw std::runtime_error("event packet has no " + name + " field");
    }
    return (pybind11::getattr(msg, name.c_str()).cast<T>());
  }

  void do_full_decode(
    const std::string & encoding, uint16_t width, uint16_t height, uint64_t timeBase,
    const uint8_t * buf, size_t bufSize)
  {
    auto decoder = initialize_decoder(encoding, width, height);
    decoder->setTimeBase(timeBase);

    accumulator_.reset_stored_events();
    decoder->decode(buf, bufSize, &accumulator_);
  }

  DecoderType * initialize_decoder(const std::string & encoding, uint32_t width, uint32_t height)
  {
    accumulator_.initialize(width, height);
    auto decoder = decoderFactory_.getInstance(encoding, width, height);
    if (!decoder) {
      throw(std::runtime_error("no decoder for encoding " + encoding));
    }
    decoder->setTimeMultiplier(1);  // report in usecs instead of nanoseconds
    return (decoder);
  }

  // ------------ variables
  event_camera_codecs::DecoderFactory<event_camera_codecs::EventPacket, A> decoderFactory_;
  A accumulator_;
};

#endif  // EVENT_CAMERA_PY__DECODER_H_
