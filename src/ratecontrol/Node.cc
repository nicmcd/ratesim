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
#include "ratecontrol/Network.h"

Node::Node(des::Simulator* _sim, const std::string& _name,
           const des::Model* _parent, u32 _id, Network* _network)
    : des::Model(_sim, _name, _parent), id(_id), nextSendTime_(0),
      network_(_network) {
  // get a random seed (try for truly random)
  std::random_device rnd;
  std::uniform_int_distribution<u32> dist;
  u32 seed = dist(rnd);

  // seed the random number generator
  prng.seed(seed);

  // register with the network
  network_->registerNode(id, this);
}

Node::~Node() {}

void Node::future_recv(Message* _msg, des::Time _time) {
  simulator->addEvent(new MessageEvent(
      this, static_cast<des::EventHandler>(&Node::handle_recv), _time, _msg));
}

void Node::send(Message* _msg) {
  // renew the next send time
  nextSendTime_ = des::Time::max(simulator->time().plusEps(),  // NOLINT
                                 nextSendTime_);

  // create and add the send message event
  simulator->addEvent(new MessageEvent(
      this, static_cast<des::EventHandler>(&Node::handle_send), nextSendTime_,
      _msg));
}

void Node::send(Message* _msg, des::Time _time) {
  simulator->addEvent(new MessageEvent(
      this, static_cast<des::EventHandler>(&Node::handle_delayedSend), _time,
      _msg));
}

u64 Node::cyclesToSend(u32 _size, f64 _rate) {
  // if the number of cycles is not even,
  //  probabilistic cycles must be computed
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

void Node::handle_recv(des::Event* _event) {
  MessageEvent* evt = reinterpret_cast<MessageEvent*>(_event);
  dlogf("%s", evt->msg->toString().c_str());
  this->recv(evt->msg);
  delete evt;
}

void Node::handle_send(des::Event* _event) {
  MessageEvent* evt = reinterpret_cast<MessageEvent*>(_event);
  Node* node = network_->getNode(evt->msg->dst);
  des::Time now = simulator->time();
  des::Time recv(now + evt->msg->size + network_->delay());
  node->future_recv(evt->msg, recv);
  dlogf("%s", evt->msg->toString().c_str());
  delete evt;
}

void Node::handle_delayedSend(des::Event* _event) {
  MessageEvent* evt = reinterpret_cast<MessageEvent*>(_event);
  send(evt->msg);
  delete evt;
}
