/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/browser/fil_tx_state_manager.h"

#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "brave/components/brave_wallet/browser/brave_wallet_utils.h"
#include "brave/components/brave_wallet/browser/fil_tx_meta.h"
#include "brave/components/brave_wallet/browser/json_rpc_service.h"
#include "brave/components/brave_wallet/browser/tx_meta.h"
#include "brave/components/brave_wallet/common/fil_address.h"

namespace brave_wallet {

FilTxStateManager::FilTxStateManager(PrefService* prefs,
                                     JsonRpcService* json_rpc_service)
    : TxStateManager(prefs, json_rpc_service), weak_factory_(this) {}

FilTxStateManager::~FilTxStateManager() = default;
/*
std::vector<std::unique_ptr<TxMeta>> FilTxStateManager::GetTransactionsByStatus(
    absl::optional<mojom::TransactionStatus> status,
    absl::optional<FilAddress> from) {
  std::vector<std::unique_ptr<TxMeta>> result;
  absl::optional<std::string> from_string =
      from.has_value() ? absl::optional<std::string>(from->ToString())
                       : absl::nullopt;
  return TxStateManager::GetTransactionsByStatus(status, from_string);
}

base::Value FilTxStateManager::TxMetaToValue(const TxMeta& meta) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("id", meta.id);
  dict.SetIntKey("status", static_cast<int>(meta.status));
  dict.SetStringKey("from", meta.from.ToString());
  dict.SetKey("created_time", base::TimeToValue(meta.created_time));
  dict.SetKey("submitted_time", base::TimeToValue(meta.submitted_time));
  dict.SetKey("confirmed_time", base::TimeToValue(meta.confirmed_time));
  dict.SetKey("tx_receipt", TransactionReceiptToValue(meta.tx_receipt));
  dict.SetStringKey("tx_hash", meta.tx_hash);
  dict.SetKey("tx", meta.tx->ToValue());

  return dict;
}

mojom::TransactionInfoPtr FilTxStateManager::TxMetaToTransactionInfo(
    const TxMeta& meta) {
  std::string chain_id;
  std::string max_priority_fee_per_gas;
  std::string max_fee_per_gas;

  mojom::TransactionType tx_type;
  std::vector<std::string> tx_params;
  std::vector<std::string> tx_args;
  return mojom::TransactionInfo::New(
      meta.id, meta.from.ToString(), meta.tx_hash,
      mojom::TxDataUnion::NewFilTxData(mojom::FilTxData::New(
          meta.tx->nonce() ? Uint256ValueToHex(meta.tx->nonce().value()) : "",
          Uint256ValueToHex(meta.tx->gas_price()),
          Uint256ValueToHex(meta.tx->gas_limit()), meta.tx->to().ToString(),
          Uint256ValueToHex(meta.tx->value()))),
      meta.status, tx_type, tx_params, tx_args,
      base::Milliseconds(meta.created_time.ToJavaTime()),
      base::Milliseconds(meta.submitted_time.ToJavaTime()),
      base::Milliseconds(meta.confirmed_time.ToJavaTime()));
}
*/
std::unique_ptr<TxMeta> FilTxStateManager::ValueToTxMeta(
    const base::Value& value) {
  std::unique_ptr<FilTxMeta> meta = std::make_unique<FilTxMeta>();

  if (!TxStateManager::ValueToTxMeta(value, meta.get()))
    return nullptr;

  const base::Value* tx_receipt = value.FindKey("tx_receipt");
  if (!tx_receipt)
    return nullptr;
  absl::optional<TransactionReceipt> tx_receipt_from_value =
      ValueToTransactionReceipt(*tx_receipt);
  meta->set_tx_receipt(*tx_receipt_from_value);

  return meta;
}

std::unique_ptr<FilTxMeta> FilTxStateManager::GetFilTx(const std::string& id) {
  return std::unique_ptr<FilTxMeta>{
      static_cast<FilTxMeta*>(TxStateManager::GetTx(id).release())};
}

std::string FilTxStateManager::GetTxPrefPathPrefix() {
  return "fil." + GetNetworkId(prefs_, json_rpc_service_->GetChainId());
}

}  // namespace brave_wallet
