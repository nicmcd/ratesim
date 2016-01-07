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
#include "ratecontrol/RelaySender.h"

#include <cassert>

#include "ratecontrol/Message.h"
#include "ratecontrol/Relay.h"

RelaySender::RelaySender(des::Simulator* _sim, const std::string& _name,
                         const des::Model* _parent, u32 _id, Network* _network,
                         std::atomic<s64>* _remaining, u32 _minMessageSize,
                         u32 _maxMessageSize, u32 _receiverMinId,
                         u32 _receiverMaxId, u32 _relayMinId, u32 _relayMaxId,
                         u32 _maxOutstanding)
    : Sender(_sim, _name, _parent, _id, _network, _remaining, _minMessageSize,
             _maxMessageSize, _receiverMinId, _receiverMaxId),
      relayMinId_(_relayMinId), relayMaxId_(_relayMaxId), relayReqId_(0),
      maxOutstanding_(_maxOutstanding), outstanding_(0), tryPending_(false) {
  // create the first event
  trySendMessage();
}

RelaySender::~RelaySender() {}

void RelaySender::recv(Message* _msg) {
  assert(_msg->type == Message::RELAY_RESPONSE);
  delete reinterpret_cast<Relay::Response*>(_msg->data);
  delete _msg;

  // decrement the outstanding count for this recv
  assert(outstanding_ > 0);
  outstanding_--;

  // if there isn't a pending try attempt, try now
  if (tryPending_ == false) {
    trySendMessage();
  }
}

void RelaySender::trySendMessage() {
  if (outstanding_ < maxOutstanding_) {
    Message* msg = getNextMessage();
    if (msg) {
      // reformat the message to be a relay request
      Relay::Request* req = new Relay::Request();
      req->reqId = relayReqId_;
      req->msgDst = msg->dst;
      relayReqId_++;
      msg->dst = prng.nextU64(relayMinId_, relayMaxId_);
      msg->size++;  // increase for request header
      msg->type = Message::RELAY_REQUEST;
      msg->data = req;

      // increment outstanding count
      outstanding_++;

      // send the message
      des::Time now = simulator->time();
      future_send(msg, now.plusEps());

      // if outstanding is not maxed out, send another after current is sent
      //  overshoot by one to make sure no overlap
      if (outstanding_ < maxOutstanding_ + 1) {
        des::Time nextTry = now + msg->size;
        des::Event* event = new des::Event(
            this, static_cast<des::EventHandler>(
                &RelaySender::handle_trySendMessage),
            nextTry);
        simulator->addEvent(event);
        tryPending_ = true;
      }
    }
  }
}

void RelaySender::handle_trySendMessage(des::Event* _event) {
  tryPending_ = false;
  trySendMessage();
  delete _event;
}
