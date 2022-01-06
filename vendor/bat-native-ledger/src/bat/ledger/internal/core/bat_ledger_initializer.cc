/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/core/bat_ledger_initializer.h"

#include <utility>

#include "bat/ledger/internal/contribution/auto_contribute_processor.h"
#include "bat/ledger/internal/contribution/contribution_fee_processor.h"
#include "bat/ledger/internal/contribution/contribution_scheduler.h"
#include "bat/ledger/internal/core/bat_ledger_job.h"
#include "bat/ledger/internal/core/job_store.h"
#include "bat/ledger/internal/core/upgrade_manager.h"

namespace ledger {

namespace {

template <typename... Ts>
class InitializeJob : public BATLedgerJob<bool> {
 public:
  void Start() { StartNext<Ts..., End>(); }

 private:
  struct End {};

  template <typename T, typename... Rest>
  void StartNext() {
    context().LogVerbose(FROM_HERE) << "Initializing " << T::kContextKey;
    context().template Get<T>().Initialize().Then(
        ContinueWith(this, &InitializeJob::OnCompleted<T, Rest...>));
  }

  template <>
  void StartNext<End>() {
    context().LogVerbose(FROM_HERE) << "Initialization complete";
    Complete(true);
  }

  template <typename T, typename... Rest>
  void OnCompleted(bool success) {
    if (!success) {
      context().LogError(FROM_HERE) << "Error initializing " << T::kContextKey;
      return Complete(false);
    }
    StartNext<Rest...>();
  }
};

using InitializeAllJob = InitializeJob<UpgradeManager,
                                       JobStore,
                                       AutoContributeProcessor,
                                       ContributionFeeProcessor,
                                       ContributionScheduler>;

}  // namespace

const char BATLedgerInitializer::kContextKey[] = "bat-ledger-initializer";

BATLedgerInitializer::BATLedgerInitializer() = default;
BATLedgerInitializer::~BATLedgerInitializer() = default;

Future<bool> BATLedgerInitializer::Initialize() {
  return initialize_cache_.GetFuture([&]() {
    return context().StartJob<InitializeAllJob>().Map(
        base::BindOnce([](bool success) {
          return std::make_pair(success, base::TimeDelta::Max());
        }));
  });
}

}  // namespace ledger
