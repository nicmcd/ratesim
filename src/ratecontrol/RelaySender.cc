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
                         const des::Model* _parent, u32 _id,
                         const std::string& _queuing, Network* _network,
                         u32 _minMessageSize, u32 _maxMessageSize,
                         u32 _receiverMinId, u32 _receiverMaxId,
                         Json::Value _settings)
    : Sender(_sim, _name, _parent, _id, _queuing, _network, _minMessageSize,
             _maxMessageSize, _receiverMinId, _receiverMaxId),
      relayReqId_(0),
      maxOutstanding_(_settings["max_outstanding"].asUInt()),
      credits_(_settings["max_outstanding"].asUInt()) {
  assert(!_settings["max_outstanding"].isNull());
  assert(maxOutstanding_ > 0);
}

RelaySender::~RelaySender() {}

void RelaySender::relayIds(u32 _relayMinId, u32 _relayMaxId) {
  relayMinId_ = _relayMinId;
  relayMaxId_ = _relayMaxId;
}

void RelaySender::recv(Message* _msg) {
  assert(_msg->type == Message::RELAY_RESPONSE);
  delete reinterpret_cast<Relay::Response*>(_msg->data);
  delete _msg;

  // decrement the outstanding count for this recv
  assert(credits_ <= maxOutstanding_);
  credits_++;

  // process the send queue
  processQueue();
}

void RelaySender::sendMessage(Message* _msg) {
  // reformat the message to be a relay request
  Relay::Request* req = new Relay::Request();
  req->reqId = relayReqId_;
  relayReqId_++;
  req->msgDst = _msg->dst;
  _msg->dst = prng.nextU64(relayMinId_, relayMaxId_);
  _msg->size++;  // increase for request header
  _msg->type = Message::RELAY_REQUEST;
  _msg->data = req;

  // add to queue
  sendQueue_.push(_msg);

  // process the send queue
  processQueue();
}

void RelaySender::processQueue() {
  // send all available messages
  while (!sendQueue_.empty() && credits_ > 0) {
    // pop the next message
    Message* _msg = sendQueue_.front();
    sendQueue_.pop();

    // send the message
    send(_msg);

    // decrement the credit count
    credits_--;
  }
}
