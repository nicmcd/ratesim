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
#include <des/des.h>
#include <prim/prim.h>
#include <tclap/CmdLine.h>

#include <cassert>
#include <cmath>

#include <atomic>
#include <thread>
#include <tuple>
#include <vector>

#include "ratecontrol/BasicSender.h"
#include "ratecontrol/Network.h"
#include "ratecontrol/Receiver.h"
#include "ratecontrol/Sender.h"

Sender* createSender(const std::string& _algorithm, des::Simulator* _sim,
                     const std::string& _name, const des::Model* _parent,
                     u32 _id, Network* _network, std::atomic<s64>* _remaining,
                     u32 _minMessageSize, u32 _maxMessageSize,
                     u32 _receiverMinId, u32 _receiverMaxId, f64 _rateLimit) {
  if (_algorithm == "none") {
    return new BasicSender(
        _sim, _name, _parent, _id, _network, _remaining, _minMessageSize,
        _maxMessageSize, _receiverMinId, _receiverMaxId, _rateLimit);
  } else {
    fprintf(stderr, "invalid algorithm: %s\n", _algorithm.c_str());
    exit(-1);
  }
}

std::string createName(const std::string& _prefix, u32 _id, u32 _total) {
  u32 digits = (u32)ceil(log10(_total));
  std::stringstream ss;
  ss << _prefix << std::setw(digits) << std::setfill('0') << _id;
  return ss.str();
}

s32 main(s32 _argc, char** _argv) {
  /* set defaults here */
  u32 numSenders = 1;
  u32 numReceivers = 1;
  f32 rateLimit = 0.5;
  std::string controlAlgorithm = "none";
  u32 numThreads = 1;
  s64 numMessages = 1000000;  // must be u32
  des::Tick networkDelay = 1000;
  u32 verbosity = 1;

  assert(numMessages >= 0 && numMessages <= U32_MAX);

  try {
    // define command line and arguments
    TCLAP::CmdLine cmd("Rate control simulation", ' ', "0.0.1");
    TCLAP::ValueArg<u32> sArg("s", "senders", "Number of senders",
                              false, numSenders, "u32", cmd);
    TCLAP::ValueArg<u32> rArg("r", "receivers", "Number of receivers",
                              false, numReceivers, "u32", cmd);
    TCLAP::ValueArg<f32> lArg("l", "limit", "rate limit",
                              false, rateLimit, "f32", cmd);
    TCLAP::ValueArg<std::string> aArg("a", "algorithm", "Rate limit algorithm",
                                      false, controlAlgorithm, "string", cmd);
    TCLAP::ValueArg<u32> tArg("t", "threads", "Number of threads",
                              false, numThreads, "u32", cmd);
    TCLAP::ValueArg<u32> mArg("m", "messages", "Number of total messages",
                              false, numMessages, "u32", cmd);
    TCLAP::ValueArg<des::Tick> dArg("d", "delay",
                                    "Network delay (latency in cycles)",
                                    false, networkDelay, "des::Tick", cmd);
    TCLAP::ValueArg<u32> vArg("v", "verbose", "Turn on verbosity",
                              false, verbosity, "u32", cmd);
    // parse the command line
    cmd.parse(_argc, _argv);
    // gather arguments
    numSenders = sArg.getValue();
    numReceivers = rArg.getValue();
    rateLimit = lArg.getValue();
    controlAlgorithm = aArg.getValue();
    numThreads = tArg.getValue();
    numMessages = (s64)mArg.getValue();
    networkDelay = dArg.getValue();
    verbosity = vArg.getValue();
  } catch (TCLAP::ArgException &e) {
    fprintf(stderr, "error: %s for arg %s\n",
            e.error().c_str(), e.argId().c_str());
    exit(-1);
  }

  // verify inputs
  if (numSenders < 1) {
    fprintf(stderr, "there must be at least one sender\n");
    exit(-1);
  }
  if (numReceivers < 1) {
    fprintf(stderr, "there must be at least one receiver\n");
    exit(-1);
  }
  if (rateLimit <= 0.0 || rateLimit > 1.0) {
    fprintf(stderr, "rate limit must be greater than 0.0 and less than or "
            "equal to 1.0\n");
    exit(-1);
  }

  // print args
  if (verbosity > 0) {
    printf("Settings:\n"
           "senders   : %u\n"
           "receivers : %u\n"
           "limit     : %f\n"
           "algorithm : %s\n"
           "threads   : %u\n"
           "messages  : %li\n"
           "delay     : %lu\n"
           "\n",
           numSenders, numReceivers, rateLimit, controlAlgorithm.c_str(),
           numThreads, numMessages, (u64)networkDelay);
  }

  // create the simulation environment
  des::Simulator sim(numThreads);

  // create a Network
  Network network(&sim, "N", nullptr, networkDelay);
  network.debug = verbosity > 1;

  // create receivers
  u32 nodeId = 0;
  std::vector<Receiver*> receivers(numReceivers, nullptr);
  for (u32 r = 0; r < numReceivers; r++) {
    receivers.at(r) = new Receiver(&sim, createName("R", r, numReceivers),
                                   &network, nodeId++, &network);
    receivers.at(r)->debug = verbosity > 1;
  }

  // create senders
  std::atomic<s64> remainingSendMessages(numMessages);
  std::vector<Sender*> senders(numSenders, nullptr);
  for (u32 s = 0; s < numSenders; s++) {
    senders.at(s) = createSender(controlAlgorithm,
        &sim, createName("S", s, numSenders), &network,
        nodeId++, &network, &remainingSendMessages, 1,
        1000, receivers.at(0)->id,
        receivers.at(numReceivers - 1)->id, rateLimit);
    senders.at(s)->debug = verbosity > 1;
  }

  // run simulation
  sim.simulate(verbosity > 0);

  // cleanup
  for (u32 r = 0; r < numReceivers; r++) {
    delete receivers.at(r);
  }
  for (u32 s = 0; s < numSenders; s++) {
    delete senders.at(s);
  }

  return 0;
}
