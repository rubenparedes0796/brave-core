/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/browser/filecoin_keyring.h"

#include <utility>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "brave/components/bls/buildflags.h"
#include "brave/components/brave_wallet/common/brave_wallet.mojom.h"
#include "brave/components/brave_wallet/common/fil_address.h"
#if BUILDFLAG(ENABLE_RUST_BLS)
#include "brave/components/bls/rs/src/lib.rs.h"
#endif

namespace brave_wallet {

FilecoinKeyring::FilecoinKeyring() = default;
FilecoinKeyring::~FilecoinKeyring() = default;

std::string FilecoinKeyring::ImportFilecoinBLSAccount(
    const std::vector<uint8_t>& private_key,
    const std::string& network) {
#if BUILDFLAG(ENABLE_RUST_BLS)
  if (private_key.empty()) {
    return std::string();
  }

  std::unique_ptr<HDKey> hd_key = HDKey::GenerateFromPrivateKey(private_key);
  if (!hd_key)
    return std::string();

  std::array<uint8_t, 32> payload;
  std::copy_n(private_key.begin(), 32, payload.begin());
  auto result = bls::fil_private_key_public_key(payload);
  std::vector<uint8_t> public_key(result.begin(), result.end());
  if (std::all_of(public_key.begin(), public_key.end(),
                  [](int i) { return i == 0; }))
    return std::string();

  FilAddress address = FilAddress::FromPublicKey(
      public_key, mojom::FilecoinAddressProtocol::BLS, network);
  if (address.IsEmpty() ||
      !AddImportedAddress(address.ToString(), std::move(hd_key))) {
    return std::string();
  }
  return address.ToString();
#else
  return std::string();
#endif
}

std::string FilecoinKeyring::ImportFilecoinSECP256K1Account(
    const std::vector<uint8_t>& input_key,
    const std::string& network) {
  if (input_key.empty()) {
    return std::string();
  }
  std::unique_ptr<HDKey> hd_key = HDKey::GenerateFromPrivateKey(input_key);
  if (!hd_key)
    return std::string();
  auto uncompressed_public_key = hd_key->GetUncompressedPublicKey();
  FilAddress address = FilAddress::FromUncompressedPublicKey(
      uncompressed_public_key, mojom::FilecoinAddressProtocol::SECP256K1,
      network);
  if (address.IsEmpty() ||
      !AddImportedAddress(address.ToString(), std::move(hd_key))) {
    return std::string();
  }
  return address.ToString();
}

void FilecoinKeyring::ImportFilecoinAccount(
    const std::vector<uint8_t>& input_key,
    const std::string& address) {
  std::unique_ptr<HDKey> hd_key = HDKey::GenerateFromPrivateKey(input_key);
  if (!hd_key)
    return;
  if (!AddImportedAddress(address, std::move(hd_key))) {
    return;
  }
}

std::string FilecoinKeyring::GetAddressInternal(HDKeyBase* hd_key_base) const {
  if (!hd_key_base)
    return std::string();
  HDKey* hd_key = static_cast<HDKey*>(hd_key_base);
  // TODO(spylogsster): Get network from settings.
  return FilAddress::FromUncompressedPublicKey(
             hd_key->GetUncompressedPublicKey(),
             mojom::FilecoinAddressProtocol::SECP256K1, mojom::kFilecoinTestnet)
      .ToString();
}

}  // namespace brave_wallet
