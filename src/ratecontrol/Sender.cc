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
#include "ratecontrol/Sender.h"

#include "ratecontrol/Message.h"
#include "ratecontrol/Receiver.h"

Sender::Sender(des::Simulator* _sim, const std::string& _name,
               const des::Model* _parent, u32 _id, Network* _network,
               std::atomic<s64>* _remaining, u32 _minMessageSize,
               u32 _maxMessageSize, u32 _receiverMinId, u32 _receiverMaxId)
    : Node(_sim, _name, _parent, _id, _network), remaining_(_remaining),
      minMessageSize_(_minMessageSize), maxMessageSize_(_maxMessageSize),
      receiverMinId_(_receiverMinId), receiverMaxId_(_receiverMaxId) {
  // create the first event
  future_sendMessage(des::Time(0));
}

Sender::~Sender() {}

void Sender::future_sendMessage(des::Time _time) {
  s64 prev = remaining_->fetch_sub(1);
  if (prev > 0) {
    des::Event* event = new des::Event(this, static_cast<des::EventHandler>(
        &Sender::handle_sendMessage), _time);
    simulator->addEvent(event);
  }
}

void Sender::recv(Message* _msg) {
  dlogf("received message\n");
  delete _msg;
}

void Sender::handle_sendMessage(des::Event* _event) {
  u32 dst = prng.nextU64(receiverMinId_, receiverMaxId_);
  u32 size = prng.nextU64(minMessageSize_, maxMessageSize_);
  Message* msg = new Message(id, dst, size, 0, nullptr);
  des::Time nextTime = send(msg, 1.0);
  future_sendMessage(nextTime);
  dlogf("sent dst=%u size=%u", dst, size);
  delete _event;
}
