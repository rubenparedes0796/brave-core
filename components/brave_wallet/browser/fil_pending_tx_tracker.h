/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_FIL_PENDING_TX_TRACKER_H_
#define BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_FIL_PENDING_TX_TRACKER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "brave/components/brave_wallet/browser/fil_tx_state_manager.h"
#include "brave/components/brave_wallet/common/brave_wallet_types.h"

namespace brave_wallet {

class TxMeta;
class FilTxMeta;
class FilNonceTracker;
class JsonRpcService;

class FilPendingTxTracker {
 public:
  FilPendingTxTracker(FilTxStateManager* tx_state_manager,
                      JsonRpcService* json_rpc_service,
                      FilNonceTracker* nonce_tracker);
  ~FilPendingTxTracker();
  FilPendingTxTracker(const FilPendingTxTracker&) = delete;
  FilPendingTxTracker operator=(const FilPendingTxTracker&) = delete;

  bool UpdatePendingTransactions(size_t* num_pending);
  void ResubmitPendingTransactions();
  void Reset();

 private:
  FRIEND_TEST_ALL_PREFIXES(FilPendingTxTrackerUnitTest, IsNonceTaken);
  FRIEND_TEST_ALL_PREFIXES(FilPendingTxTrackerUnitTest, ShouldTxDropped);
  FRIEND_TEST_ALL_PREFIXES(FilPendingTxTrackerUnitTest, DropTransaction);

  void OnGetTxReceipt(std::string id,
                      TransactionReceipt receipt,
                      mojom::ProviderError error,
                      const std::string& error_message);
  void OnGetNetworkNonce(std::string address,
                         uint256_t result,
                         mojom::ProviderError error,
                         const std::string& error_message);
  void OnSendRawTransaction(const std::string& tx_hash,
                            mojom::ProviderError error,
                            const std::string& error_message);

  bool IsNonceTaken(const FilTxMeta&);
  bool ShouldTxDropped(const FilTxMeta&);

  void DropTransaction(TxMeta*);

  // (address, nonce)
  base::flat_map<std::string, uint256_t> network_nonce_map_;
  // (txHash, count)
  base::flat_map<std::string, uint8_t> dropped_blocks_counter_;

  raw_ptr<FilTxStateManager> tx_state_manager_ = nullptr;
  raw_ptr<JsonRpcService> json_rpc_service_ = nullptr;
  raw_ptr<FilNonceTracker> nonce_tracker_ = nullptr;

  base::WeakPtrFactory<FilPendingTxTracker> weak_factory_;
};

}  // namespace brave_wallet

#endif  // BRAVE_COMPONENTS_BRAVE_WALLET_BROWSER_FIL_PENDING_TX_TRACKER_H_
