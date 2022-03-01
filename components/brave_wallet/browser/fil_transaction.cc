/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/browser/fil_transaction.h"
#include <stdint.h>
#include <charconv>
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

namespace {

std::string Uint256ValueToString(uint256_t value) {
  // 80 as max length of uint256 string
  std::array<char, 80> buffer{0};
  auto [p, ec] =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  if (ec != std::errc())
    return std::string();
  std::string response;
  response.assign(buffer.begin(), p);
  return response;
}

}  // namespace

FilTransaction::FilTransaction() : gas_limit_(0) {}

FilTransaction::FilTransaction(const FilTransaction&) = default;
FilTransaction::FilTransaction(absl::optional<uint64_t> nonce,
                               const std::string& gas_premium,
                               const std::string& gas_fee_cap,
                               uint64_t gas_limit,
                               const FilAddress& to,
                               const std::string& value)
    : nonce_(nonce),
      gas_premium_(gas_premium),
      gas_fee_cap_(gas_fee_cap),
      gas_limit_(gas_limit),
      to_(to),
      value_(value) {}
FilTransaction::~FilTransaction() = default;

bool FilTransaction::operator==(const FilTransaction& tx) const {
  return nonce_ == tx.nonce_ && gas_premium_ == tx.gas_premium_ &&
         gas_fee_cap_ == tx.gas_fee_cap_ && gas_limit_ == tx.gas_limit_ &&
         to_ == tx.to_ && value_ == tx.value_ && v_ == tx.v_ &&
         std::equal(r_.begin(), r_.end(), tx.r_.begin()) &&
         std::equal(s_.begin(), s_.end(), tx.s_.begin());
}

// static
absl::optional<FilTransaction> FilTransaction::FromTxData(
    const mojom::FilTxDataPtr& tx_data) {
  FilTransaction tx;
  uint64_t nonce = 0;
  if (!tx_data->nonce.empty() && base::StringToUint64(tx_data->nonce, &nonce)) {
    tx.nonce_ = nonce;
  }

  tx.to_ = FilAddress::FromString(tx_data->to);
  uint256_t value = 0;
  if (!HexValueToUint256(tx_data->value, &value))
    return absl::nullopt;
  tx.set_value(Uint256ValueToString(value));
  return tx;
}

base::Value FilTransaction::ToValue() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("nonce",
                    nonce_ ? base::NumberToString(nonce_.value()) : "");
  dict.SetStringKey("gas_premium", gas_premium_);
  dict.SetStringKey("gas_fee_cap", gas_fee_cap_);
  dict.SetStringKey("gas_limit", base::NumberToString(gas_limit_));
  dict.SetStringKey("to", to_.ToString());
  dict.SetStringKey("value", value_);

  return dict;
}

bool FilTransaction::IsSigned() const {
  return v_ != (uint256_t)0 && r_.size() != 0 && s_.size() != 0;
}

std::string FilTransaction::GetSignedTransaction() const {
  DCHECK(nonce_);
  base::ListValue list;
  /*  list.Append(RLPUint256ToBlobValue(nonce_.value()));
    list.Append(RLPUint256ToBlobValue(gas_premium_));
    list.Append(RLPUint256ToBlobValue(gas_fee_cap_));
    list.Append(RLPUint256ToBlobValue(gas_limit_));
    */
  list.Append(base::Value(to_.bytes()));
  // list.Append(RLPUint256ToBlobValue(value_));
  // list.Append(RLPUint256ToBlobValue(v_));
  list.Append(base::Value(r_));
  list.Append(base::Value(s_));

  return ToHex(RLPEncode(std::move(list)));
}

// static
absl::optional<FilTransaction> FilTransaction::FromValue(
    const base::Value& value) {
  FilTransaction tx;
  const std::string* nonce = value.FindStringKey("nonce");
  if (!nonce)
    return absl::nullopt;

  if (!nonce->empty()) {
    uint64_t nonce_uint;
    if (!base::StringToUint64(*nonce, &nonce_uint))
      return absl::nullopt;
    tx.nonce_ = nonce_uint;
  }

  const std::string* gas_premium = value.FindStringKey("gas_premium");
  if (!gas_premium)
    return absl::nullopt;
  tx.gas_premium_ = *gas_premium;

  const std::string* gas_fee_cap = value.FindStringKey("gas_fee_cap");
  if (!gas_fee_cap)
    return absl::nullopt;
  tx.gas_fee_cap_ = *gas_fee_cap;

  const std::string* gas_limit = value.FindStringKey("gas_limit");
  if (!gas_limit || !base::StringToUint64(*gas_limit, &tx.gas_limit_))
    return absl::nullopt;

  const std::string* to = value.FindStringKey("to");
  if (!to)
    return absl::nullopt;
  tx.to_ = FilAddress::FromString(*to);

  const std::string* tx_value = value.FindStringKey("value");
  if (!tx_value)
    return absl::nullopt;
  tx.value_ = *tx_value;

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
