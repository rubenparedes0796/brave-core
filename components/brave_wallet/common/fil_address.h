/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_WALLET_COMMON_FIL_ADDRESS_H_
#define BRAVE_COMPONENTS_BRAVE_WALLET_COMMON_FIL_ADDRESS_H_

#include <string>
#include <vector>

#include "brave/components/brave_wallet/common/brave_wallet_types.h"

namespace brave_wallet {

class FilAddress {
 public:
  static FilAddress FromPublicKey(const std::vector<uint8_t>& public_key);
  static FilAddress FromHex(const std::string& input);
  static FilAddress From(const std::string& input);
  static bool IsValidAddress(const std::string& input);
  FilAddress();
  FilAddress(const FilAddress& other);
  ~FilAddress();
  bool operator==(const FilAddress& other) const;
  bool operator!=(const FilAddress& other) const;

  bool IsEmpty() const { return bytes_.empty(); }
  std::vector<uint8_t> bytes() const { return bytes_; }

  std::string ToHex() const;

 private:
  explicit FilAddress(const std::vector<uint8_t>& bytes);

  std::vector<uint8_t> bytes_;
};

}  // namespace brave_wallet

#endif  // BRAVE_COMPONENTS_BRAVE_WALLET_COMMON_FIL_ADDRESS_H_
