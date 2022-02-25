/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/browser/fil_transaction.h"

#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "brave/components/brave_wallet/browser/rlp_encode.h"
#include "brave/components/brave_wallet/common/fil_address.h"
#include "brave/components/brave_wallet/common/hash_utils.h"
#include "brave/components/brave_wallet/common/hex_utils.h"

namespace brave_wallet {

FilTransaction::FilTransaction() : gas_price_(0), gas_limit_(0), value_(0) {}

FilTransaction::FilTransaction(const FilTransaction&) = default;
FilTransaction::FilTransaction(absl::optional<uint256_t> nonce,
                               uint256_t gas_price,
                               uint256_t gas_limit,
                               const FilAddress& to,
                               uint256_t value,
                               const std::vector<uint8_t>& data)
    : nonce_(nonce),
      gas_price_(gas_price),
      gas_limit_(gas_limit),
      to_(to),
      value_(value),
      data_(data) {}
FilTransaction::~FilTransaction() = default;

bool FilTransaction::operator==(const FilTransaction& tx) const {
  return nonce_ == tx.nonce_ && gas_price_ == tx.gas_price_ &&
         gas_limit_ == tx.gas_limit_ && to_ == tx.to_ && value_ == tx.value_ &&
         std::equal(data_.begin(), data_.end(), tx.data_.begin()) &&
         v_ == tx.v_ && std::equal(r_.begin(), r_.end(), tx.r_.begin()) &&
         std::equal(s_.begin(), s_.end(), tx.s_.begin());
}

// static
absl::optional<FilTransaction> FilTransaction::FromTxData(
    const mojom::FilTxDataPtr& tx_data) {
  FilTransaction tx;
  if (!tx_data->base_data->nonce.empty()) {
    uint256_t nonce_uint;
    if (HexValueToUint256(tx_data->base_data->nonce, &nonce_uint)) {
      tx.nonce_ = nonce_uint;
    }
  }

  tx.to_ = FilAddress::FromString(tx_data->base_data->to);
  if (!HexValueToUint256(tx_data->base_data->value, &tx.value_))
    return absl::nullopt;
  tx.data_ = tx_data->base_data->data;
  return tx;
}

base::Value FilTransaction::ToValue() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("nonce", nonce_ ? Uint256ValueToHex(nonce_.value()) : "");
  dict.SetStringKey("gas_price", Uint256ValueToHex(gas_price_));
  dict.SetStringKey("gas_limit", Uint256ValueToHex(gas_limit_));
  dict.SetStringKey("to", to_.ToString());
  dict.SetStringKey("value", Uint256ValueToHex(value_));

  return dict;
}

// static
absl::optional<FilTransaction> FilTransaction::FromValue(
    const base::Value& value) {
  FilTransaction tx;
  const std::string* nonce = value.FindStringKey("nonce");
  if (!nonce)
    return absl::nullopt;

  if (!nonce->empty()) {
    uint256_t nonce_uint;
    if (!HexValueToUint256(*nonce, &nonce_uint))
      return absl::nullopt;
    tx.nonce_ = nonce_uint;
  }

  const std::string* gas_price = value.FindStringKey("gas_price");
  if (!gas_price)
    return absl::nullopt;
  if (!HexValueToUint256(*gas_price, &tx.gas_price_))
    return absl::nullopt;

  const std::string* gas_limit = value.FindStringKey("gas_limit");
  if (!gas_limit)
    return absl::nullopt;
  if (!HexValueToUint256(*gas_limit, &tx.gas_limit_))
    return absl::nullopt;

  const std::string* to = value.FindStringKey("to");
  if (!to)
    return absl::nullopt;
  tx.to_ = FilAddress::FromString(*to);

  const std::string* tx_value = value.FindStringKey("value");
  if (!tx_value)
    return absl::nullopt;
  if (!HexValueToUint256(*tx_value, &tx.value_))
    return absl::nullopt;

  const std::string* data = value.FindStringKey("data");
  if (!data)
    return absl::nullopt;
  std::string data_decoded;
  if (!base::Base64Decode(*data, &data_decoded))
    return absl::nullopt;
  tx.data_ = std::vector<uint8_t>(data_decoded.begin(), data_decoded.end());

  absl::optional<int> v = value.FindIntKey("v");
  if (!v)
    return absl::nullopt;
  tx.v_ = (uint8_t)*v;

  const std::string* r = value.FindStringKey("r");
  if (!r)
    return absl::nullopt;
  std::string r_decoded;
  if (!base::Base64Decode(*r, &r_decoded))
    return absl::nullopt;
  tx.r_ = std::vector<uint8_t>(r_decoded.begin(), r_decoded.end());

  const std::string* s = value.FindStringKey("s");
  if (!s)
    return absl::nullopt;
  std::string s_decoded;
  if (!base::Base64Decode(*s, &s_decoded))
    return absl::nullopt;
  tx.s_ = std::vector<uint8_t>(s_decoded.begin(), s_decoded.end());

  return tx;
}

}  // namespace brave_wallet
