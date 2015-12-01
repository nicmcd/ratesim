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
#include "ratecontrol/BasicSender.h"

#include <cassert>

#include "ratecontrol/Message.h"

BasicSender::BasicSender(des::Simulator* _sim, const std::string& _name,
                         const des::Model* _parent, u32 _id, Network* _network,
                         std::atomic<s64>* _remaining, u32 _minMessageSize,
                         u32 _maxMessageSize, u32 _receiverMinId,
                         u32 _receiverMaxId, f64 _rate)
    : Sender(_sim, _name, _parent, _id, _network, _remaining, _minMessageSize,
             _maxMessageSize, _receiverMinId, _receiverMaxId), rate_(_rate) {
  // create the first event
  future_sendMessage(des::Time(0));
}

BasicSender::~BasicSender() {}

void BasicSender::recv(Message* _msg) {
  (void)_msg;  // unused
  assert(false);
}

BasicSender::MessageEvent::MessageEvent(
    BasicSender* _sender, des::EventHandler _handler, des::Time _time,
    Message* _msg)
    : des::Event(_sender, _handler, _time), msg(_msg) {}

BasicSender::MessageEvent::~MessageEvent() {}

void BasicSender::future_sendMessage(des::Time _time) {
  Message* msg = getNextMessage();
  if (msg) {
    des::Event* event = new MessageEvent(
        this, static_cast<des::EventHandler>(&BasicSender::handle_sendMessage),
        _time, msg);
    simulator->addEvent(event);
  }
}

void BasicSender::handle_sendMessage(des::Event* _event) {
  MessageEvent* msgEvt = reinterpret_cast<MessageEvent*>(_event);
  Message* msg = msgEvt->msg;

  des::Time nextTime = send(msg, rate_);
  future_sendMessage(nextTime);
  dlogf("sent dst=%u size=%u", msg->dst, msg->size);
  delete _event;
}
