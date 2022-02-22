/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/common/fil_address.h"
#include <charconv>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "brave/components/brave_wallet/common/brave_wallet.mojom.h"
#include "brave/third_party/argon2/src/src/blake2/blake2.h"
#include "components/base32/base32.h"

namespace brave_wallet {

namespace {

#define ADDRESS_SIZE_SECP256K 41
#define ADDRESS_SIZE_BLS 86

std::vector<uint8_t> BlakeHash(const std::vector<uint8_t>& payload,
                               size_t length) {
  blake2b_state blakeState;
  if (blake2b_init(&blakeState, length) != 0) {
    VLOG(0) << __func__ << ": blake2b_init failed";
    return std::vector<uint8_t>();
  }
  if (blake2b_update(&blakeState, payload.data(), payload.size()) != 0) {
    VLOG(0) << __func__ << ": blake2b_update failed";
    return std::vector<uint8_t>();
  }
  std::vector<uint8_t> result;
  result.resize(length);
  if (blake2b_final(&blakeState, result.data(), length) != 0) {
    VLOG(0) << __func__ << ": blake2b_final failed";
    return result;
  }
  return result;
}

bool IsValidNetwork(const std::string& network) {
  return network == mojom::kFilecoinTestnet ||
         network == mojom::kFilecoinMainnet;
}

absl::optional<mojom::FilecoinAddressProtocol> ToProtocol(char input) {
  if ((input - '0') == static_cast<int>(mojom::FilecoinAddressProtocol::BLS))
    return mojom::FilecoinAddressProtocol::BLS;
  if ((input - '0') ==
      static_cast<int>(mojom::FilecoinAddressProtocol::SECP256K1))
    return mojom::FilecoinAddressProtocol::SECP256K1;
  return absl::nullopt;
}
}  // namespace

FilAddress::FilAddress(const std::vector<uint8_t>& bytes,
                       mojom::FilecoinAddressProtocol protocol,
                       const std::string& network)
    : protocol_(protocol), network_(network), bytes_(bytes) {
  DCHECK(IsValidNetwork(network));
}
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
FilAddress FilAddress::FromString(const std::string& address) {
  if (address.size() != ADDRESS_SIZE_BLS &&
      address.size() != ADDRESS_SIZE_SECP256K)
    return FilAddress();
  auto protocol = ToProtocol(address[1]);
  if (!protocol)
    return FilAddress();

  std::string network{address[0]};
  if (!IsValidNetwork(network))
    return FilAddress();

  std::string payload{
      base32::Base32Decode(base::ToUpperASCII(address.substr(2)))};
  if (payload.empty())
    return FilAddress();

  int checksum_size = 4;
  std::string key{payload.substr(0, payload.size() - checksum_size)};
  std::vector<uint8_t> public_key(key.begin(), key.end());
  return FilAddress::FromPublicKey(public_key, protocol.value(), network);
}

// static
FilAddress FilAddress::FromUncompressedPublicKey(
    const std::vector<uint8_t>& uncompressed_public_key,
    mojom::FilecoinAddressProtocol protocol,
    const std::string& network) {
  if (uncompressed_public_key.empty())
    return FilAddress();
  auto public_key = BlakeHash(uncompressed_public_key, 20);
  if (public_key.empty())
    return FilAddress();
  return FromPublicKey(public_key, protocol, network);
}

// static
FilAddress FilAddress::FromPublicKey(const std::vector<uint8_t>& public_key,
                                     mojom::FilecoinAddressProtocol protocol,
                                     const std::string& network) {
  if (!IsValidNetwork(network))
    return FilAddress();
  return FilAddress(public_key, protocol, network);
}

// static
bool FilAddress::IsValidAddress(const std::string& address) {
  return !address.empty() &&
         FilAddress::FromString(address).ToString() == address;
}

std::string FilAddress::ToString() const {
  if (bytes_.empty())
    return std::string();
  std::vector<uint8_t> payload(bytes_);
  std::vector<uint8_t> checksumPayload(bytes_);
  checksumPayload.insert(checksumPayload.begin(), static_cast<int>(protocol_));
  auto checksum = BlakeHash(checksumPayload, 4);
  payload.insert(payload.end(), checksum.begin(), checksum.end());
  std::string input(payload.begin(), payload.end());
  std::string encoded_output = base::ToLowerASCII(
      base32::Base32Encode(input, base32::Base32EncodePolicy::OMIT_PADDING));
  return network_ + std::to_string(static_cast<int>(protocol_)) +
         encoded_output;
}

}  // namespace brave_wallet
