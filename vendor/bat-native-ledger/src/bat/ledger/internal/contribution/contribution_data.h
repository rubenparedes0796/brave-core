/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CONTRIBUTION_CONTRIBUTION_DATA_H_
#define BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CONTRIBUTION_CONTRIBUTION_DATA_H_

#include <string>

#include "base/time/time.h"
#include "base/values.h"

namespace ledger {

enum class ContributionType { kOneTime, kRecurring, kAutoContribute };

void ParseEnum(const std::string& s, absl::optional<ContributionType>* value);

std::string StringifyEnum(ContributionType value);

// TODO(zenparsing): Do we need the "Brave" prefix?
enum class ContributionSource { kBraveVG, kBraveSKU, kExternal };

void ParseEnum(const std::string& s, absl::optional<ContributionSource>* value);

std::string StringifyEnum(ContributionSource value);

// TODO(zenparsing): This should just be "Contribution" I suppose. Do we even
// really need this structure?
struct ContributionRequest {
  ContributionRequest();

  ContributionRequest(ContributionType contribution_type,
                      const std::string& publisher_id,
                      ContributionSource source,
                      double amount);

  ~ContributionRequest();

  ContributionRequest(const ContributionRequest& other);
  ContributionRequest& operator=(const ContributionRequest& other);

  ContributionRequest(ContributionRequest&& other);
  ContributionRequest& operator=(ContributionRequest&& other);

  std::string id;
  ContributionType type;
  std::string publisher_id;
  double amount;
  ContributionSource source;
};

enum class ContributionTokenType { kVG, kSKU };

struct ContributionToken {
  int64_t id;
  double value;
  std::string unblinded_token;
  std::string public_key;
};

struct PublisherActivity {
  std::string publisher_id;
  int64_t visits;
  base::TimeDelta duration;
};

struct RecurringContribution {
  std::string publisher_id;
  double amount;
};

}  // namespace ledger

#endif  // BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CONTRIBUTION_CONTRIBUTION_DATA_H_
