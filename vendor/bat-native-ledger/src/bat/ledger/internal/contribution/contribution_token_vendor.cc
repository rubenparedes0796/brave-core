/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/contribution/contribution_token_vendor.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "bat/ledger/internal/contribution/contribution_token_manager.h"
#include "bat/ledger/internal/core/bat_ledger_job.h"
#include "bat/ledger/internal/core/delay_generator.h"
#include "bat/ledger/internal/core/environment_config.h"
#include "bat/ledger/internal/core/job_store.h"
#include "bat/ledger/internal/core/privacy_pass.h"
#include "bat/ledger/internal/core/value_converters.h"
#include "bat/ledger/internal/endpoint/payment/payment_server.h"
#include "bat/ledger/internal/external_wallet/external_wallet_manager.h"
#include "bat/ledger/internal/ledger_impl.h"

namespace ledger {

namespace {

constexpr double kVotePrice = 0.25;
constexpr base::TimeDelta kMinRetryDelay = base::Seconds(15);
constexpr base::TimeDelta kMaxRetryDelay = base::Minutes(30);

// TODO(zenparsing): Do we need a completed status for these jobs?
enum class PurchaseStatus {
  kPending,
  kOrderCreated,
  kTransferCompleted,
  kTransactionSent,
  kTokensCreated,
  kTokensClaimed,
  kComplete
};

std::string StringifyEnum(PurchaseStatus status) {
  switch (status) {
    case PurchaseStatus::kPending:
      return "pending";
    case PurchaseStatus::kOrderCreated:
      return "order-created";
    case PurchaseStatus::kTransferCompleted:
      return "transfer-completed";
    case PurchaseStatus::kTransactionSent:
      return "transaction-sent";
    case PurchaseStatus::kTokensCreated:
      return "tokens-created";
    case PurchaseStatus::kTokensClaimed:
      return "tokens-claimed";
    case PurchaseStatus::kComplete:
      return "complete";
  }
}

void ParseEnum(const std::string& s, absl::optional<PurchaseStatus>* value) {
  if (s == "pending") {
    *value = PurchaseStatus::kPending;
  } else if (s == "order-created") {
    *value = PurchaseStatus::kOrderCreated;
  } else if (s == "transfer-completed") {
    *value = PurchaseStatus::kTransferCompleted;
  } else if (s == "transaction-sent") {
    *value = PurchaseStatus::kTransactionSent;
  } else if (s == "tokens-created") {
    *value = PurchaseStatus::kTokensCreated;
  } else if (s == "tokens-claimed") {
    *value = PurchaseStatus::kTokensClaimed;
  } else if (s == "complete") {
    *value = PurchaseStatus::kComplete;
  } else {
    *value = {};
  }
}

struct PurchaseState {
  int quantity = 0;
  std::string order_id;
  std::string order_item_id;
  absl::optional<ExternalWalletProvider> external_provider;
  std::string external_transaction_id;
  std::vector<std::string> tokens;
  std::vector<std::string> blinded_tokens;
  PurchaseStatus status = PurchaseStatus::kPending;

  base::Value ToValue() const {
    ValueWriter w;
    w.Write("quantity", quantity);
    w.Write("order_id", order_id);
    w.Write("order_item_id", order_item_id);
    w.Write("external_provider", external_provider);
    w.Write("external_transaction_id", external_transaction_id);
    w.Write("tokens", tokens);
    w.Write("blinded_tokens", blinded_tokens);
    w.Write("status", status);
    return w.Finish();
  }

  static absl::optional<PurchaseState> FromValue(const base::Value& value) {
    StructValueReader<PurchaseState> r(value);
    r.Read("quantity", &PurchaseState::quantity);
    r.Read("order_id", &PurchaseState::order_id);
    r.Read("order_item_id", &PurchaseState::order_item_id);
    r.Read("external_provider", &PurchaseState::external_provider);
    r.Read("external_transaction_id", &PurchaseState::external_transaction_id);
    r.Read("tokens", &PurchaseState::tokens);
    r.Read("blinded_tokens", &PurchaseState::blinded_tokens);
    r.Read("status", &PurchaseState::status);
    return r.Finish();
  }
};

class PurchaseJob : public ResumableJob<bool, PurchaseState> {
 public:
  static constexpr char kJobType[] = "contribution-token-purchase";

 protected:
  void Resume() override {
    payment_server_.reset(
        new endpoint::PaymentServer(context().GetLedgerImpl()));

    switch (state().status) {
      case PurchaseStatus::kPending:
        return CreateOrder();
      case PurchaseStatus::kOrderCreated:
        return TransferFunds();
      case PurchaseStatus::kTransferCompleted:
        return SendTransaction();
      case PurchaseStatus::kTransactionSent:
        return CreateTokens();
      case PurchaseStatus::kTokensCreated:
        return ClaimTokens();
      case PurchaseStatus::kTokensClaimed:
        return FetchSignedTokens();
      case PurchaseStatus::kComplete:
        return Complete(true);
    }
  }

