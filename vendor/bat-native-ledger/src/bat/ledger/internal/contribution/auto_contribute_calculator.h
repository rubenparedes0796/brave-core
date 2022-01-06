/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CONTRIBUTION_AUTO_CONTRIBUTE_CALCULATOR_H_
#define BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CONTRIBUTION_AUTO_CONTRIBUTE_CALCULATOR_H_

#include <map>
#include <string>
#include <vector>

#include "bat/ledger/internal/contribution/contribution_data.h"
#include "bat/ledger/internal/core/bat_ledger_context.h"

namespace ledger {

class AutoContributeCalculator : public BATLedgerContext::Object {
 public:
  static const char kContextKey[];

  // TODO(zenparsing): base::flat_map instead?
  std::map<std::string, double> CalculateWeights(
      const std::vector<PublisherActivity>& publishers,
      int min_visits,
      base::TimeDelta min_duration);

  std::map<std::string, size_t> AllocateVotes(
      const std::map<std::string, double>& publisher_weights,
      size_t total_votes);
};

}  // namespace ledger

#endif  // BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CONTRIBUTION_AUTO_CONTRIBUTE_CALCULATOR_H_
