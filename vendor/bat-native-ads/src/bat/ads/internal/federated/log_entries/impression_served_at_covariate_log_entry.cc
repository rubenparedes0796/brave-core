/* Copyright 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/federated/log_entries/impression_served_at_covariate_log_entry.h"

#include "bat/ads/internal/federated/covariate_logs_util.h"

namespace ads {

ImpressionServedAtCovariateLogEntry::ImpressionServedAtCovariateLogEntry() =
    default;

ImpressionServedAtCovariateLogEntry::~ImpressionServedAtCovariateLogEntry() =
    default;

void ImpressionServedAtCovariateLogEntry::SetLastImpressionAt(
    const base::Time& time) {
  impression_served_at_ = time;
}

mojom::DataType ImpressionServedAtCovariateLogEntry::GetDataType() const {
  return mojom::DataType::kDouble;
}

mojom::CovariateType ImpressionServedAtCovariateLogEntry::GetCovariateType()
    const {
  return mojom::CovariateType::kAdNotificationImpressionServedAt;
}

std::string ImpressionServedAtCovariateLogEntry::GetValue() const {
  return ToString(impression_served_at_);
}

}  // namespace ads