  void OnStateInvalid() override { Complete(false); }

 private:
  void CreateOrder() {
    if (state().quantity <= 0) {
      NOTREACHED();
      context().LogError(FROM_HERE) << "Invalid token order quantity";
      return Complete(false);
    }

    mojom::SKUOrderItem item;
    item.sku = context().Get<EnvironmentConfig>().auto_contribute_sku();
    item.quantity = state().quantity;
    std::vector<mojom::SKUOrderItem> items;
    items.push_back(std::move(item));

    payment_server_->post_order()->Request(
        items, CreateLambdaCallback(this, &PurchaseJob::OnOrderResponse));
  }

  void OnOrderResponse(mojom::Result result, mojom::SKUOrderPtr order) {
    if (result != mojom::Result::LEDGER_OK || !order) {
      // TODO(zenparsing): Retry instead?
      context().LogError(FROM_HERE) << "Error attempting to create SKU order";
      return Complete(false);
    }

    if (order->items.size() != 1) {
      context().LogError(FROM_HERE) << "Unexpected number of SKU order items";
      return Complete(false);
    }

    auto& item = order->items.front();
    // TODO(zenparsing): Double comparison?
    if (item->price != kVotePrice) {
      context().LogError(FROM_HERE) << "Unexpected vote price for SKU item";
      return Complete(false);
    }

    state().order_id = item->order_id;
    state().order_item_id = item->order_item_id;
    state().status = PurchaseStatus::kOrderCreated;
    SaveState();

    TransferFunds();
  }

  void TransferFunds() {
    auto& manager = context().Get<ExternalWalletManager>();
    auto destination = manager.GetContributionTokenOrderAddress();
    if (!destination) {
      context().LogError(FROM_HERE) << "External provider does not support "
                                    << "contribution token orders";
      return Complete(false);
    }

    double transfer_amount = state().quantity * kVotePrice;
    manager.TransferBAT(*destination, transfer_amount)
        .Then(ContinueWith(this, &PurchaseJob::OnTransferCompleted));
  }

  /*
  void TransferAnonymous() {
    std::string address =
        context().Get<EnvironmentConfig>().anonymous_token_order_address();

    payment_server_->post_transaction_anon()->Request(
        order_.quantity * kVotePrice, order_.order_id, address,
        CreateLambdaCallback(this, &ResumeJob::OnTransferAnonymousResponse));
  }

  void OnTransferAnonymousResponse(mojom::Result result) {
    if (result != mojom::Result::LEDGER_OK) {
      context().LogError(FROM_HERE) << "Anonymous funds transfer failed";
      return MarkFailed();
    }

    context()
        .Get<ContributionStore>()
        .MarkOrderTransferCompleted(order_.order_id)
        .Then(ContinueWith(this, &ResumeJob::OnTransferCompleteSaved));
  }
  */

  void OnTransferCompleted(
      absl::optional<ExternalWalletTransferResult> result) {
    if (!result) {
      // TODO(zenparsing): Retry instead?
      context().LogError(FROM_HERE) << "External transfer failed";
      return Complete(false);
    }

    state().external_provider = result->provider;
    state().external_transaction_id = result->transaction_id;
    state().status = PurchaseStatus::kTransferCompleted;
    SaveState();

    SendTransaction();
  }

  void SendTransaction() {
    if (!state().external_provider) {
      NOTREACHED();
      context().LogError(FROM_HERE)
          << "AC state missing external wallet provider";
      return Complete(false);
    }

    switch (*state().external_provider) {
      case ExternalWalletProvider::kUphold:
        return SendUpholdTransaction();
      case ExternalWalletProvider::kGemini:
        return SendGeminiTransaction();
      case ExternalWalletProvider::kBitflyer:
        NOTREACHED();
        context().LogError(FROM_HERE) << "Invalid external wallet provider";
        return Complete(false);
    }
  }

  void SendUpholdTransaction() {
    mojom::SKUTransaction sku_transaction;
    sku_transaction.order_id = state().order_id;
    sku_transaction.external_transaction_id = state().external_transaction_id;

    payment_server_->post_transaction_uphold()->Request(
        sku_transaction,
        CreateLambdaCallback(this, &PurchaseJob::OnTransactionSent));
  }

  void SendGeminiTransaction() {
    mojom::SKUTransaction sku_transaction;
    sku_transaction.order_id = state().order_id;
    sku_transaction.external_transaction_id = state().external_transaction_id;

    payment_server_->post_transaction_gemini()->Request(
        sku_transaction,
        CreateLambdaCallback(this, &PurchaseJob::OnTransactionSent));
  }

