/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/browser/fil_tx_state_manager.h"

#include <utility>

#include "base/guid.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "brave/components/brave_wallet/browser/brave_wallet_utils.h"
#include "brave/components/brave_wallet/browser/pref_names.h"
#include "brave/components/brave_wallet/common/brave_wallet.mojom.h"
#include "brave/components/brave_wallet/common/fil_address.h"
#include "brave/components/brave_wallet/common/hex_utils.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace brave_wallet {
namespace {
constexpr size_t kMaxConfirmedTxNum = 10;
constexpr size_t kMaxRejectedTxNum = 10;
}  // namespace
#include "base/notreached.h"

namespace brave_wallet {


FilTxStateManager::FilTxStateManager(PrefService* prefs,
                                     JsonRpcService* json_rpc_service)
    : TxStateManager(prefs, json_rpc_service), weak_factory_(this) {
  DCHECK(json_rpc_service_);
  json_rpc_service_->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());
  chain_id_ = json_rpc_service_->GetChainId();
  network_url_ = json_rpc_service_->GetNetworkUrl();
}

FilTxStateManager::~FilTxStateManager() = default;

FilTxStateManager::TxMeta::TxMeta() : tx(std::make_unique<FilTransaction>()) {}
FilTxStateManager::TxMeta::TxMeta(std::unique_ptr<FilTransaction> tx_in)
    : tx(std::move(tx_in)) {}
FilTxStateManager::TxMeta::~TxMeta() = default;
bool FilTxStateManager::TxMeta::operator==(const TxMeta& meta) const {
  return id == meta.id && status == meta.status && from == meta.from &&
         created_time == meta.created_time &&
         submitted_time == meta.submitted_time &&
         confirmed_time == meta.confirmed_time &&
         tx_receipt == meta.tx_receipt && tx_hash == meta.tx_hash &&
         *tx == *meta.tx;
}

base::Value FilTxStateManager::TxMetaToValue(const TxMeta& meta) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("id", meta.id);
  dict.SetIntKey("status", static_cast<int>(meta.status));
  dict.SetStringKey("from", meta.from.ToHex());
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
      meta.id, meta.from.ToHex(), meta.tx_hash,
      mojom::TxDataUnion::NewFilTxData(
          mojom::FilTxData::New(
              meta.tx->nonce() ? Uint256ValueToHex(meta.tx->nonce().value())
                               : "",
              Uint256ValueToHex(meta.tx->gas_price()),
              Uint256ValueToHex(meta.tx->gas_limit()),
              meta.tx->to().ToHex(),
              Uint256ValueToHex(meta.tx->value())
          )),
      meta.status, tx_type, tx_params, tx_args,
      base::Milliseconds(meta.created_time.ToJavaTime()),
      base::Milliseconds(meta.submitted_time.ToJavaTime()),
      base::Milliseconds(meta.confirmed_time.ToJavaTime()));
}

std::string FilTxStateManager::GetTxPrefPathPrefix() {
  return "fil";
}

std::unique_ptr<TxMeta> FilTxStateManager::ValueToTxMeta(
    const base::Value& value) {
  std::unique_ptr<FilTxStateManager::TxMeta> meta =
      std::make_unique<FilTxStateManager::TxMeta>();
  const std::string* id = value.FindStringKey("id");
  if (!id)
    return nullptr;
  meta->id = *id;

  absl::optional<int> status = value.FindIntKey("status");
  if (!status)
    return nullptr;
  meta->status = static_cast<mojom::TransactionStatus>(*status);

  const std::string* from = value.FindStringKey("from");
  if (!from)
    return nullptr;
  meta->from = FilAddress::FromHex(*from);

  const base::Value* created_time = value.FindKey("created_time");
  if (!created_time)
    return nullptr;
  absl::optional<base::Time> created_time_from_value =
      base::ValueToTime(created_time);
  if (!created_time_from_value)
    return nullptr;
  meta->created_time = *created_time_from_value;

  const base::Value* submitted_time = value.FindKey("submitted_time");
  if (!submitted_time)
    return nullptr;
  absl::optional<base::Time> submitted_time_from_value =
      base::ValueToTime(submitted_time);
  if (!submitted_time_from_value)
    return nullptr;
  meta->submitted_time = *submitted_time_from_value;

  const base::Value* confirmed_time = value.FindKey("confirmed_time");
  if (!confirmed_time)
    return nullptr;
  absl::optional<base::Time> confirmed_time_from_value =
      base::ValueToTime(confirmed_time);
  if (!confirmed_time_from_value)
    return nullptr;
  meta->confirmed_time = *confirmed_time_from_value;

  const base::Value* tx_receipt = value.FindKey("tx_receipt");
  if (!tx_receipt)
    return nullptr;
  absl::optional<TransactionReceipt> tx_receipt_from_value =
      ValueToTransactionReceipt(*tx_receipt);
  meta->tx_receipt = *tx_receipt_from_value;

  const std::string* tx_hash = value.FindStringKey("tx_hash");
  if (!tx_hash)
    return nullptr;
  meta->tx_hash = *tx_hash;

  const base::Value* tx = value.FindKey("tx");
  if (!tx)
    return nullptr;
  absl::optional<FilTransaction> tx_from_value =
      FilTransaction::FromValue(*tx);
  if (!tx_from_value)
    return nullptr;
  meta->tx = std::make_unique<FilTransaction>(*tx_from_value);

  return meta;
}

