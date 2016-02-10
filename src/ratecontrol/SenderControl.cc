/*
 * Copyright (c) 2012-2015, Nic McDonald
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * - Neither the name of prim nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "ratecontrol/SenderControl.h"

#include <strop/strop.h>

#include <cassert>

#include <unordered_set>

SenderControl::SenderControl(des::Simulator* _sim, const std::string& _name,
                             const des::Model* _parent,
                             std::vector<Sender*>* _senders,
                             Json::Value _settings)
    : des::Model(_sim, _name, _parent), senders_(_senders) {
  // check settings form
  assert(_settings.isArray());

  // process all entries in settings
  //  expecting [des::Tick, std::string] pairs
  std::unordered_set<des::Tick> usedTicks;
  for (auto rateChange : _settings) {
    // pull out the values
    des::Tick tick(rateChange[0].asUInt64());
    des::Time time(tick);
    std::string control = rateChange[1].asString();

    // check that the tick hasn't already been used
    assert(usedTicks.count(tick) == 0);
    usedTicks.insert(tick);

    // add an event for this time
    simulator->addEvent(new des::ItemEvent<std::string>(
        this, static_cast<des::EventHandler>(&SenderControl::handle_rateChange),
        time, control));
  }
}

SenderControl::~SenderControl() {}

void SenderControl::handle_rateChange(des::Event* _event) {
  des::ItemEvent<std::string>* evt =
      reinterpret_cast<des::ItemEvent<std::string>*>(_event);
  const std::string& control = evt->item;
  std::unordered_set<u32> usedSenders;
  std::vector<std::string> groups = strop::split(control, ':');
  for (auto& group : groups) {
    std::vector<std::string> setting = strop::split(group, '=');
    assert(setting.size() == 2);
    const std::string& senderRange = setting.at(0);
    f64 rate = std::stod(setting.at(1));
    assert(rate >= 0.0 && rate <= 1.0);
    u32 start;
    u32 stop;
    if (senderRange == "*") {
      // full range
      start = 1;
      stop = senders_->size();
    } else {
      // a single specifier (i.e. "4") or a range (i.e. "4-89")
      std::vector<std::string> startStop = strop::split(senderRange, '-');
      assert(startStop.size() == 1 || startStop.size() == 2);
      start = std::stoul(startStop.at(0));
      stop = startStop.size() == 2 ? std::stoul(startStop.at(1)) : start;
      assert(stop >= start);
    }
    // set the injection rates
    for (u32 idx = start - 1; idx < stop; idx++) {
      assert(usedSenders.count(idx) == 0);
      usedSenders.insert(idx);
      senders_->at(idx)->setInjectionRate(rate);
    }
  }
  delete _event;
}
