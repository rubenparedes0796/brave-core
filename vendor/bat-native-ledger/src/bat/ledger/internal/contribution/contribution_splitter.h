/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CONTRIBUTION_CONTRIBUTION_SPLITTER_H_
#define BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CONTRIBUTION_CONTRIBUTION_SPLITTER_H_

#include <string>
#include <vector>

#include "bat/ledger/internal/contribution/contribution_data.h"
#include "bat/ledger/internal/core/bat_ledger_context.h"
#include "bat/ledger/internal/core/future.h"

namespace ledger {

// TODO(zenparsing): This is no longer necessary.
class ContributionSplitter : public BATLedgerContext::Object {
 public:
  static const char kContextKey[];

  struct SourceAmount {
    ContributionSource source;
    double amount;
  };

  enum class Error {
    kNone,
    kPublisherNotFound,
    kInsufficientFunds,
    kPublisherNotConfigured
  };

  struct Split {
    Split();
    ~Split();

    explicit Split(Error error);

    Split(const Split& other);
    Split& operator=(const Split& other);

    Split(Split&& other);
    Split& operator=(Split&& other);

    Error error = Error::kNone;
    std::vector<SourceAmount> amounts;
  };

  Future<Split> SplitContribution(const std::string& publisher_id,
                                  double amount);
};

}  // namespace ledger

#endif  // BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CONTRIBUTION_CONTRIBUTION_SPLITTER_H_
