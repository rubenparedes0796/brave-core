/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/core/delay_generator.h"

#include <algorithm>

#include "base/threading/sequenced_task_runner_handle.h"
#include "bat/ledger/internal/core/bat_ledger_job.h"
#include "bat/ledger/internal/core/randomizer.h"

namespace ledger {

namespace {

class DelayJob : public BATLedgerJob<bool> {
 public:
  void Start(base::TimeDelta delay) {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&DelayJob::Callback, base::AsWeakPtr(this)),
        delay);
  }

 private:
  void Callback() { Complete(true); }
};

}  // namespace

const char DelayGenerator::kContextKey[] = "delay-generator";

Future<bool> DelayGenerator::Delay(base::Location location,
                                   base::TimeDelta delay) {
  context().LogInfo(location) << "Delay set for " << delay;
  return context().StartJob<DelayJob>(delay);
}

Future<bool> DelayGenerator::RandomDelay(base::Location location,
                                         base::TimeDelta delay) {
  uint64_t seconds = context().Get<Randomizer>().Geometric(delay.InSecondsF());
  return Delay(location, base::Seconds(static_cast<int64_t>(seconds)));
}

BackoffDelay::BackoffDelay(base::TimeDelta min, base::TimeDelta max)
    : min_(min), max_(max) {}

base::TimeDelta BackoffDelay::GetNextDelay() {
  auto factor = 1 << std::min(backoff_count_, 24);
  backoff_count_ += 1;
  return std::min(min_ * factor, max_);
}

void BackoffDelay::Reset() {
  backoff_count_ = 0;
}

}  // namespace ledger
