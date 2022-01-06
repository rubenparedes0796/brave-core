/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/contribution/external_contribution_processor.h"

#include <string>

#include "bat/ledger/internal/contribution/contribution_fee_processor.h"
#include "bat/ledger/internal/contribution/contribution_store.h"
#include "bat/ledger/internal/core/bat_ledger_job.h"
#include "bat/ledger/internal/external_wallet/external_wallet_manager.h"
#include "bat/ledger/internal/ledger_impl.h"

namespace ledger {

namespace {

constexpr double kTransferFee = 0.05;

class ProcessJob : public BATLedgerJob<bool> {
 public:
  void Start(const ContributionRequest& contribution) {
    DCHECK_GT(contribution.amount, 0.0);
    DCHECK(!contribution.id.empty());
    DCHECK(contribution.type != ContributionType::kAutoContribute);
    DCHECK(contribution.source == ContributionSource::kExternal);

    contribution_ = contribution;
    fee_ = contribution_.amount * kTransferFee;
    amount_ = contribution_.amount - fee_;

    context().Get<ExternalWalletManager>().GetBalance().Then(
        ContinueWith(this, &ProcessJob::OnBalanceFetched));
  }

 private:
  void OnBalanceFetched(absl::optional<double> balance) {
    if (!balance) {
      context().LogError(FROM_HERE) << "Unable to read external wallet balance";
      return Complete(false);
    }

    if (*balance < contribution_.amount) {
      context().LogError(FROM_HERE) << "Insufficient funds for contribution";
      return Complete(false);
    }

    // TODO(zenparsing): Should we force a refresh of the publisher info here?
    // TODO(zenparsing): Should we do this before getting the balance?
    context().GetLedgerImpl()->publisher()->GetServerPublisherInfo(
        contribution_.publisher_id,
        CreateLambdaCallback(this, &ProcessJob::OnPublisherInfoFetched));
  }

  void OnPublisherInfoFetched(mojom::ServerPublisherInfoPtr publisher) {
    if (!publisher) {
      context().LogError(FROM_HERE) << "Unable to fetch publisher info";
      return Complete(false);
    }

    std::string publisher_address = GetPublisherAddress(*publisher);
    if (publisher_address.empty()) {
      context().LogError(FROM_HERE) << "Publisher does not have a matching "
                                    << "wallet provider address";
      return Complete(false);
    }

    context()
        .Get<ExternalWalletManager>()
        .TransferBAT(publisher_address, amount_)
        .Then(ContinueWith(this, &ProcessJob::OnTransferCompleted));
  }

  void OnTransferCompleted(
      absl::optional<ExternalWalletTransferResult> result) {
    if (!result) {
      context().LogError(FROM_HERE) << "Unable to send contribution to "
                                       "publisher";
      return Complete(false);
    }

    context().Get<ContributionFeeProcessor>().SendContributionFee(
        contribution_.id, fee_);

    context()
        .Get<ContributionStore>()
        .SaveContribution(contribution_, *result)
        .Then(ContinueWith(this, &ProcessJob::OnSaved));
  }

  void OnSaved(bool) {
    // TODO(zenparsing): Call the following.
    // ledger_->ledger_client()->OnReconcileComplete
    // ledger_->database()->SaveBalanceReportInfoItem
    Complete(true);
  }

  std::string GetPublisherAddress(const mojom::ServerPublisherInfo& publisher) {
    auto wallet = context().Get<ExternalWalletManager>().GetExternalWallet();
    if (!wallet)
      return "";

    auto can_accept = false;
    auto status = publisher.status;

    switch (wallet->provider) {
      case ExternalWalletProvider::kUphold:
        can_accept = status == mojom::PublisherStatus::UPHOLD_VERIFIED;
        break;
      case ExternalWalletProvider::kGemini:
        can_accept = status == mojom::PublisherStatus::GEMINI_VERIFIED;
        break;
      case ExternalWalletProvider::kBitflyer:
        can_accept = status == mojom::PublisherStatus::BITFLYER_VERIFIED;
        break;
    }

    return can_accept ? publisher.address : "";
  }

  ContributionRequest contribution_;
  double amount_;
  double fee_;
};

}  // namespace

const char ExternalContributionProcessor::kContextKey[] =
    "external-contribution-processor";

Future<bool> ExternalContributionProcessor::ProcessContribution(
    const ContributionRequest& contribution) {
  return context().StartJob<ProcessJob>(contribution);
}

}  // namespace ledger
