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
#include <settings/settings.h>

#include <cassert>
#include <cmath>

#include <atomic>
#include <iomanip>
#include <sstream>
#include <thread>
#include <tuple>
#include <vector>

#include "ratecontrol/BasicSender.h"
#include "ratecontrol/DistSender.h"
#include "ratecontrol/Network.h"
#include "ratecontrol/Receiver.h"
#include "ratecontrol/Relay.h"
#include "ratecontrol/RelaySender.h"
#include "ratecontrol/Sender.h"
#include "ratecontrol/SenderControl.h"

std::string createName(const std::string& _prefix, u32 _id, u32 _total);

s32 main(s32 _argc, char** _argv) {
  Json::Value settings;
  settings::commandLine(_argc, _argv, &settings);

  u32 numSenders = settings["senders"].asUInt();
  u32 numReceivers = settings["receivers"].asUInt();;
  u32 numRelays = settings["relays"].asUInt();
  des::Tick networkDelay = (des::Tick)settings["network_delay"].asUInt64();
  std::string queuing = settings["queuing"].asString();
  f64 rateLimit = settings["rate_limit"].asDouble();
  u32 minMessageSize = settings["min_message_size"].asUInt();
  u32 maxMessageSize = settings["max_message_size"].asUInt();
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
  if (minMessageSize == 0) {
    fprintf(stderr, "minimum message size must be greater than 0\n");
    exit(-1);
  }
  if (maxMessageSize < minMessageSize) {
    fprintf(stderr, "maximum message size must be greater than or equal to"
            " the minimum message size\n");
    exit(-1);
  }

  // create the simulation environment
  des::Simulator sim(numThreads);

  // create a logger for the simulation
  des::Logger logger(logFile);
  sim.setLogger(&logger);

  // log the configuration
  if (verbosity > 0) {
    std::string conf = settings::toString(settings);
    logger.log(conf.c_str(), conf.size());
  }

  // create a Network
  Network network(&sim, "Network", nullptr, networkDelay);
  network.debug = verbosity > 1;

  // create receivers
  u32 nodeId = 0;
  std::vector<Receiver*> receivers(numReceivers, nullptr);
  for (u32 r = 0; r < numReceivers; r++) {
    receivers.at(r) = new Receiver(
        &sim, createName("Receiver", r, numReceivers), nullptr, nodeId++,
        queuing, &network);
    receivers.at(r)->debug = verbosity > 1;
  }

  // create relays
  std::vector<Relay*> relays(numRelays, nullptr);
  for (u32 r = 0; r < numRelays; r++) {
    f64 relayRateLimit = rateLimit / numRelays;
    assert(relayRateLimit <= 1.0);
    relays.at(r) = new Relay(&sim, createName("Relay", r, numRelays),
                             nullptr, nodeId++, queuing, &network,
                             relayRateLimit);
    relays.at(r)->debug = verbosity > 1;
  }

  // create senders
  std::vector<Sender*> senders(numSenders, nullptr);
  for (u32 s = 0; s < numSenders; s++) {
    if (algorithm == "basic") {
      senders.at(s) = new BasicSender(
          &sim, createName("Sender", s, numSenders), nullptr,
          nodeId++, queuing, &network, minMessageSize,
          maxMessageSize, receivers.at(0)->id,
          receivers.at(numReceivers - 1)->id,
          settings["sender_config"]);
    } else if (algorithm == "relay") {
      senders.at(s) = new RelaySender(
          &sim, createName("Sender", s, numSenders), nullptr,
          nodeId++, queuing, &network, minMessageSize,
          maxMessageSize, receivers.at(0)->id,
          receivers.at(numReceivers - 1)->id,
          settings["sender_config"]);
    } else if (algorithm == "dist") {
      senders.at(s) = new DistSender(
          &sim, createName("Sender", s, numSenders), nullptr,
          nodeId++, queuing, &network, minMessageSize,
          maxMessageSize, receivers.at(0)->id,
          receivers.at(numReceivers - 1)->id,
          rateLimit, settings["sender_config"]);
    } else {
      fprintf(stderr, "invalid algorithm: %s\n", algorithm.c_str());
      exit(-1);
    }
    senders.at(s)->debug = verbosity > 1;
  }

  // inform senders of any IDs they need
  for (u32 s = 0; s < numSenders; s++) {
    if (algorithm == "relay") {
      reinterpret_cast<RelaySender*>(senders.at(s))->relayIds(
          relays.at(0)->id, relays.at(numRelays - 1)->id);
    } else if (algorithm == "dist") {
      reinterpret_cast<DistSender*>(senders.at(s))->distIds(
          senders.at(0)->id, senders.at(numSenders - 1)->id);
    }
  }

  // create a sender control unit for controlling desired injection rate
  SenderControl senderControl(&sim, "SenderControl", nullptr, &senders,
                              settings["sender_control"]);
  senderControl.debug = verbosity > 0;

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

std::string createName(const std::string& _prefix, u32 _id, u32 _total) {
  u32 digits = (u32)ceil(log10(_total));
  std::stringstream ss;
  ss << _prefix << '_' << std::setw(digits) << std::setfill('0') << _id;
  return ss.str();
}
