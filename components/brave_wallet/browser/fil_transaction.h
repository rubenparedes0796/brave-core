/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_FIL_TRANSACTION_H_
#define BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_FIL_TRANSACTION_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "brave/components/brave_wallet/common/brave_wallet.mojom.h"
#include "brave/components/brave_wallet/common/brave_wallet_types.h"
#include "brave/components/brave_wallet/common/fil_address.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Value;
}  // namespace base

namespace brave_wallet {

class FilTransaction {
 public:
  FilTransaction();
  FilTransaction(const FilTransaction&);
  virtual ~FilTransaction();
  bool operator==(const FilTransaction&) const;

  static absl::optional<FilTransaction> FromTxData(
      const mojom::FilTxDataPtr& tx_data);

  absl::optional<uint256_t> nonce() const { return nonce_; }
  uint256_t gas_price() const { return gas_price_; }
  uint256_t gas_limit() const { return gas_limit_; }
  FilAddress to() const { return to_; }
  uint256_t value() const { return value_; }
  std::vector<uint8_t> data() const { return data_; }
  uint256_t v() const { return v_; }
  std::vector<uint8_t> r() const { return r_; }
  std::vector<uint8_t> s() const { return s_; }

  void set_to(FilAddress to) { to_ = to; }
  void set_value(uint256_t value) { value_ = value; }
  void set_nonce(absl::optional<uint256_t> nonce) { nonce_ = nonce; }
  void set_gas_price(uint256_t gas_price) { gas_price_ = gas_price; }
  void set_gas_limit(uint256_t gas_limit) { gas_limit_ = gas_limit; }
  void set_data(const std::vector<uint8_t>& data) { data_ = data; }

  virtual base::Value ToValue() const;
  static absl::optional<FilTransaction> FromValue(const base::Value& value);

 protected:
  absl::optional<uint256_t> nonce_;
  uint256_t gas_price_;
  uint256_t gas_limit_;
  FilAddress to_;
  uint256_t value_;
  std::vector<uint8_t> data_;

  uint256_t v_ = 0;
  std::vector<uint8_t> r_;
  std::vector<uint8_t> s_;

 protected:
  FilTransaction(absl::optional<uint256_t> nonce,
                 uint256_t gas_price,
                 uint256_t gas_limit,
                 const FilAddress& to,
                 uint256_t value,
                 const std::vector<uint8_t>& data);
};

}  // namespace brave_wallet
#endif  // BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_FIL_TRANSACTION_H_
