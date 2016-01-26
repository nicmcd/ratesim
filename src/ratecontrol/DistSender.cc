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
#include "ratecontrol/DistSender.h"

#include <rnd/Queue.h>

#include <cassert>

#include <algorithm>

#include "ratecontrol/Message.h"

DistSender::DistSender(des::Simulator* _sim, const std::string& _name,
                       const des::Model* _parent, u32 _id,
                       const std::string& _queuing, Network* _network,
                       u32 _minMessageSize, u32 _maxMessageSize,
                       u32 _receiverMinId, u32 _receiverMaxId, f64 _rateLimit,
                       Json::Value _settings)
    : Sender(_sim, _name, _parent, _id, _queuing, _network, _minMessageSize,
             _maxMessageSize, _receiverMinId, _receiverMaxId),
      distRate_(_rateLimit),
      stealTokens_(_settings["steal_tokens"].asBool()),
      stealRate_(_settings["steal_rate"].asBool()),
      maxTokens_(_settings["max_tokens"].asUInt64()),
      tokenThreshold_(_settings["token_threshold"].asUInt64()),
      rateThreshold_(_settings["rate_threshold"].asUInt64()),
      stealThreshold_(_settings["steal_threshold"].asUInt64()),
      maxRateGiveFactor_(_settings["max_rate_give_factor"].asDouble()),
      maxRequestsOutstanding_(_settings["max_requests_outstanding"].asUInt()),
      distReqId_(0),
      tokens_(0.0),
      lastTick_(0),
      queueSize_(0),
      requestsOutstanding_(0),
      waiting_(false) {
  // verify settings fields
  assert(_settings.isMember("steal_tokens"));
  assert(_settings.isMember("steal_rate"));
  assert(_settings.isMember("max_tokens"));
  assert(_settings.isMember("token_threshold"));
  assert(_settings.isMember("rate_threshold"));
  assert(_settings.isMember("steal_threshold"));
  assert(_settings.isMember("max_rate_give_factor"));
  assert(_settings.isMember("max_requests_outstanding"));

  // verify const settings values
  assert(maxTokens_ >= minMessageSize);
  assert(tokenThreshold_ < maxTokens_);
  assert(rateThreshold_ <= maxTokens_);
  assert(maxRateGiveFactor_ > 0.0 && maxRateGiveFactor_ <= 1.0);
  assert(maxRequestsOutstanding_ > 0);

  // add debug stats print
  simulator->addEvent(new des::Event(
      this, static_cast<des::EventHandler>(&DistSender::showStats),
      des::Time(9999)));
  simulator->addEvent(new des::Event(
        this, static_cast<des::EventHandler>(&DistSender::showStats),
      des::Time(19876)));
}

DistSender::~DistSender() {}

void DistSender::distIds(u32 _distMinId, u32 _distMaxId) {
  distMinId_ = _distMinId;
  distMaxId_ = _distMaxId;
  u32 totalDistSenders = distMaxId_ - distMinId_ + 1;
  rate_ = distRate_ / totalDistSenders;
  assert(rate_ > 0.0 && rate_ <= 1.0);
  assert(maxRequestsOutstanding_ <= totalDistSenders - 1);
}

void DistSender::recv(Message* _msg) {
  if (_msg->type == Message::DIST_REQUEST) {
    recvRequest(_msg);
  } else if (_msg->type == Message::DIST_RESPONSE) {
    recvResponse(_msg);
  } else {
    assert(false);
  }
}

void DistSender::sendMessage(Message* _msg) {
  // add to queue
  sendQueue_.push(_msg);
  queueSize_ += _msg->size;

  // process the send queue
  processQueue();
}

void DistSender::recvRequest(Message* _msg) {
  assert(_msg->size == 1);
  assert(_msg->data);

  // create and format the reponse
  DistSender::Request* req = reinterpret_cast<DistSender::Request*>(_msg->data);
  DistSender::Response* res = new DistSender::Response();

  dlogf("recvd steal request %lu from %u for %u tokens and %f rate",
        req->reqId, _msg->src, req->tokens, req->rate);
  assert(req->tokens > 0 || req->rate > 0.0);

  // request id
  res->reqId = req->reqId;

  // make sure we don't give anything if we are stealing or waiting
  u32 tokens = getTokens();
  if (requestsOutstanding_ > 0 || waiting_) {
    tokens = 0;
  }

  // give tokens as requested and available above token threshold
  u32 giveTokens = 0;
  if (tokens >= tokenThreshold_) {
    giveTokens = tokens - tokenThreshold_;
  }

  // remove token being given away
  res->tokens = std::min(std::max(0u, giveTokens), req->tokens);
  removeTokens(res->tokens);

  // give rate as requested and available above rate threshold
  if ((req->rate > 0.0) && (tokens >= rateThreshold_)) {
    // determine the factor of giving
    f64 giveFactor = ((f64)(tokens - rateThreshold_) /
                      (f64)(maxTokens_ - rateThreshold_));
    giveFactor *= maxRateGiveFactor_;

    // determine the give rate
    res->rate = removeRate(giveFactor, req->rate);
  } else {
    res->rate = 0;
  }

  // reverse the message back to the requester
  u32 tmp = _msg->src;
  _msg->src = _msg->dst;
  _msg->dst = tmp;
  _msg->type = Message::DIST_RESPONSE;
  _msg->data = res;
  delete req;

  // send the response
  send(_msg);
}

