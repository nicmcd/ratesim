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
#ifndef RATECONTROL_NODE_H_
#define RATECONTROL_NODE_H_

#include <des/des.h>
#include <prim/prim.h>
#include <rng/Random.h>

#include <string>

class Message;
class Network;

class Node : public des::Model {
 public:
  Node(des::Simulator* _sim, const std::string& _name,
       const des::Model* _parent, u32 _id, Network* _network);
  virtual ~Node();

  /*
   * This creates a future event to receive a message at the specified time.
   */
  void future_recv(Message* _msg, des::Time _time);

  /*
   * When a message is received at this node, this method will be called.
   */
  virtual void recv(Message* _msg) = 0;

  /*
   * This returns the time in which the message will be completely sent by this
   * Node. This is the time in which this node can send another message.
   */
  des::Time send(Message* _msg, f64 _rate);

  const u32 id;

 protected:
  rng::Random prng;

 private:
  class RecvEvent : public des::Event {
   public:
    RecvEvent(des::Model* _model, des::EventHandler _handler, Message* _msg,
              des::Time _time);
    ~RecvEvent();
    Message* msg;
  };

  u64 cyclesToSend(u32 _size, f64 _rate);
  void handle_recv(des::Event* _event);

  Network* network_;
};

#endif  // RATECONTROL_NODE_H_
