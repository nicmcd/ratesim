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
#ifndef RATECONTROL_SENDER_H_
#define RATECONTROL_SENDER_H_

#include <des/des.h>
#include <prim/prim.h>

#include <string>

#include "ratecontrol/Node.h"

class Network;
class Receiver;

class Sender : public Node {
 public:
  Sender(des::Simulator* _sim, const std::string& _name,
         const des::Model* _parent, u32 _id, const std::string& _queuing,
         Network* _network, u32 _minMessageSize, u32 _maxMessageSize,
         u32 _receiverMinId, u32 _receiverMaxId);
  virtual ~Sender();

  void setInjectionRate(f64 _rate);
  f64 getInjectionRate() const;

 protected:
  /*
   * Subclasses must override this to implement custom send algorithms.
   */
  virtual void sendMessage(Message* _msg) = 0;

  const u32 minMessageSize;
  const u32 maxMessageSize;

 private:
  void handle_injectionRateEvent(des::Event* _event);
  void handle_sendMessage(des::Event* _event);

  f64 injectionRate_;
  const u32 receiverMinId_;
  const u32 receiverMaxId_;
  u32 messageCount_;
};

#endif  // RATECONTROL_SENDER_H_
