/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CONTRIBUTION_CONTRIBUTION_DATA_H_
#define BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CONTRIBUTION_CONTRIBUTION_DATA_H_

#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "bat/ledger/internal/core/enum_string.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ledger {

enum class ContributionType { kOneTime, kRecurring, kAutoContribute };

std::string StringifyEnum(ContributionType value);

absl::optional<ContributionType> ParseEnum(
    const EnumString<ContributionType>& s);

enum class ContributionSource { kBraveVG, kBraveSKU, kExternal };

std::string StringifyEnum(ContributionSource value);

absl::optional<ContributionSource> ParseEnum(
    const EnumString<ContributionSource>& s);

struct Contribution {
  Contribution();

  Contribution(ContributionType contribution_type,
               const std::string& publisher_id,
               ContributionSource source,
               double amount);

  ~Contribution();

  Contribution(const Contribution& other);
  Contribution& operator=(const Contribution& other);

  Contribution(Contribution&& other);
  Contribution& operator=(Contribution&& other);

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

  base::Value ToValue() const;
  static absl::optional<PublisherActivity> FromValue(const base::Value& value);
};

struct RecurringContribution {
  std::string publisher_id;
  double amount;
};

struct PendingContribution {
  int64_t id;
  std::string publisher_id;
  double amount;
};

}  // namespace ledger

#endif  // BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CONTRIBUTION_CONTRIBUTION_DATA_H_