void FilTxStateManager::AddOrUpdateTx(const TxMeta& meta) {
  DictionaryPrefUpdate update(prefs_, kBraveWalletTransactions);
  base::Value* dict = update.Get();
  const std::string path = GetNetworkId(prefs_, chain_id_) + "." + meta.id;
  bool is_add = dict->FindPath(path) == nullptr;
  dict->SetPath(path, TxMetaToValue(meta));
  if (!is_add) {
    for (auto& observer : observers_)
      observer.OnTransactionStatusChanged(TxMetaToTransactionInfo(meta));
    return;
  }

  for (auto& observer : observers_)
    observer.OnNewUnapprovedTx(TxMetaToTransactionInfo(meta));

  // We only keep most recent 10 confirmed and rejected tx metas per network
  RetireTxByStatus(mojom::TransactionStatus::Confirmed, kMaxConfirmedTxNum);
  RetireTxByStatus(mojom::TransactionStatus::Rejected, kMaxRejectedTxNum);
}

std::unique_ptr<FilTxStateManager::TxMeta> FilTxStateManager::GetTx(
    const std::string& id) {
  const base::Value* dict = prefs_->GetDictionary(kBraveWalletTransactions);
  if (!dict)
    return nullptr;
  const base::Value* value =
      dict->FindPath(GetNetworkId(prefs_, chain_id_) + "." + id);
  if (!value)
    return nullptr;

  return ValueToTxMeta(*value);
}

void FilTxStateManager::DeleteTx(const std::string& id) {
  DictionaryPrefUpdate update(prefs_, kBraveWalletTransactions);
  base::Value* dict = update.Get();
  dict->RemovePath(GetNetworkId(prefs_, chain_id_) + "." + id);
}

void FilTxStateManager::WipeTxs() {
  prefs_->ClearPref(kBraveWalletTransactions);
}

std::vector<std::unique_ptr<FilTxStateManager::TxMeta>>
FilTxStateManager::GetTransactionsByStatus(
    absl::optional<mojom::TransactionStatus> status,
    absl::optional<FilAddress> from) {
  std::vector<std::unique_ptr<FilTxStateManager::TxMeta>> result;
  const base::Value* dict = prefs_->GetDictionary(kBraveWalletTransactions);
  const base::Value* network_dict =
      dict->FindKey(GetNetworkId(prefs_, chain_id_));
  if (!network_dict)
    return result;

  for (const auto it : network_dict->DictItems()) {
    std::unique_ptr<FilTxStateManager::TxMeta> meta = ValueToTxMeta(it.second);
    if (!meta) {
      continue;
    }
    if (!status.has_value() || meta->status == *status) {
      if (from.has_value() && meta->from != *from)
        continue;
      result.push_back(std::move(meta));
    }
  }
  return result;
}

void FilTxStateManager::ChainChangedEvent(const std::string& chain_id) {
  chain_id_ = chain_id;
  network_url_ = json_rpc_service_->GetNetworkUrl();
}

void FilTxStateManager::OnAddEthereumChainRequestCompleted(
    const std::string& chain_id,
    const std::string& error) {}

void FilTxStateManager::OnIsEip1559Changed(const std::string& chain_id, bool is_eip1559) {
  
}
void FilTxStateManager::RetireTxByStatus(mojom::TransactionStatus status,
                                         size_t max_num) {
  if (status != mojom::TransactionStatus::Confirmed &&
      status != mojom::TransactionStatus::Rejected)
    return;
  auto tx_metas = GetTransactionsByStatus(status, absl::nullopt);
  if (tx_metas.size() > max_num) {
    FilTxStateManager::TxMeta* oldest_meta = nullptr;
    for (const auto& tx_meta : tx_metas) {
      if (!oldest_meta) {
        oldest_meta = tx_meta.get();
      } else {
        if (tx_meta->status == mojom::TransactionStatus::Confirmed &&
            tx_meta->confirmed_time < oldest_meta->confirmed_time) {
          oldest_meta = tx_meta.get();
        } else if (tx_meta->status == mojom::TransactionStatus::Rejected &&
                   tx_meta->created_time < oldest_meta->created_time) {
          oldest_meta = tx_meta.get();
        }
      }
    }
    DeleteTx(oldest_meta->id);
  }
}

void FilTxStateManager::AddObserver(FilTxStateManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void FilTxStateManager::RemoveObserver(FilTxStateManager::Observer* observer) {
  observers_.RemoveObserver(observer);
>>>>>>> 30a070426b (wip)
}

}  // namespace brave_wallet
