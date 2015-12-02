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
#include <jsoncpp/json/json.h>
#include <prim/prim.h>
#include <settings/Settings.h>

#include <cassert>
#include <cmath>

#include <atomic>
#include <iomanip>
#include <sstream>
#include <thread>
#include <tuple>
#include <vector>

#include "ratecontrol/BasicSender.h"
#include "ratecontrol/Network.h"
#include "ratecontrol/Receiver.h"
#include "ratecontrol/Relay.h"
#include "ratecontrol/RelaySender.h"
#include "ratecontrol/Sender.h"


std::string createName(const std::string& _prefix, u32 _id, u32 _total) {
  u32 digits = (u32)ceil(log10(_total));
  std::stringstream ss;
  ss << _prefix << std::setw(digits) << std::setfill('0') << _id;
  return ss.str();
}

s32 main(s32 _argc, char** _argv) {
  Json::Value settings;
  settings::Settings::commandLine(_argc, _argv, &settings);

  u32 numSenders = settings["senders"].asUInt();
  u32 numReceivers = settings["receivers"].asUInt();;
  u32 numRelays = settings["relays"].asUInt();
  des::Tick networkDelay = (des::Tick)settings["network_delay"].asUInt64();
  f64 rateLimit = settings["rate_limit"].asDouble();
  s64 numMessages = (s64)settings["messages"].asUInt();
  u32 numThreads = settings["threads"].asUInt();
  u32 verbosity = settings["verbosity"].asUInt();
  std::string algorithm = settings["algorithm"].asString();
  std::string logFile = settings["log_file"].asString();

  // verify inputs
  if (numSenders < 1) {
    fprintf(stderr, "there must be at least one sender\n");
    exit(-1);
  }
  if (numReceivers < 1) {
    fprintf(stderr, "there must be at least one receiver\n");
    exit(-1);
  }
  if (rateLimit <= 0.0) {
    fprintf(stderr, "rate limit must be greater than 0.0\n");
    exit(-1);
  }

  // create the simulation environment
  des::Simulator sim(numThreads);

  // create a logger for the simulation
  des::Logger logger(logFile);
  sim.setLogger(&logger);

  // log the configuration
  if (verbosity > 0) {
    std::string conf = settings::Settings::toString(settings);
    logger.log(conf.c_str(), conf.size());
  }

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

  // create relays
  std::vector<Relay*> relays(numRelays, nullptr);
  for (u32 r = 0; r < numRelays; r++) {
    f64 relayRateLimit = rateLimit / numRelays;
    assert(relayRateLimit <= 1.0);
    relays.at(r) = new Relay(&sim, createName("I", r, numRelays),
                             &network, nodeId++, &network, relayRateLimit);
    relays.at(r)->debug = verbosity > 1;
  }

  // create senders
  std::atomic<s64> remainingSendMessages(numMessages);
  std::vector<Sender*> senders(numSenders, nullptr);
  for (u32 s = 0; s < numSenders; s++) {
    if (algorithm == "basic") {
      senders.at(s) = new BasicSender(
          &sim, createName("S", s, numSenders), &network,
          nodeId++, &network, &remainingSendMessages, 1,
          1000, receivers.at(0)->id, receivers.at(numReceivers - 1)->id,
          1.0);

    } else if (algorithm == "relay") {
      senders.at(s) = new RelaySender(
          &sim, createName("S", s, numSenders), &network,
          nodeId++, &network, &remainingSendMessages, 1,
          1000, receivers.at(0)->id, receivers.at(numReceivers - 1)->id,
          relays.at(0)->id, relays.at(numRelays - 1)->id, 1);

    } else {
      fprintf(stderr, "invalid algorithm: %s\n", algorithm.c_str());
      exit(-1);
    }

    senders.at(s)->debug = verbosity > 1;
  }

  // run simulation
  sim.simulate(verbosity > 0);

  // cleanup
  for (u32 r = 0; r < numReceivers; r++) {
    delete receivers.at(r);
  }
  for (u32 r = 0; r < numRelays; r++) {
    delete relays.at(r);
  }
  for (u32 s = 0; s < numSenders; s++) {
    delete senders.at(s);
  }

  return 0;
}
