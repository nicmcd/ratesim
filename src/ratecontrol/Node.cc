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

#include <chrono>

Node::Node(des::Simulator* _sim, const std::string& _name,
           const des::Model* _parent, u32 _id)
    : des::Model(_sim, _name, _parent), id(_id) {
  // seed the random number generator with the id
  prng.seed(std::chrono::system_clock::now().time_since_epoch().count() + id);
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

void Node::handle_recv(des::Event* _event) {
  Node::RecvEvent* evt = reinterpret_cast<RecvEvent*>(_event);
  this->recv(evt->msg);
  delete evt;
}
