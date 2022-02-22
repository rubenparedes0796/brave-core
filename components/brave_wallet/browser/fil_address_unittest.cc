/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "brave/components/brave_wallet/common/fil_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace brave_wallet {

TEST(FilAddressUnitTest, FromString) {
  std::string address = "t1h4n7rphclbmwyjcp6jrdiwlfcuwbroxy3jvg33q";
  EXPECT_EQ(FilAddress::FromString(address).ToString(), address);

  address = "f1h4n7rphclbmwyjcp6jrdiwlfcuwbroxy3jvg33q";
  EXPECT_EQ(FilAddress::FromString(address).ToString(), address);
  EXPECT_FALSE(FilAddress::FromString(address).IsEmpty());

  address =
      "t3wv3u6pmfi3j6pf3fhjkch372pkyg2tgtlb3jpu3eo6mnt7ttsft6x2xr54ct7fl2"
      "oz4o4tpa4mvigcrayh4a";
  EXPECT_EQ(FilAddress::FromString(address).ToString(), address);

  address =
      "f3wv3u6pmfi3j6pf3fhjkch372pkyg2tgtlb3jpu3eo6mnt7ttsft6x2xr54ct7fl2"
      "oz4o4tpa4mvigcrayh4a";
  EXPECT_EQ(FilAddress::FromString(address).ToString(), address);

  address =
      "t3yaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaby2smx7a";
  EXPECT_EQ(FilAddress::FromString(address).ToString(), address);

  // wrong size for SECP256K1 account
  address =
      "f1wv3u6pmfi3j6pf3fhjkch372pkyg2tgtlb3jpu3eo6mnt7ttsft6x2xr54ct7fl2"
      "oz4o4tpa4mvigcrayh4a";
  EXPECT_NE(FilAddress::FromString(address).ToString(), address);

  // wrong size for BLS account
  address = "t3h4n7rphclbmwyjcp6jrdiwlfcuwbroxy3jvg33q";
  EXPECT_NE(FilAddress::FromString(address).ToString(), address);

  // broken key
  address = "t1h3n7rphclbmwyjcp6jrdiwlfcuwbroxy3jvg33q";
  EXPECT_NE(FilAddress::FromString(address).ToString(), address);

  address = "";
  EXPECT_EQ(FilAddress::FromString(address).ToString(), address);
  EXPECT_TRUE(FilAddress::FromString(address).IsEmpty());
}

TEST(FilAddressUnitTest, IsValidAddress) {
  EXPECT_TRUE(
      FilAddress::IsValidAddress("t1h4n7rphclbmwyjcp6jrdiwlfcuwbroxy3jvg33q"));
  EXPECT_TRUE(FilAddress::IsValidAddress(
      "t3wv3u6pmfi3j6pf3fhjkch372pkyg2tgtlb3jpu3eo6mnt7ttsft6x2xr54ct7fl2"
      "oz4o4tpa4mvigcrayh4a"));
  EXPECT_TRUE(
      FilAddress::IsValidAddress("f1h4n7rphclbmwyjcp6jrdiwlfcuwbroxy3jvg33q"));
  EXPECT_TRUE(FilAddress::IsValidAddress(
      "t3yaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaby2smx7a"));
  EXPECT_FALSE(FilAddress::IsValidAddress(
      "f1wv3u6pmfi3j6pf3fhjkch372pkyg2tgtlb3jpu3eo6mnt7ttsft6x2xr54ct7fl2"
      "oz4o4tpa4mvigcrayh4a"));
  EXPECT_FALSE(
      FilAddress::IsValidAddress("t3h4n7rphclbmwyjcp6jrdiwlfcuwbroxy3jvg33q"));
  EXPECT_FALSE(
      FilAddress::IsValidAddress("t1h3n7rphclbmwyjcp6jrdiwlfcuwbroxy3jvg33q"));
  EXPECT_FALSE(FilAddress::IsValidAddress(""));
}

}  // namespace brave_wallet
