/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/contribution/contribution_engine.h"

#include <string>

#include "bat/ledger/internal/contribution/contribution_data.h"
#include "bat/ledger/internal/contribution/contribution_router.h"
#include "bat/ledger/internal/contribution/contribution_store.h"
#include "bat/ledger/internal/contribution/external_contribution_processor.h"
#include "bat/ledger/internal/contribution/token_contribution_processor.h"
#include "bat/ledger/internal/external_wallet/external_wallet_manager.h"

namespace ledger {

const char ContributionEngine::kContextKey[] = "contribution-engine";

Future<bool> ContributionEngine::SendOneTimeContribution(
    const std::string& publisher_id,
    double amount) {
  return context().Get<ContributionRouter>().SendContribution(
      ContributionType::kOneTime, publisher_id, amount);
}

Future<bool> ContributionEngine::SavePendingContribution(
    const std::string& publisher_id,
    double amount) {
  // TODO(zenparsing): Implement this. The front-end will call this directly
  // instead of relying on the contribution engine to figure it out.
  return Future<bool>::Completed(false);
}

Future<bool> ContributionEngine::AddPublisherVisit(
    const std::string& publisher_id,
    base::TimeDelta duration) {
  return context().Get<ContributionStore>().AddPublisherVisit(publisher_id,
                                                              duration);
}

}  // namespace ledger
