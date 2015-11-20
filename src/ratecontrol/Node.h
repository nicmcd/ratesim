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

class Node : public des::Model {
 public:
  Node(des::Simulator* _sim, const std::string& _name,
       const des::Model* _parent, u32 _id);
  virtual ~Node();

  void future_recv(Message* _msg, des::Time _time);
  virtual void recv(Message* _msg) = 0;

  const u32 id;

 protected:
  u64 cyclesToSend(u32 _size, f64 _rate);

  rng::Random prng;

 private:
  class RecvEvent : public des::Event {
   public:
    RecvEvent(des::Model* _model, des::EventHandler _handler, Message* _msg,
              des::Time _time);
    ~RecvEvent();
    Message* msg;
  };

  void handle_recv(des::Event* _event);
};

#endif  // RATECONTROL_NODE_H_
