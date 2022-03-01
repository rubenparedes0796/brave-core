/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/browser/fil_tx_manager.h"
#include <stdint.h>
#include <string>

#include <memory>

#include "base/notreached.h"
#include "brave/components/brave_wallet/browser/fil_nonce_tracker.h"
#include "brave/components/brave_wallet/browser/fil_pending_tx_tracker.h"
#include "brave/components/brave_wallet/browser/fil_transaction.h"
#include "brave/components/brave_wallet/browser/fil_tx_meta.h"
#include "brave/components/brave_wallet/browser/fil_tx_state_manager.h"
#include "brave/components/brave_wallet/browser/json_rpc_service.h"
#include "brave/components/brave_wallet/common/fil_address.h"
#include "brave/components/brave_wallet/common/hex_utils.h"
#include "components/grit/brave_components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace brave_wallet {

// static
bool FilTxManager::ValidateTxData(const mojom::FilTxDataPtr& tx_data,
                                  std::string* error) {
  CHECK(error);
  // 'to' cannot be empty
  if (tx_data->to.empty()) {
    *error = l10n_util::GetStringUTF8(IDS_WALLET_FIL_SEND_TRANSACTION_TO);
    return false;
  }

  if (!tx_data->value.empty() && !IsValidHexString(tx_data->value)) {
    *error =
        l10n_util::GetStringUTF8(IDS_WALLET_SEND_TRANSACTION_VALUE_INVALID);
    return false;
  }
  if (!tx_data->to.empty() && !FilAddress::IsValidAddress(tx_data->to)) {
    *error =
        l10n_util::GetStringUTF8(IDS_WALLET_ETH_SEND_TRANSACTION_TO_INVALID);
    return false;
  }
  return true;
}

FilTxManager::FilTxManager(TxService* tx_service,
                           JsonRpcService* json_rpc_service,
                           KeyringService* keyring_service,
                           PrefService* prefs)
    : TxManager(std::make_unique<FilTxStateManager>(prefs, json_rpc_service),
                tx_service,
                json_rpc_service,
                keyring_service,
                prefs),
      tx_state_manager_(
          std::make_unique<FilTxStateManager>(prefs, json_rpc_service)),
      nonce_tracker_(std::make_unique<FilNonceTracker>(tx_state_manager_.get(),
                                                       json_rpc_service)),
      pending_tx_tracker_(
          std::make_unique<FilPendingTxTracker>(tx_state_manager_.get(),
                                                json_rpc_service,
                                                nonce_tracker_.get())) {
  tx_state_manager_->AddObserver(this);
}

FilTxManager::~FilTxManager() {
  tx_state_manager_->RemoveObserver(this);
}

void FilTxManager::AddUnapprovedTransaction(
    mojom::FilTxDataPtr tx_data,
    const std::string& from,
    AddUnapprovedTransactionCallback callback) {
  if (from.empty()) {
    std::move(callback).Run(
        false, "",
        l10n_util::GetStringUTF8(IDS_WALLET_SEND_TRANSACTION_FROM_EMPTY));
    return;
  }

  std::string error;
  if (!FilTxManager::ValidateTxData(tx_data, &error)) {
    std::move(callback).Run(false, "", error);
    return;
  }
  auto tx = FilTransaction::FromTxData(tx_data);
  if (!tx) {
    std::move(callback).Run(
        false, "",
        l10n_util::GetStringUTF8(
            IDS_WALLET_ETH_SEND_TRANSACTION_CONVERT_TX_DATA));
    return;
  }
  auto tx_ptr = std::make_unique<FilTransaction>(*tx);
  auto gas_limit = tx->gas_limit();
  if (!gas_limit) {
    GetEstimatedGas(from, std::move(tx_ptr), std::move(callback));
  } else {
    const std::string gas_premium = tx->gas_premium();
    const std::string gas_fee_cap = tx->gas_fee_cap();
    ContinueAddUnapprovedTransaction(
        from, std::move(tx_ptr), std::move(callback), gas_premium, gas_fee_cap,
        gas_limit, mojom::ProviderError::kSuccess, "");
  }
}

