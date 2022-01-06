/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CONTRIBUTION_CONTRIBUTION_ENGINE_H_
#define BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CONTRIBUTION_CONTRIBUTION_ENGINE_H_

#include <string>

#include "base/time/time.h"
#include "bat/ledger/internal/contribution/contribution_data.h"
#include "bat/ledger/internal/core/bat_ledger_context.h"
#include "bat/ledger/internal/core/future.h"

namespace ledger {

class ContributionEngine : public BATLedgerContext::Object {
 public:
  static const char kContextKey[];

  Future<bool> SendOneTimeContribution(const std::string& publisher_id,
                                       double amount);

  Future<bool> SetRecurringContribution(const std::string& publisher_id,
                                        double amount);

  Future<bool> DeleteRecurringContribution(const std::string& publisher_id);

  Future<bool> SavePendingContribution(const std::string& publisher_id,
                                       double amount);

  // TODO(zenparsing): We'll need query methods for pending contributions as
  // well, unless we have a separate query engine. At that point we'll need to
  // figure out the Mojo problem.

  Future<bool> SendPendingContributions();

  Future<bool> AddPublisherVisit(const std::string& publisher_id,
                                 base::TimeDelta duration);
};

}  // namespace ledger

#endif  // BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CONTRIBUTION_CONTRIBUTION_ENGINE_H_