void DistSender::recvResponse(Message* _msg) {
  // update the current number of tokens (REQUIRED!)
  getTokens();

  assert(_msg->size == 1);
  assert(_msg->data);

  // consume the stolen values
  DistSender::Response* res =
      reinterpret_cast<DistSender::Response*>(_msg->data);
  dlogf("recvd steal response %lu from %u for %u tokens and %f rate",
        res->reqId, _msg->src, res->tokens, res->rate);
  addTokens(res->tokens);
  addRate(res->rate);

  // cleanup
  delete res;
  delete _msg;

  // process the send queue
  assert(requestsOutstanding_ > 0);
  requestsOutstanding_--;
  processQueue();
}

void DistSender::handle_wait(des::Event* _event) {
  assert(waiting_);
  waiting_ = false;
  processQueue();
  delete _event;
}

void DistSender::processQueue() {
  // send all messages that we have tokens for
  while (!sendQueue_.empty()) {
    // get the current token count
    u32 tokens = getTokens();

    // peek at the next message
    Message* _msg = sendQueue_.front();

    // try to send the message
    if (tokens >= _msg->size) {
      dlogf("sending a message");
      // send the message
      send(_msg);

      // remove the used tokens
      removeTokens(_msg->size);

      // remove the sent message from the queue
      sendQueue_.pop();
      queueSize_ -= _msg->size;
      continue;
    }

    // this figures out how many ticks we'd have to wait at the current rate
    f64 tokensShort = (_msg->size - tokens_) / rate_;

    // can't send the message, try to send a steal request
    if ((stealTokens_ || (stealRate_ && rate_ < 1.0)) &&  // stealing enabled
        (requestsOutstanding_ == 0) &&  // no outstanding requests
        (tokensShort > stealThreshold_)) {  // steal threshold triggered
      // create a random set with destination peers
      rnd::Queue<u32> peers(&prng);
      peers.add(distMinId_, distMaxId_);
      peers.erase(id);

      // the message can't be sent, but we can send steal requests
      distReqId_++;
      while (requestsOutstanding_ < maxRequestsOutstanding_) {
        // format the steal request
        DistSender::Request* req = new DistSender::Request();
        req->reqId = 0x1000000000000000lu | ((u64)id << 32) | distReqId_;

        // request enough tokens for the whole queue
        req->tokens = stealTokens_ ? queueSize_ / maxRequestsOutstanding_ : 0;

        // divide the remaining rate by number of requests incase all the
        //  responders say yes. we can only have a total of 1.0 rate
        req->rate = stealRate_ ? (1.0 - rate_) / maxRequestsOutstanding_ : 0.0;

        // choose a random peer
        assert(peers.size() > 0);
        u32 peer = peers.pop();
        assert(peer != id);

        // create and send the request message
        send(new Message(id, peer, 1, 0, Message::DIST_REQUEST, req));

        // increment the requests outstanding counter
        requestsOutstanding_++;

        dlogf("sent steal request %lu to %u for %u tokens and %f rate",
              req->reqId, peer, req->tokens, req->rate);
      }
      break;
    }

    // can't send the message or a steal request, just wait for tokens
    if ((!waiting_) &&  // not already waiting
        (requestsOutstanding_ == 0) &&  // no outstanding requests
        ((!stealTokens_ && !stealRate_) ||  // stealing disabled
         (tokensShort <= stealThreshold_))) {  // steal threshold not triggered
      dlogf("starting wait period");
      // the message can't be sent. for one of various reasons we've decided to
      // wait for credit accumulation instead of stealing
      des::Tick tokensNeeded = _msg->size - tokens;
      des::Time wakeUp = simulator->time() + (u64)(tokensNeeded / rate_);
      des::Event* evt = new des::Event(this, static_cast<des::EventHandler>(
          &DistSender::handle_wait), wakeUp);
      simulator->addEvent(evt);
      waiting_ = true;
      break;
    }

    // right now we can't do anything productive
    break;
  }
}

u32 DistSender::getTokens() {
  des::Tick now = simulator->time().tick;
  if (now > lastTick_) {
    tokens_ += ((now - lastTick_) * rate_);
    tokens_ = std::min(tokens_, (f64)maxTokens_);
    lastTick_ = now;
  }
  return (u32)tokens_;
}

void DistSender::addTokens(u32 _tokens) {
  tokens_ += _tokens;
  tokens_ = std::min(tokens_, (f64)maxTokens_);
}

void DistSender::removeTokens(u32 _tokens) {
  tokens_ -= _tokens;
  assert(tokens_ >= 0.0);
}

f64 DistSender::removeRate(f64 _factor, f64 _max) {
  assert(_factor >= 0.0 && _factor <= 1.0);
  f64 take = std::min(_factor * rate_, _max);
  rate_ -= take;
  assert(rate_ >= 0.0 && rate_ <= 1.0);
  return take;
}

void DistSender::addRate(f64 _rate) {
  assert(_rate >= 0.0);
  rate_ += _rate;
  assert(rate_ >= 0.0);
  assert(rate_ <= 1.01);
  rate_ = std::min(1.0, rate_);
}

void DistSender::showStats(des::Event* _event) {
  dlogf("tokens=%f rate=%f", getTokens(), rate_);
  delete _event;
}
