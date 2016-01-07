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
#include "ratecontrol/Relay.h"

#include <cassert>

#include "ratecontrol/Message.h"

Relay::Relay(des::Simulator* _sim, const std::string& _name,
             const des::Model* _parent, u32 _id, Network* _network,
             f64 _rate)
    : Node(_sim, _name, _parent, _id, _network), rate_(_rate), nextTime_(0) {
  assert(rate_ > 0.0 && rate_ <= 1.0);
}

Relay::~Relay() {}

void Relay::recv(Message* _msg) {
  assert(_msg->type == Message::RELAY_REQUEST);

  // determine when the message will be sent
  des::Time now = simulator->time();
  nextTime_ = des::Time::max(nextTime_, now.plusEps());  // NOLINT

  // create a response for the source
  Request* req = reinterpret_cast<Request*>(_msg->data);
  Response* resp = new Response();
  resp->reqId = req->reqId;
  Message* respMsg = new Message(id, _msg->src, 1, _msg->trans,
                                 Message::RELAY_RESPONSE, resp);

  // reformat the message for the real destination
  _msg->dst = req->msgDst;
  _msg->size -= 1;
  _msg->type = Message::PLAIN;
  _msg->data = nullptr;
  delete req;

  // send both messages
  future_send(respMsg, nextTime_);
  future_send(_msg, nextTime_);

  // compute the next time based on the token bucket algorithm
  u64 cycles = cyclesToSend(_msg->size, rate_);
  nextTime_ = nextTime_ + cycles;
}
