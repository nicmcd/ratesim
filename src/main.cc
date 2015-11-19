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
#include <des/Event.h>
#include <des/Model.h>
#include <des/Simulator.h>
#include <prim/prim.h>
#include <tclap/CmdLine.h>

#include <cmath>

#include <atomic>
#include <memory>
#include <thread>
#include <tuple>
#include <vector>

#include "ratecontrol/Receiver.h"
#include "ratecontrol/Sender.h"

/*
des::Model* createModel(const std::string& _type, des::Simulator* _sim,
                        const std::string& _name, u64 _id, u64 _events,
                        bool _shiftyEpsilon, bool _verbose) {
  if (_type == "empty") {
    return new example::EmptyModel(
        _sim, _name, nullptr, _id, _events, _shiftyEpsilon, _verbose);
  } else if (_type == "sha1") {
    return new example::Sha1Model(
        _sim, _name, nullptr, _id, _events, _shiftyEpsilon, _verbose);
  } else if (_type == "sha512") {
    return new example::Sha512Model(
        _sim, _name, nullptr, _id, _events, _shiftyEpsilon, _verbose);
  } else if (_type == "simple") {
    return new example::SimpleModel(
        _sim, _name, nullptr, _id, _events, _shiftyEpsilon, _verbose);
  } else {
    fprintf(stderr, "invalid model type: %s\n", _type.c_str());
    exit(-1);
  }
}
*/

std::string createName(const std::string& _prefix, u32 _id, u32 _total) {
  u32 digits = (u32)ceil(log10(_total));
  std::stringstream ss;
  ss << _prefix << std::setw(digits) << std::setfill('0') << _id;
  return ss.str();
}

s32 main(s32 _argc, char** _argv) {
  /* set defaults here */
  u32 numSenders = 100;
  u32 numReceivers = 50;
  f32 rateLimit = 0.5;
  std::string controlAlgorithm = "none";
  u32 numThreads = 1;
  s64 numMessages = 1000000;  // must be u32
  bool verbose;

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
    TCLAP::SwitchArg vArg("v", "verbose", "Turn on verbosity",
                          cmd, false);
    // parse the command line
    cmd.parse(_argc, _argv);
    // gather arguments
    numSenders = sArg.getValue();
    numReceivers = rArg.getValue();
    rateLimit = lArg.getValue();
    controlAlgorithm = aArg.getValue();
    numThreads = tArg.getValue();
    numMessages = (s64)mArg.getValue();
    verbose = vArg.getValue();
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
  if (verbose) {
    printf("Settings:\n"
           "senders   : %u\n"
           "receivers : %u\n"
           "limit     : %f\n"
           "algorithm : %s\n"
           "threads   : %u\n"
           "messages  : %lu\n",
           numSenders, numReceivers, rateLimit, controlAlgorithm.c_str(),
           numThreads, numMessages);
  }

  // create the simulation environment
  des::Simulator sim(numThreads);

  // create models and components
  std::atomic<s64> remainingSendMessages(numMessages);
  std::vector<std::shared_ptr<Sender> > senders(numSenders, nullptr);
  for (u32 s = 0; s < numSenders; s++) {
    senders.at(s) = std::shared_ptr<Sender>(
        new Sender(&sim, createName("S", s, numSenders), nullptr,
                   &remainingSendMessages));
  }
  std::vector<std::shared_ptr<Receiver> > receivers(numReceivers, nullptr);
  for (u32 r = 0; r < numReceivers; r++) {
    receivers.at(r) = std::shared_ptr<Receiver>(
        new Receiver(&sim, createName("R", r, numReceivers), nullptr));
  }

  // run simulation
  sim.simulate(true);

  return 0;
}
