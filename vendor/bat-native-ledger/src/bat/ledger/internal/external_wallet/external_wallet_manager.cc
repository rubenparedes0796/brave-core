/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/external_wallet/external_wallet_manager.h"

#include <string>
#include <utility>

#include "bat/ledger/internal/core/bat_ledger_job.h"
#include "bat/ledger/internal/external_wallet/bitflyer/bitflyer_wallet_handler.h"
#include "bat/ledger/internal/external_wallet/gemini/gemini_wallet_handler.h"
#include "bat/ledger/internal/external_wallet/uphold/uphold_wallet_handler.h"
#include "bat/ledger/internal/ledger_impl.h"

namespace ledger {

namespace {

ExternalWalletHandler& GetHandler(BATLedgerContext* context,
                                  ExternalWalletProvider provider) {
  DCHECK(context);
  switch (provider) {
    case ExternalWalletProvider::kUphold:
      return context->Get<UpholdWalletHandler>();
    case ExternalWalletProvider::kGemini:
      return context->Get<GeminiWalletHandler>();
    case ExternalWalletProvider::kBitflyer:
      return context->Get<BitflyerWalletHandler>();
  }
}

ExternalWalletHandler& GetHandler(BATLedgerContext* context,
                                  const ExternalWallet& wallet) {
  return GetHandler(context, wallet.provider);
}

absl::optional<ExternalWallet> GetFirstExternalWallet(LedgerImpl* ledger) {
  DCHECK(ledger);

  if (auto uphold = ledger->uphold()->GetWallet()) {
    if (auto external = ExternalWallet::FromMojo(*uphold))
      return external;
  }

  if (auto gemini = ledger->gemini()->GetWallet()) {
    if (auto external = ExternalWallet::FromMojo(*gemini))
      return external;
  }

  if (auto bitflyer = ledger->bitflyer()->GetWallet()) {
    if (auto external = ExternalWallet::FromMojo(*bitflyer))
      return external;
  }

  return {};
}

using TransferResult = ExternalWalletTransferResult;

class TransferJob : public BATLedgerJob<absl::optional<TransferResult>> {
 public:
  void Start(const ExternalWallet& wallet,
             const std::string& destination,
             double amount,
             const std::string& description) {
    provider_ = wallet.provider;

    GetHandler(&context(), wallet)
        .TransferBAT(wallet, destination, amount, description)
        .Then(ContinueWith(this, &TransferJob::OnCompleted));
  }

 private:
  void OnCompleted(absl::optional<std::string> transaction_id) {
    if (!transaction_id) {
      return Complete({});
    }

    Complete(TransferResult{.provider = provider_,
                            .transaction_id = std::move(*transaction_id)});
  }

  ExternalWalletProvider provider_;
};

}  // namespace

const char ExternalWalletManager::kContextKey[] = "external-wallet-manager";

std::string ExternalWalletManager::GetAuthorizationURL(
    ExternalWalletProvider provider) {
  return GetHandler(&context(), provider).GetAuthorizationURL();
}

Future<absl::optional<ExternalWallet>>
ExternalWalletManager::HandleAuthorizationResponse(
    ExternalWalletProvider provider,
    const base::flat_map<std::string, std::string>& auth_params) {
  auto& handler = GetHandler(&context(), provider);
  return handler.HandleAuthorizationResponse(auth_params);
}

Future<absl::optional<double>> ExternalWalletManager::GetBalance() {
  auto external_wallet = GetExternalWallet();
  if (!external_wallet)
    return Future<absl::optional<double>>::Completed({});

  auto& handler = GetHandler(&context(), *external_wallet);
  return handler.GetBalance(*external_wallet);
}

Future<absl::optional<TransferResult>> ExternalWalletManager::TransferBAT(
    const std::string& destination,
    double amount) {
  return TransferBAT(destination, amount, "");
}

Future<absl::optional<TransferResult>> ExternalWalletManager::TransferBAT(
    const std::string& destination,
    double amount,
    const std::string& description) {
  auto external_wallet = GetExternalWallet();
  if (!external_wallet)
    return Future<absl::optional<TransferResult>>::Completed({});

  return context().StartJob<TransferJob>(*external_wallet, destination, amount,
                                         description);
}

absl::optional<ExternalWallet> ExternalWalletManager::GetExternalWallet() {
  return GetFirstExternalWallet(context().GetLedgerImpl());
}

bool ExternalWalletManager::HasExternalWallet() {
  return static_cast<bool>(GetExternalWallet());
}

absl::optional<std::string> ExternalWalletManager::GetContributionFeeAddress() {
  auto external_wallet = GetExternalWallet();
  if (!external_wallet)
    return {};

  auto& handler = GetHandler(&context(), *external_wallet);
  return handler.GetContributionFeeAddress();
}

absl::optional<std::string>
ExternalWalletManager::GetContributionTokenOrderAddress() {
  auto external_wallet = GetExternalWallet();
  if (!external_wallet)
    return {};

  auto& handler = GetHandler(&context(), *external_wallet);
  return handler.GetContributionTokenOrderAddress();
}

Future<bool> ExternalWalletManager::Unlink() {
  // TODO(zenparsing): Implement this.
  return Future<bool>::Completed(false);
}

}  // namespace ledger
