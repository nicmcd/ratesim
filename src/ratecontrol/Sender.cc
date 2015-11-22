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
               const des::Model* _parent, u32 _id, MonitorGroup* _monitorGroup,
               u32 _gid, std::atomic<s64>* _remaining, u32 _minMessageSize,
               u32 _maxMessageSize, std::vector<Receiver*>* _receivers)
    : Node(_sim, _name, _parent, _id, _monitorGroup, _gid),
      remaining_(_remaining), minMessageSize_(_minMessageSize),
      maxMessageSize_(_maxMessageSize), receivers_(_receivers) {
  // create the first event
  trySendMessageEvent(des::Time(0));
}

Sender::~Sender() {}

void Sender::trySendMessageEvent(des::Time _time) {
  s64 prev = remaining_->fetch_sub(1);
  if (prev > 0) {
    des::Event* event = new des::Event(this, static_cast<des::EventHandler>(
        &Sender::sendMessageHandler), _time);
    simulator->addEvent(event);
  }
}

void Sender::recv(Message* _msg) {
  dlogf("received message\n");
  delete _msg;
}

void Sender::sendMessageHandler(des::Event* _event) {
  // get random destination
  u32 dst = prng.nextU64(0, receivers_->size() - 1);

  // create a random sized message
  u32 size = prng.nextU64(minMessageSize_, maxMessageSize_);
  u64 cycles = cyclesToSend(size, 1.0);
  Message* msg = new Message(id, receivers_->at(dst)->id, size);
  des::Time time = simulator->time();
  time.tick += cycles;
  dlogf("dst=%u size=%u", dst, size);
  receivers_->at(dst)->future_recv(msg, time);

  // determine how long until the next message
  trySendMessageEvent(time);

  delete _event;
}
