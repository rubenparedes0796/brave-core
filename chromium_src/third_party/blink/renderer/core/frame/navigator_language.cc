/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "third_party/blink/renderer/core/frame/navigator_language.h"

#include "third_party/blink/public/platform/web_content_settings_client.h"

#define EnsureUpdatedLanguage EnsureUpdatedLanguage_ChromiumImpl
#include "src/third_party/blink/renderer/core/frame/navigator_language.cc"
#undef EnsureUpdatedLanguage

namespace blink {

void NavigatorLanguage::EnsureUpdatedLanguage() {
  LOG(ERROR) << "1";
  EnsureUpdatedLanguage_ChromiumImpl();
  languages_.clear();
  languages_.push_back("zuul");
#if 0
  blink::WebContentSettingsClient* settings =
      brave::GetContentSettingsClientFor(execution_context_);
  // If Brave Shields are down or anti-fingerprinting is off for this site,
  // do nothing.
  LOG(ERROR) << "1";
  if (!settings || settings->GetBraveFarblingLevel() == BraveFarblingLevel::OFF)
    return;
  LOG(ERROR) << "2";
  if (settings->GetBraveFarblingLevel() == BraveFarblingLevel::MAXIMUM) {
    languages_.resize(2);
    languages_[0] = "en-US";
    languages_[1] = "en";
  } else {
    languages_.Shrink(1);
  }
#endif
}

}  // namespace blink
