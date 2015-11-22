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
#include "ratecontrol/MonitorGroup.h"

#include <cassert>

MonitorGroup::MonitorGroup(des::Simulator* _sim, const std::string& _name,
                           const des::Model* _parent, des::Tick _period,
                           u32 _size)
    : des::Model(_sim, _name, _parent), period(_period), size_(_size),
      anyRecvd_(false), enabled_(true), remaining_(_size) {}

MonitorGroup::~MonitorGroup() {}

des::Time MonitorGroup::next() const {
  if (enabled_) {
    return des::Time(simulator->time() + period, 250);  // use 250 as epsilon
  } else {
    return des::Time();
  }
}

void MonitorGroup::done(u32 _id, bool _recvd) {
  // save rate
  assert(_id < size_);

  // determine if any client received a message
  if (_recvd) {
    dlogf("%u recvd", _id);
    anyRecvd_.store(true);
  }

  // determine if last to report
  u32 rem = remaining_.fetch_sub(1);
  assert(rem > 0);  // shouldn't be possible
  dlogf("rem for id is %u", rem);
  if (enabled_ && rem == 1) {
    // if no client received, disable
    if (!anyRecvd_) {
      dlogf("shutting down");
      enabled_.store(false);
    }

    // reset for next time
    anyRecvd_.store(false);
    remaining_.store(size_);
    dlogf("here!");
  }
}
