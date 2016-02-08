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
#ifndef RATECONTROL_DISTSENDER_H_
#define RATECONTROL_DISTSENDER_H_

#include <jsoncpp/json/json.h>
#include <prim/prim.h>

#include <queue>
#include <string>

#include "ratecontrol/Sender.h"

class Message;
class Network;

class DistSender : public Sender {
 public:
  struct Request {
    u64 reqId;
    u32 tokens;
    f64 rate;
  };
  struct Response {
    u64 reqId;
    u32 tokens;
    f64 rate;
  };

  DistSender(des::Simulator* _sim, const std::string& _name,
             const des::Model* _parent, u32 _id, const std::string& _queuing,
             Network* _network, u32 _minMessageSize, u32 _maxMessageSize,
             u32 _receiverMinId, u32 _receiverMaxId, f64 _rateLimit,
             Json::Value _settings);
  ~DistSender();
  void distIds(u32 _distMinId, u32 _distMaxId);

  void recv(Message* _msg) override;

 protected:
  void sendMessage(Message* _msg) override;

 private:
  // this handles steal requests
  void recvRequest(Message* _msg);

  // this handles steal responses
  void recvResponse(Message* _msg);

  // this handles waiting events to send another message
  void handle_wait(des::Event* _event);

  // this processes the send queue
  void processQueue();

  // this returns the current amount of tokens this sender has
  u32 getTokens();

  // this adds tokens to this sender
  void addTokens(u32 _tokens);

  // this removes tokens from this sender
  void removeTokens(u32 _tokens);

  // this removes a portion of the rate as specified by _factor (a percentage)
  //  and takes no more than _max (an absolute). the removed value is returned.
  f64 removeRate(f64 _factor, f64 _max);

  // this adds the specified rate value to this sender's rate
  void addRate(f64 _rate);

  // DEBUG - print some stats
  void showStats(des::Event* _event);

  const f64 distRate_;
  u32 distMinId_;
  u32 distMaxId_;

  const bool stealTokens_;
  const bool stealRate_;
  const u32 maxTokens_;  // bucket size
  const f64 tokenThreshold_;  // bucket percentage
  const f64 rateThreshold_;  // bucket percentage
  const u64 stealThreshold_;  // bucket percentage
  const f64 maxRateGiveFactor_;
  const u32 maxRequestsOutstanding_;

  u64 distReqId_;
  f64 rate_;
  f64 tokens_;
  des::Tick lastTick_;

  std::queue<Message*> sendQueue_;
  u64 queueSize_;

  u32 requestsOutstanding_;
  bool waiting_;
};

#endif  // RATECONTROL_DISTSENDER_H_
