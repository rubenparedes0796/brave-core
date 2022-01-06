/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/contribution/contribution_settings.h"

namespace ledger {

const ContributionSettings ContributionSettings::kDevelopment = {
    .auto_contribute_sku =
        "AgEJYnJhdmUuY29tAiNicmF2ZSB1c2VyLXdhbGxldC12b3RlIHNrdSB0b2tlbiB2MQACFH"
        "NrdT11c2VyLXdhbGxldC12b3RlAAIKcHJpY2U9MC4yNQACDGN1cnJlbmN5PUJBVAACDGRl"
        "c2NyaXB0aW9uPQACGmNyZWRlbnRpYWxfdHlwZT1zaW5nbGUtdXNlAAAGINiB9dUmpqLyeS"
        "EdZ23E4dPXwIBOUNJCFN9d5toIME2M",
    .anonymous_funds_sku =
        "AgEJYnJhdmUuY29tAiFicmF2ZSBhbm9uLWNhcmQtdm90ZSBza3UgdG9rZW4gdjEAAhJza3"
        "U9YW5vbi1jYXJkLXZvdGUAAgpwcmljZT0wLjI1AAIMY3VycmVuY3k9QkFUAAIMZGVzY3Jp"
        "cHRpb249AAIaY3JlZGVudGlhbF90eXBlPXNpbmdsZS11c2UAAAYgPpv+Al9jRgVCaR49/"
        "AoRrsjQqXGqkwaNfqVka00SJxQ=",
    .anonymous_token_order_address = "9094c3f2-b3ae-438f-bd59-92aaad92de5c"};

const ContributionSettings ContributionSettings::kStaging = {
    .auto_contribute_sku =
        "AgEJYnJhdmUuY29tAiNicmF2ZSB1c2VyLXdhbGxldC12b3RlIHNrdSB0b2tlbiB2MQACFH"
        "NrdT11c2VyLXdhbGxldC12b3RlAAIKcHJpY2U9MC4yNQACDGN1cnJlbmN5PUJBVAACDGRl"
        "c2NyaXB0aW9uPQACGmNyZWRlbnRpYWxfdHlwZT1zaW5nbGUtdXNlAAAGIOH4Li+"
        "rduCtFOfV8Lfa2o8h4SQjN5CuIwxmeQFjOk4W",
    .anonymous_funds_sku =
        "AgEJYnJhdmUuY29tAiFicmF2ZSBhbm9uLWNhcmQtdm90ZSBza3UgdG9rZW4gdjEAAhJza3"
        "U9YW5vbi1jYXJkLXZvdGUAAgpwcmljZT0wLjI1AAIMY3VycmVuY3k9QkFUAAIMZGVzY3Jp"
        "cHRpb249AAIaY3JlZGVudGlhbF90eXBlPXNpbmdsZS11c2UAAAYgPV/"
        "WYY5pXhodMPvsilnrLzNH6MA8nFXwyg0qSWX477M=",
    .anonymous_token_order_address = "6654ecb0-6079-4f6c-ba58-791cc890a561"};

const ContributionSettings ContributionSettings::kProduction = {
    .auto_contribute_sku =
        "AgEJYnJhdmUuY29tAiNicmF2ZSB1c2VyLXdhbGxldC12b3RlIHNrdSB0b2tlbiB2MQACFH"
        "NrdT11c2VyLXdhbGxldC12b3RlAAIKcHJpY2U9MC4yNQACDGN1cnJlbmN5PUJBVAACDGRl"
        "c2NyaXB0aW9uPQACGmNyZWRlbnRpYWxfdHlwZT1zaW5nbGUtdXNlAAAGIOaNAUCBMKm0Ia"
        "LqxefhvxOtAKB0OfoiPn0NPVfI602J",
    .anonymous_funds_sku =
        "AgEJYnJhdmUuY29tAiFicmF2ZSBhbm9uLWNhcmQtdm90ZSBza3UgdG9rZW4gdjEAAhJza3"
        "U9YW5vbi1jYXJkLXZvdGUAAgpwcmljZT0wLjI1AAIMY3VycmVuY3k9QkFUAAIMZGVzY3Jp"
        "cHRpb249AAIaY3JlZGVudGlhbF90eXBlPXNpbmdsZS11c2UAAAYgrMZm85YYwnmjPXcegy"
        "5pBM5C+ZLfrySZfYiSe13yp8o=",
    .anonymous_token_order_address = "86f26f49-9d3b-4f97-9b56-d305ad7a856f"};

}  // namespace ledger
