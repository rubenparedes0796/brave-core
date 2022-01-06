/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This ContributionSource Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/contribution/contribution_splitter.h"

#include <algorithm>
#include <string>
#include <tuple>
#include <utility>

#include "bat/ledger/internal/contribution/contribution_token_manager.h"
#include "bat/ledger/internal/core/bat_ledger_job.h"
#include "bat/ledger/internal/core/future_join.h"
#include "bat/ledger/internal/external_wallet/external_wallet_manager.h"
#include "bat/ledger/internal/ledger_impl.h"

namespace ledger {

namespace {

using SourceAmount = ContributionSplitter::SourceAmount;
using Split = ContributionSplitter::Split;
using Error = ContributionSplitter::Error;

class SplitJob : public BATLedgerJob<Split> {
 public:
  void Start(const std::string& publisher_id, double amount) {
    remaining_ = amount;

    // TODO(zenparsing): Force refresh of publisher data?
    context().GetLedgerImpl()->publisher()->GetServerPublisherInfo(
        publisher_id,
        CreateLambdaCallback(this, &SplitJob::OnPublisherFetched));
  }

 private:
  void OnPublisherFetched(mojom::ServerPublisherInfoPtr publisher) {
    if (!publisher)
      return Complete(Split(Error::kPublisherNotFound));

    publisher_ = std::move(publisher);

    JoinFutures(GetVirtual(), GetExternal())
        .Then(ContinueWith(this, &SplitJob::OnBalancesReady));
  }

  void OnBalancesReady(std::tuple<double, double> balances) {
    double virtual_tokens = std::get<0>(balances);
    double external = std::get<1>(balances);

    context().LogVerbose(FROM_HERE) << "Contribution sources: ("
                                    << "tokens: " << virtual_tokens << ", "
                                    << "external: " << external << ")";

    if (virtual_tokens + external < remaining_)
      return Complete(Split(Error::kInsufficientFunds));

    Split split;

    AddSplitAmount(ContributionSource::kBraveVG, virtual_tokens, &split);
    AddSplitAmount(ContributionSource::kExternal, external, &split);

    if (remaining_ > 0)
      return Complete(Split(Error::kPublisherNotConfigured));

    Complete(std::move(split));
  }

  bool CanAcceptSource(ContributionSource source) {
    switch (source) {
      case ContributionSource::kBraveVG:
      case ContributionSource::kBraveSKU:
        return publisher_->status != mojom::PublisherStatus::NOT_VERIFIED;
      case ContributionSource::kExternal: {
        auto external =
            context().Get<ExternalWalletManager>().GetExternalWallet();

        if (!external)
          return false;

        auto status = publisher_->status;
        switch (external->provider) {
          case ExternalWalletProvider::kUphold:
            return status == mojom::PublisherStatus::UPHOLD_VERIFIED;
          case ExternalWalletProvider::kGemini:
            return status == mojom::PublisherStatus::GEMINI_VERIFIED;
          case ExternalWalletProvider::kBitflyer:
            return status == mojom::PublisherStatus::BITFLYER_VERIFIED;
        }
      }
    }
  }

  void AddSplitAmount(ContributionSource source,
                      double available,
                      Split* split) {
    if (!CanAcceptSource(source))
      return;

    double amount = std::min(available, remaining_);

    if (amount <= 0)
      return;

    remaining_ -= amount;
    split->amounts.push_back({.source = source, .amount = amount});
  }

  Future<double> GetVirtual() {
    return context().Get<ContributionTokenManager>().GetAvailableTokenBalance(
        ContributionTokenType::kVG);
  }

  Future<double> GetExternal() {
    return context().Get<ExternalWalletManager>().GetBalance().Map(
        base::BindOnce([](absl::optional<double> balance) {
          return balance ? *balance : 0;
        }));
  }

  mojom::ServerPublisherInfoPtr publisher_;
  double remaining_;
};

}  // namespace

Split::Split() = default;

Split::~Split() = default;

Split::Split(Error error) : error(error) {}

Split::Split(const Split& other) = default;

Split& Split::operator=(const Split& other) = default;

Split::Split(Split&& other) = default;

Split& Split::operator=(Split&& other) = default;

const char ContributionSplitter::kContextKey[] = "contribution-splitter";

Future<Split> ContributionSplitter::SplitContribution(
    const std::string& publisher_id,
    double amount) {
  return context().StartJob<SplitJob>(publisher_id, amount);
}

}  // namespace ledger
