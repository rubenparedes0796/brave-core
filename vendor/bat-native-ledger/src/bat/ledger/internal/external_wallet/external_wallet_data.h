/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_EXTERNAL_WALLET_EXTERNAL_WALLET_DATA_H_
#define BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_EXTERNAL_WALLET_EXTERNAL_WALLET_DATA_H_

#include <iostream>
#include <string>

#include "bat/ledger/public/interfaces/ledger.mojom.h"

namespace ledger {

enum class ExternalWalletProvider { kUphold, kGemini, kBitflyer };

void ParseEnum(const std::string& s,
               absl::optional<ExternalWalletProvider>* value);

std::string StringifyEnum(ExternalWalletProvider value);

struct ExternalWallet {
  ExternalWalletProvider provider;
  std::string address;
  std::string access_token;

  static absl::optional<ExternalWallet> FromMojo(
      const mojom::ExternalWallet& wallet);
};

struct ExternalWalletTransferResult {
  ExternalWalletProvider provider;
  std::string transaction_id;
};

}  // namespace ledger

#endif  // BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_EXTERNAL_WALLET_EXTERNAL_WALLET_DATA_H_
