/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CORE_BAT_LEDGER_INITIALIZER_H_
#define BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CORE_BAT_LEDGER_INITIALIZER_H_

#include "bat/ledger/internal/core/bat_ledger_context.h"
#include "bat/ledger/internal/core/future.h"
#include "bat/ledger/internal/core/future_cache.h"

namespace ledger {

// Performs one-time initialization of the ledger context by delegating to a
// list of classes that expose an |Initialize| method.
class BATLedgerInitializer : public BATLedgerContext::Object {
 public:
  static const char kContextKey[];

  BATLedgerInitializer();
  ~BATLedgerInitializer() override;

  // Calls |Initialize| on all components that require one-time initialization
  // and returns a result indicating whether all components were successfully
  // initialized. Subsequent calls return a cached result.
  Future<bool> Initialize();

 private:
  // TODO(zenparsing): Do we need this, or can we assume that callers will only
  // call this once?
  FutureCache<bool> initialize_cache_;
};

}  // namespace ledger

#endif  // BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CORE_BAT_LEDGER_INITIALIZER_H_
