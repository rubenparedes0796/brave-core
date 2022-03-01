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

  absl::optional<uint64_t> nonce() const { return nonce_; }
  std::string gas_premium() const { return gas_premium_; }
  std::string gas_fee_cap() const { return gas_fee_cap_; }
  uint64_t gas_limit() const { return gas_limit_; }
  FilAddress to() const { return to_; }
  std::string value() const { return value_; }
  uint256_t v() const { return v_; }
  std::vector<uint8_t> r() const { return r_; }
  std::vector<uint8_t> s() const { return s_; }

  void set_to(FilAddress to) { to_ = to; }
  void set_value(std::string value) { value_ = value; }
  void set_nonce(absl::optional<uint64_t> nonce) { nonce_ = nonce; }
  void set_gas_premium(std::string gas_premium) { gas_premium_ = gas_premium; }
  void set_fee_cap(std::string gas_fee_cap) { gas_fee_cap_ = gas_fee_cap; }
  void set_gas_limit(uint64_t gas_limit) { gas_limit_ = gas_limit; }

  std::string GetSignedTransaction() const;
  bool IsSigned() const;

  virtual base::Value ToValue() const;
  static absl::optional<FilTransaction> FromValue(const base::Value& value);

 protected:
  absl::optional<uint64_t> nonce_;
  std::string gas_premium_;
  std::string gas_fee_cap_;
  uint64_t gas_limit_;
  FilAddress to_;
  std::string value_;

  uint256_t v_ = 0;
  std::vector<uint8_t> r_;
  std::vector<uint8_t> s_;

 protected:
  FilTransaction(absl::optional<uint64_t> nonce,
                 const std::string& gas_premium,
                 const std::string& gas_fee_cap,
                 uint64_t gas_limit,
                 const FilAddress& to,
                 const std::string& value);
};

}  // namespace brave_wallet
#endif  // BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_FIL_TRANSACTION_H_
