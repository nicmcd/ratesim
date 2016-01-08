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

#include <cassert>

SenderControl::SenderControl(des::Simulator* _sim, const std::string& _name,
                             const des::Model* _parent,
                             std::vector<Sender*>* _senders,
                             Json::Value _settings)
    : des::Model(_sim, _name, _parent), senders_(_senders) {
  // check settings form
  assert(_settings.isArray());

  // save values during processing to verify after complete
  des::Tick firstTick(des::TICK_INV);
  f64 lastRate = F64_POS_INF;

  // process all entries in settings
  //  expecting [des::Tick, f64] pairs
  for (auto rateChange : _settings) {
    // pull out the values
    des::Tick tick(rateChange[0].asUInt64());
    des::Time time(tick);
    f64 aggRate = rateChange[1].asDouble();
    f64 indRate = aggRate / senders_->size();

    // check the values
    assert(indRate >= 0.0 && indRate <= 1.0);

    // add an event for this time
    simulator->addEvent(new des::ItemEvent<f64>(
        this, static_cast<des::EventHandler>(&SenderControl::handle_rateChange),
        time, indRate));

    // set the first time and last value
    if (firstTick == des::TICK_INV) {
      firstTick = tick;
    }
    lastRate = indRate;
  }

  // verify setup is good
  assert(firstTick == 0);
  assert(lastRate == 0.0);
}

SenderControl::~SenderControl() {}

void SenderControl::handle_rateChange(des::Event* _event) {
  des::ItemEvent<f64>* evt = reinterpret_cast<des::ItemEvent<f64>*>(_event);
  f64 rate = evt->item;
  dlogf("injectionrate=%f", rate);
  for (u64 idx = 0; idx < senders_->size(); idx++) {
    senders_->at(idx)->setInjectionRate(rate);
  }
  delete _event;
}