  void OnTransactionSent(mojom::Result result) {
    if (result != mojom::Result::LEDGER_OK) {
      context().LogError(FROM_HERE) << "Unable to send external transaction ID";
      WaitForRetryThen(ContinueWith(this, &PurchaseJob::SendTransaction));
      return;
    }

    backoff_.Reset();

    state().status = PurchaseStatus::kTransactionSent;
    SaveState();

    // TODO(zenparsing): Before creating and claiming tokens, we need to poll
    // the order endpoint to wait for the transaction to complete.

    CreateTokens();
  }

  void CreateTokens() {
    auto batch =
        context().Get<PrivacyPass>().CreateBlindedTokens(state().quantity);

    state().tokens = std::move(batch.tokens);
    state().blinded_tokens = std::move(batch.blinded_tokens);
    state().status = PurchaseStatus::kTokensCreated;
    SaveState();

    ClaimTokens();
  }

  void ClaimTokens() {
    base::Value blinded_token_list(base::Value::Type::LIST);
    for (auto& blinded_token : state().blinded_tokens) {
      blinded_token_list.Append(blinded_token);
    }

    payment_server_->post_credentials()->Request(
        state().order_id, state().order_item_id, "single-use",
        std::make_unique<base::ListValue>(blinded_token_list.GetList()),
        CreateLambdaCallback(this, &PurchaseJob::OnTokensClaimed));
  }

  void OnTokensClaimed(mojom::Result result) {
    if (result != mojom::Result::LEDGER_OK) {
      // TODO(zenparsing): Claiming can fail if the external transaction has not
      // cleared yet. Retrying the claiming endpoint won't help, since the
      // backend does not poll for transaction completion in the background.
      // Instead, the protocol requires that we call the "get order" endpoint
      // until it reports that the order has been paid.
      context().LogError(FROM_HERE) << "Unable to claim signed tokens";
      WaitForRetryThen(ContinueWith(this, &PurchaseJob::ClaimTokens));
      return;
    }

    backoff_.Reset();

    state().status = PurchaseStatus::kTokensClaimed;
    SaveState();

    FetchSignedTokens();
  }

  void FetchSignedTokens() {
    payment_server_->get_credentials()->Request(
        state().order_id, state().order_item_id,
        CreateLambdaCallback(this, &PurchaseJob::OnSignedTokensFetched));
  }

  void OnSignedTokensFetched(mojom::Result result, mojom::CredsBatchPtr batch) {
    if (result != mojom::Result::LEDGER_OK || !batch) {
      context().LogError(FROM_HERE) << "Unable to fetch signed tokens";
      WaitForRetryThen(ContinueWith(this, &PurchaseJob::FetchSignedTokens));
      return;
    }

    backoff_.Reset();

    std::vector<std::string> signed_tokens;
    if (auto value = base::JSONReader::Read(batch->signed_creds)) {
      if (value->is_list()) {
        for (auto& item : value->GetList()) {
          if (auto* s = item.GetIfString()) {
            signed_tokens.push_back(*s);
          }
        }
      }
    }

    auto unblinded_tokens = context().Get<PrivacyPass>().UnblindTokens(
        state().tokens, state().blinded_tokens, signed_tokens,
        batch->batch_proof, batch->public_key);

    if (!unblinded_tokens) {
      // TODO(zenparsing): How to recover? Keep retrying the server? What if
      // something is wrong with us?
      return Complete(false);
    }

    std::vector<ContributionToken> contribution_tokens;
    for (auto& unblinded : *unblinded_tokens) {
      contribution_tokens.push_back(
          ContributionToken{.id = 0,
                            .value = kVotePrice,
                            .unblinded_token = std::move(unblinded),
                            .public_key = batch->public_key});
    }

    context()
        .Get<ContributionTokenManager>()
        .InsertTokens(contribution_tokens, ContributionTokenType::kSKU)
        .Then(ContinueWith(this, &PurchaseJob::OnTokensInserted));
  }

  void OnTokensInserted(bool success) {
    if (!success) {
      // TODO(zenparsing): What to do?
    }

    state().status = PurchaseStatus::kComplete;
    SaveState();

    Complete(true);
  }

  void WaitForRetryThen(base::OnceCallback<void()> callback) {
    // TODO(zenparsing): Does this need to be random?
    context()
        .Get<DelayGenerator>()
        .Delay(FROM_HERE, backoff_.GetNextDelay())
        .DiscardValueThen(std::move(callback));
  }

  std::unique_ptr<endpoint::PaymentServer> payment_server_;
  BackoffDelay backoff_{kMinRetryDelay, kMaxRetryDelay};
};

}  // namespace

const char ContributionTokenVendor::kContextKey[] = "contribution-token-vendor";

std::string ContributionTokenVendor::StartPurchase(double amount) {
  int quantity = std::max(double(0), std::floor(amount / kVotePrice));
  return context().Get<JobStore>().InitializeJobState<PurchaseJob>(
      PurchaseState{.quantity = quantity});
}

Future<bool> ContributionTokenVendor::CompletePurchase(
    const std::string& job_id) {
  return context().StartJob<PurchaseJob>(job_id);
}

}  // namespace ledger
