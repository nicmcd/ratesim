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
      // common parameters
      maxTokens_(_settings["params"]["max_tokens"].asUInt64()),
      // stealing parameters
      stealTokens_(_settings["steal_tokens"].asBool()),
      stealRate_(_settings["steal_rate"].asBool()),
      stealThreshold_(_settings["params"]["steal_threshold"].asDouble()),
      tokenAskFactor_(_settings["params"]["token_ask_factor"].asDouble()),
      rateAskFactor_(_settings["params"]["rate_ask_factor"].asDouble()),
      maxRequestsOutstanding_(
          _settings["params"]["max_requests_outstanding"].asUInt()),
      // giving parameters
      giveTokenThreshold_(
          _settings["params"]["give_token_threshold"].asDouble()),
      giveRateThreshold_(
          _settings["params"]["give_rate_threshold"].asDouble()),
      maxRateGiveFactor_(
          _settings["params"]["max_rate_give_factor"].asDouble()),
      // init FSMs
      distReqId_(0),
      tokens_(maxTokens_),
      lastTick_(0),
      rateAsked_(0.0),
      queueSize_(0),
      requestsOutstanding_(0),
      waiting_(false) {
  // verify settings fields
  assert(!_settings["params"]["max_tokens"].isNull());
  assert(!_settings["steal_tokens"].isNull());
  assert(!_settings["steal_rate"].isNull());
  assert(!_settings["params"]["steal_threshold"].isNull());
  assert(!_settings["params"]["token_ask_factor"].isNull());
  assert(!_settings["params"]["rate_ask_factor"].isNull());
  assert(!_settings["params"]["max_requests_outstanding"].isNull());
  assert(!_settings["params"]["give_token_threshold"].isNull());
  assert(!_settings["params"]["give_rate_threshold"].isNull());
  assert(!_settings["params"]["max_rate_give_factor"].isNull());

  // verify const settings values
  assert(maxTokens_ >= minMessageSize);
  assert(stealThreshold_ >= 0.0 && stealThreshold_ <= 1.0);
  assert(tokenAskFactor_ > 0.0 && tokenAskFactor_ <= 1.0);
  assert(rateAskFactor_ > 0.0 && rateAskFactor_ <= 1.0);
  assert(maxRequestsOutstanding_ > 0);
  assert(giveTokenThreshold_ >= 0.0 && giveTokenThreshold_ <= 1.0);
  assert(giveRateThreshold_ >= 0.0 && giveRateThreshold_ <= 1.0);
  assert(maxRateGiveFactor_ > 0.0 && maxRateGiveFactor_ <= 1.0);

  if (stealRate_) {
    // there is the case where we have more than the threshold but less
    //  than the message size. if rate=0, can't every recover
    assert((stealThreshold_ * maxTokens_) >= maxMessageSize);
  }

  // add debug stats print
  simulator->addEvent(new des::Event(
      this, static_cast<des::EventHandler>(&DistSender::showStats),
      des::Time(0)));
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

  // make sure we don't give anything if we are stealing
  u32 tokens = getTokens();
  if (requestsOutstanding_ > 0) {  // TODO(nicmcd): try without this!
    tokens = 0;
  }

  // give tokens as requested and available above token threshold
  u32 giveTokens = 0;
  u32 giveTokenTrigger = (u32)(giveTokenThreshold_ * maxTokens_);
  if (tokens >= giveTokenTrigger) {
    giveTokens = tokens - giveTokenTrigger;  // only give excess tokens
  }

  // remove token being given away
  res->tokens = std::min(giveTokens, req->tokens);
  removeTokens(res->tokens);

  // give rate as requested and available above rate threshold
  res->rateReq = req->rate;
  f64 giveRateTrigger = giveRateThreshold_ * maxTokens_;
  if ((req->rate > 0.0) && (tokens >= giveRateTrigger)) {
    // determine the factor of giving
    f64 giveFactor = maxRateGiveFactor_;
    /*
    f64 giveFactor = ((f64)(tokens - giveRateThreshold_) /
                      (f64)(maxTokens_ - giveRateThreshold_));
    giveFactor *= maxRateGiveFactor_;
    */

    // determine the give rate
    res->givenRate = removeRate(giveFactor, req->rate);
  } else {
    res->givenRate = 0.0;
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
  dlogf("recvd steal response %lu from %u for %u tokens and %f rate (%f)",
        res->reqId, _msg->src, res->tokens, res->givenRate, res->rateReq);
  addTokens(res->tokens);
  addRate(res->givenRate);
  rateAsked_ -= res->rateReq;

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
  // see if steal requests need to be sent
  processSteal();

  // send all messages that we have tokens for
  while (!sendQueue_.empty()) {
    // get the current token count
    u32 tokens = getTokens();

    // peek at the next message
    Message* msg = sendQueue_.front();

    // try to send the message
    if (tokens >= msg->size) {
      dlogf("sending a message");
      // send the message
      send(msg);

      // remove the used tokens
      removeTokens(msg->size);

      // remove the sent message from the queue
      sendQueue_.pop();
      queueSize_ -= msg->size;

      // we might need steal requests now
      processSteal();
    } else if (!waiting_) {
      // can't send the message, wait for tokens to arrive
      dlogf("creating wait period");
      waiting_ = true;
      des::Tick tokensNeeded = msg->size - tokens;
      des::Time wakeUp = simulator->time();
      wakeUp += (u64)(tokensNeeded / std::max(0.001, getRate()));
      assert(wakeUp != simulator->time());
      simulator->addEvent(new des::Event(this, static_cast<des::EventHandler>(
          &DistSender::handle_wait), wakeUp));
    } else {
      break;
    }
  }
}

void DistSender::processSteal() {
  // get the current token count
  u32 tokens = getTokens();

  // conditions for stealing
  bool lowWaterTrigger = tokens < (stealThreshold_ * maxTokens_);
  bool canStealTokens = stealTokens_ && (tokens < maxTokens_);
  bool canStealRate = stealRate_ && ((getRate() + rateAsked_) < 0.9999);
  bool stealsAvailable = requestsOutstanding_ < maxRequestsOutstanding_;

  if ((canStealTokens || canStealRate) &&
      stealsAvailable && lowWaterTrigger) {
    /*
    // if enabled and needed, issue steal requests
    if ((stealTokens_ || (stealRate_ && getRate() < 1.0)) &&
      (requestsOutstanding_ < maxRequestsOutstanding_) &&
      (tokens <= (stealThreshold_ * maxTokens_))) {
    */

    // create a random set with destination peers
    rnd::Queue<u32> peers(&prng);
    peers.add(distMinId_, distMaxId_);
    peers.erase(id);

    // issue all available steal requests
    distReqId_++;
    u32 numReqs = maxRequestsOutstanding_ - requestsOutstanding_;
    for (u32 rr = 0; rr < numReqs; rr++) {
      // format the steal request
      DistSender::Request* req = new DistSender::Request();
      req->reqId = 0x1000000000000000lu | ((u64)id << 32) | distReqId_;

      // request enough tokens for the whole queue
      u32 askTokens = maxTokens_ - tokens;
      req->tokens = stealTokens_ ? askTokens * tokenAskFactor_ : 0;

      // divide the remaining rate by number of requests incase all the
      //  responders say yes. we can only have a total of 1.0 rate
      f64 askRate = ((1.0 - getRate() - rateAsked_) * rateAskFactor_) / numReqs;
      req->rate = stealRate_ ? askRate : 0.0;
      rateAsked_ += askRate;
      assert(getRate() + rateAsked_ < 1.00001);

      assert(req->tokens > 0 || req->rate > 0.0);

      // choose a random peer
      assert(peers.size() > 0);
      u32 peer = peers.pop();
      assert(peer != id);

      // create and send the request message
      send(new Message(id, peer, 1, 0, Message::DIST_REQUEST, req,
                       simulator->time().tick));

      // increment the requests outstanding counter
      requestsOutstanding_++;

      dlogf("sent steal request %lu to %u for %u tokens and %f rate",
            req->reqId, peer, req->tokens, req->rate);
    }
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

f64 DistSender::getRate() const {
  return std::min(1.0, rate_);
}

f64 DistSender::removeRate(f64 _factor, f64 _max) {
  assert(_factor >= 0.0 && _factor <= 1.0);
  f64 take = std::min(_factor * rate_, _max);
  rate_ -= take;
  assert(rate_ >= 0.0);
  return take;
}

void DistSender::addRate(f64 _rate) {
  assert(_rate >= 0.0);
  rate_ += _rate;
  assert(rate_ >= 0.0);  // not needed?
}

void DistSender::showStats(des::Event* _event) {
  dlogf("tokens=%u rate=%f", getTokens(), rate_);
  delete _event;

  if (simulator->time() <= 90000) {
    simulator->addEvent(new des::Event(
        this, static_cast<des::EventHandler>(&DistSender::showStats),
        simulator->time() + 1000));
  }
}
