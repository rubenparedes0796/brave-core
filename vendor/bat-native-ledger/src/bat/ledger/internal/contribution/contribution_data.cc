/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/contribution/contribution_data.h"

#include <iostream>
#include <string>

#include "base/guid.h"

namespace ledger {

void ParseEnum(const std::string& s, absl::optional<ContributionType>* value) {
  DCHECK(value);
  if (s == "one-time") {
    *value = ContributionType::kOneTime;
  } else if (s == "recurring") {
    *value = ContributionType::kRecurring;
  } else if (s == "auto-contribute") {
    *value = ContributionType::kAutoContribute;
  } else {
    *value = {};
  }
}

std::string StringifyEnum(ContributionType value) {
  switch (value) {
    case ContributionType::kOneTime:
      return "one-time";
    case ContributionType::kRecurring:
      return "recurring";
    case ContributionType::kAutoContribute:
      return "auto-contribute";
  }
}

void ParseEnum(const std::string& s,
               absl::optional<ContributionSource>* value) {
  DCHECK(value);
  if (s == "brave-vg") {
    *value = ContributionSource::kBraveVG;
  } else if (s == "brave-sku") {
    *value = ContributionSource::kBraveSKU;
  } else if (s == "external") {
    *value = ContributionSource::kExternal;
  } else {
    *value = {};
  }
}

std::string StringifyEnum(ContributionSource value) {
  switch (value) {
    case ContributionSource::kBraveVG:
      return "brave-vg";
    case ContributionSource::kBraveSKU:
      return "brave-sku";
    case ContributionSource::kExternal:
      return "external";
  }
}

ContributionRequest::ContributionRequest() = default;

ContributionRequest::ContributionRequest(ContributionType contribution_type,
                                         const std::string& publisher_id,
                                         ContributionSource source,
                                         double amount)
    : id(base::GUID::GenerateRandomV4().AsLowercaseString()),
      type(contribution_type),
      publisher_id(publisher_id),
      amount(amount),
      source(source) {}

ContributionRequest::~ContributionRequest() = default;

ContributionRequest::ContributionRequest(const ContributionRequest& other) =
    default;

ContributionRequest& ContributionRequest::operator=(
    const ContributionRequest& other) = default;

ContributionRequest::ContributionRequest(ContributionRequest&& other) = default;

ContributionRequest& ContributionRequest::operator=(
    ContributionRequest&& other) = default;

}  // namespace ledger
