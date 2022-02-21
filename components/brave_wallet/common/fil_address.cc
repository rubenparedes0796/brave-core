/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/common/fil_address.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "brave/components/brave_wallet/common/brave_wallet.mojom.h"
#include "brave/components/brave_wallet/common/hex_utils.h"

namespace brave_wallet {

namespace {
#define ADDRESS_LEN_SECP256K 41
#define ADDRESS_LEN_BLS 86
}  // namespace

FilAddress::FilAddress(const std::vector<uint8_t>& bytes) : bytes_(bytes) {}
FilAddress::FilAddress() = default;
FilAddress::FilAddress(const FilAddress& other) = default;
FilAddress::~FilAddress() {}

bool FilAddress::operator==(const FilAddress& other) const {
  return std::equal(bytes_.begin(), bytes_.end(), other.bytes_.begin());
}

bool FilAddress::operator!=(const FilAddress& other) const {
  return !std::equal(bytes_.begin(), bytes_.end(), other.bytes_.begin());
}

// static
FilAddress FilAddress::FromHex(const std::string& input) {
  if (!IsValidAddress(input))
    return FilAddress();
  std::vector<uint8_t> bytes;
  if (!PrefixedHexStringToBytes(input, &bytes)) {
    VLOG(1) << __func__ << ": PrefixedHexStringToBytes failed";
    return FilAddress();
  }

  return FilAddress(bytes);
}
// static
FilAddress FilAddress::FromPublicKey(const std::vector<uint8_t>& public_key) {
  if (public_key.size() != 64) {
    VLOG(1) << __func__ << ": public key size should be 64 bytes";
    return FilAddress();
  }

  return FilAddress(public_key);
}

// static
bool FilAddress::IsValidAddress(const std::string& input) {
  bool secp_address = input.size() == ADDRESS_LEN_SECP256K;
  bool bls_address = input.size() == ADDRESS_LEN_BLS;
  if (!secp_address && !bls_address) {
    VLOG(1) << __func__ << ": input should be " << ADDRESS_LEN_SECP256K
            << " or " << ADDRESS_LEN_BLS << " bytes long";
    return false;
  }
  bool valid_network = base::StartsWith(input, mojom::kFilecoinTestnet) ||
                       base::StartsWith(input, mojom::kFilecoinMainnet);
  auto protocol = secp_address ? mojom::FilecoinAddressProtocol::SECP256K1
                               : mojom::FilecoinAddressProtocol::BLS;
  std::string current{input[1]};
  bool valid_protocol = current == std::to_string(static_cast<int>(protocol));
  return valid_network && valid_protocol;
}

std::string FilAddress::ToHex() const {
  const std::string input(bytes_.begin(), bytes_.end());
  return ::brave_wallet::ToHex(input);
}

}  // namespace brave_wallet