void FilTxManager::GetEstimatedGas(const std::string& from,
                                   std::unique_ptr<FilTransaction> tx,
                                   AddUnapprovedTransactionCallback callback) {
  const std::string gas_premium = tx->gas_premium();
  const std::string gas_fee_cap = tx->gas_fee_cap();
  auto gas_limit = tx->gas_limit();
  uint64_t nonce = 0;  // tx->nonce().value();
  const std::string value = tx->value();
  // TODO(spylogsster): add code to get max fee
  const std::string max_fee = "30000000000000";
  auto to = tx->to().ToString();
  json_rpc_service_->GetFilEstimateGas(
      from, to, gas_premium, gas_fee_cap, gas_limit, nonce, max_fee, value,
      base::BindOnce(&FilTxManager::ContinueAddUnapprovedTransaction,
                     weak_factory_.GetWeakPtr(), from, std::move(tx),
                     std::move(callback)));
}

void FilTxManager::ContinueAddUnapprovedTransaction(
    const std::string& from,
    std::unique_ptr<FilTransaction> tx,
    AddUnapprovedTransactionCallback callback,
    const std::string& gas_premium,
    const std::string& gas_fee_cap,
    uint64_t gas_limit,
    mojom::ProviderError error,
    const std::string& error_message) {
  DLOG(INFO) << gas_premium;
  DLOG(INFO) << gas_fee_cap;
  DLOG(INFO) << gas_limit;
  tx->set_gas_premium(gas_premium);
  tx->set_fee_cap(gas_fee_cap);
  tx->set_gas_limit(gas_limit);

  FilTxMeta meta(std::move(tx));
  meta.set_id(TxMeta::GenerateMetaID());
  meta.set_from(FilAddress::FromString(from).ToString());
  meta.set_created_time(base::Time::Now());
  meta.set_status(mojom::TransactionStatus::Unapproved);
  tx_state_manager_->AddOrUpdateTx(meta);
  std::move(callback).Run(true, meta.id(), "");
}

void FilTxManager::AddUnapprovedTransaction(
    mojom::TxDataUnionPtr tx_data_union,
    const std::string& from,
    AddUnapprovedTransactionCallback callback) {
  AddUnapprovedTransaction(std::move(tx_data_union->get_fil_tx_data()), from,
                           std::move(callback));
}

void FilTxManager::ApproveTransaction(const std::string& tx_meta_id,
                                      ApproveTransactionCallback) {
  NOTIMPLEMENTED();
}

void FilTxManager::RejectTransaction(const std::string& tx_meta_id,
                                     RejectTransactionCallback) {
  NOTIMPLEMENTED();
}

void FilTxManager::GetAllTransactionInfo(const std::string& from,
                                         GetAllTransactionInfoCallback) {
  NOTIMPLEMENTED();
}

void FilTxManager::SpeedupOrCancelTransaction(
    const std::string& tx_meta_id,
    bool cancel,
    SpeedupOrCancelTransactionCallback callback) {
  NOTIMPLEMENTED();
}

void FilTxManager::RetryTransaction(const std::string& tx_meta_id,
                                    RetryTransactionCallback callback) {
  NOTIMPLEMENTED();
}

void FilTxManager::GetTransactionMessageToSign(
    const std::string& tx_meta_id,
    GetTransactionMessageToSignCallback callback) {
  NOTIMPLEMENTED();
}

void FilTxManager::Reset() {
  // TODO(spylogsster): reset members as necessary.
}

std::unique_ptr<FilTxMeta> FilTxManager::GetTxForTesting(
    const std::string& tx_meta_id) {
  return tx_state_manager_->GetFilTx(tx_meta_id);
}

}  // namespace brave_wallet
