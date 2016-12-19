////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_STATISTICS_H
#define ARANGODB_PREGEL_STATISTICS_H 1

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include "Pregel/Utils.h"

namespace arangodb {
namespace pregel {

struct WorkerStats {
  size_t activeCount = 0;
  size_t sendCount = 0;
  size_t receivedCount = 0;
  double superstepRuntimeSecs = 0;

  WorkerStats() {}
  WorkerStats(VPackSlice statValues) {
    accumulate(statValues);
  }
  WorkerStats(size_t a, size_t s, size_t r)
      : activeCount(a), sendCount(s), receivedCount(r) {}

  void accumulate(WorkerStats const& other) {
    activeCount += other.activeCount;
    sendCount += other.sendCount;
    receivedCount += other.receivedCount;
    superstepRuntimeSecs += other.superstepRuntimeSecs;
  }

  void accumulate(VPackSlice statValues) {
    VPackSlice p = statValues.get(Utils::activeCountKey);
    if (p.isInteger()) {
      activeCount += p.getUInt();
    }
    p = statValues.get(Utils::sendCountKey);
    if (p.isInteger()) {
      sendCount += p.getUInt();
    }
    p = statValues.get(Utils::receivedCountKey);
    if (p.isInteger()) {
      receivedCount += p.getUInt();
    }
    p = statValues.get(Utils::superstepRuntimeKey);
    if (p.isNumber()) {
      superstepRuntimeSecs += p.getNumber<double>();
    }
  }

  void serializeValues(VPackBuilder& b) const {
    b.add(Utils::activeCountKey, VPackValue(activeCount));
    b.add(Utils::sendCountKey, VPackValue(sendCount));
    b.add(Utils::receivedCountKey, VPackValue(receivedCount));
    b.add(Utils::superstepRuntimeKey, VPackValue(superstepRuntimeSecs));
  }

  void reset() {
    activeCount = 0;
    sendCount = 0;
    receivedCount = 0;
    superstepRuntimeSecs = 0;
  }
  
  bool allMessagesProcessed() {
    return sendCount == receivedCount;
  }
  
  bool isDone() {
    return activeCount == 0 && sendCount == receivedCount;
  }
};

struct StatsManager {

  void accumulate(VPackSlice data) {
    VPackSlice sender = data.get(Utils::senderKey);
    if (sender.isString()) {
      _serverStats[sender.copyString()].accumulate(data);
    }
  }
  
  void serializeValues(VPackBuilder &b) {
    WorkerStats stats;
    for (auto const& pair : _serverStats) {
      stats.accumulate(pair.second);
    }
    stats.serializeValues(b);
  }

  bool allMessagesProcessed() {
    uint64_t send = 0, received = 0;
    for (auto const& pair : _serverStats) {
      send += pair.second.sendCount;
      received += pair.second.receivedCount;
    }
    return send == received;
  }

  /// tests for convergence
  bool executionFinished() {
    uint64_t send = 0, received = 0;
    for (auto const& pair : _serverStats) {
      send += pair.second.sendCount;
      received += pair.second.receivedCount;
      if (pair.second.activeCount > 0) {
        return false;
      }
    }
    return send == received;
  }

  void resetActiveCount() {
    for (auto &pair : _serverStats) {
      pair.second.activeCount = 0;
    }
  }

  void reset() {
    _serverStats.clear();
  }
  
  size_t clientCount() {
    return _serverStats.size();
  }

  private:
  std::map<std::string, WorkerStats> _serverStats;
};
  
}
}
#endif