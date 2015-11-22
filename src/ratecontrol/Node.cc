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
#include "ratecontrol/Node.h"

#include <cassert>

#include "ratecontrol/Message.h"
#include "ratecontrol/MonitorGroup.h"

Node::Node(des::Simulator* _sim, const std::string& _name,
           const des::Model* _parent, u32 _id, MonitorGroup* _monitorGroup,
           u32 _gid)
    : des::Model(_sim, _name, _parent), id(_id), gid(_gid),
      monitorGroup_(_monitorGroup),
      monitorEvent_(this, static_cast<des::EventHandler>(
          &Node::handle_monitor)),
      monitorCount_(0) {
  // get a random seed (try for truly random)
  std::random_device rnd;
  std::uniform_int_distribution<u32> dist;
  u32 seed = dist(rnd);

  // seed the random number generator
  prng.seed(seed);

  // set the first monitor event
  monitorEvent_.time = monitorGroup_->next();
  assert(monitorEvent_.time.valid());
  simulator->addEvent(&monitorEvent_);
}

Node::~Node() {}

void Node::future_recv(Message* _msg, des::Time _time) {
  simulator->addEvent(new Node::RecvEvent(
      this, static_cast<des::EventHandler>(&Node::handle_recv), _msg, _time));
}

u64 Node::cyclesToSend(u32 _size, f64 _rate) {
  // if the number of cycles is not even,
  //  probablistic cycles must be computed
  f64 cycles = _size / _rate;
  f64 fraction = modf(cycles, &cycles);
  if (fraction != 0.0) {
    assert(fraction > 0.0);
    assert(fraction < 1.0);
    f64 rnd = prng.nextF64();
    if (fraction > rnd) {
      cycles += 1.0;
    }
  }
  return (u64)cycles;
}

Node::RecvEvent::RecvEvent(des::Model* _model, des::EventHandler _handler,
                           Message* _msg, des::Time _time)
    : des::Event(_model, _handler, _time), msg(_msg) {}

Node::RecvEvent::~RecvEvent() {}

Node::MonitorEvent::MonitorEvent(des::Model* _model, des::EventHandler _handler)
    : des::Event(_model, _handler) {}

Node::MonitorEvent::~MonitorEvent() {}

void Node::handle_recv(des::Event* _event) {
  Node::RecvEvent* evt = reinterpret_cast<RecvEvent*>(_event);
  monitorCount_ += evt->msg->size;
  this->recv(evt->msg);
  delete evt;
}

void Node::handle_monitor(des::Event* _event) {
  (void)_event;  // unused

  // compute rate
  f64 rate = (f64)monitorCount_ / (f64)monitorGroup_->period;
  dlogf("receive rate %f", rate);

  // set the next monitor event
  monitorEvent_.time = monitorGroup_->next();
  if (monitorEvent_.time.valid()) {
    simulator->addEvent(&monitorEvent_);
  }

  // report the results (must be after addEvent)
  monitorGroup_->done(gid, monitorCount_ > 0);

  // reset count
  monitorCount_ = 0;
}
