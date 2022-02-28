/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/contribution/token_contribution_processor.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "bat/ledger/internal/contribution/contribution_store.h"
#include "bat/ledger/internal/core/bat_ledger_job.h"
#include "bat/ledger/internal/credentials/credentials_redeem.h"
#include "bat/ledger/internal/endpoint/promotion/promotion_server.h"
#include "bat/ledger/internal/ledger_impl.h"
#include "bat/ledger/internal/payments/payment_service.h"

namespace ledger {

namespace {

mojom::RewardsType ContributionTypeToRewardsType(ContributionType type) {
  switch (type) {
    case ContributionType::kOneTime:
      return mojom::RewardsType::ONE_TIME_TIP;
    case ContributionType::kRecurring:
      return mojom::RewardsType::RECURRING_TIP;
    case ContributionType::kAutoContribute:
      return mojom::RewardsType::AUTO_CONTRIBUTE;
  }
}

class ProcessJob : public BATLedgerJob<bool> {
 public:
  void Start(const Contribution& contribution) {
    DCHECK(!contribution.id.empty());
    DCHECK_GT(contribution.amount, 0.0);

    contribution_ = contribution;

    context()
        .Get<ContributionTokenManager>()
        .ReserveTokens(GetContributionTokenType(), contribution_.amount)
        .Then(ContinueWith(this, &ProcessJob::OnTokensReserved));
  }

  void Start(const Contribution& contribution,
             ContributionTokenManager::Hold hold) {
    DCHECK(!contribution.id.empty());
    contribution_ = contribution;
    OnTokensReserved(std::move(hold));
  }

 private:
  void OnTokensReserved(ContributionTokenHold hold) {
    hold_ = std::move(hold);

    double total_value = hold_.GetTotalValue();
    if (total_value < contribution_.amount) {
      context().LogError(FROM_HERE)
          << "Insufficient tokens reserved for contribution";
      return Complete(false);
    }

    // The contribution amount could differ slightly from the requested amount
    // based on the per-token value. Update the contribution amount to reflect
    // the value of the tokens being sent.
    contribution_.amount = total_value;

    if (GetContributionTokenType() == ContributionTokenType::kSKU) {
      RedeemVotes();
    } else {
      RedeemGrantTokens();
    }
  }

  void RedeemVotes() {
    std::vector<PaymentVote> votes;

    // TODO(zenparsing): It's odd that we have to make copies here. The problem
    // is that there is currently a knowledge boundary between payments and
    // contributions.
    for (auto& token : hold_.tokens()) {
      votes.push_back(PaymentVote{.unblinded_token = token.unblinded_token,
                                  .public_key = token.public_key});
    }

    context()
        .Get<PaymentService>()
        .PostPublisherVotes(contribution_.publisher_id, GetVoteType(),
                            std::move(votes))
        .Then(ContinueWith(this, &ProcessJob::OnContributionProcessed));
  }

  void RedeemGrantTokens() {
    std::vector<mojom::UnblindedToken> ut_list;
    for (auto& token : hold_.tokens()) {
      mojom::UnblindedToken ut;
      ut.id = token.id;
      ut.token_value = token.unblinded_token;
      ut.public_key = token.public_key;
      ut_list.push_back(std::move(ut));
    }

    credential::CredentialsRedeem redeem;
    redeem.publisher_key = contribution_.publisher_id;
    redeem.type = ContributionTypeToRewardsType(contribution_.type);
    redeem.processor = mojom::ContributionProcessor::NONE;
    redeem.token_list = std::move(ut_list);

    promotion_server_.reset(
        new endpoint::PromotionServer(context().GetLedgerImpl()));

    promotion_server_->post_suggestions()->Request(
        redeem, CreateLambdaCallback(this, &ProcessJob::OnGrantTokensRedeemed));
  }

  void OnGrantTokensRedeemed(mojom::Result result) {
    OnContributionProcessed(result == mojom::Result::LEDGER_OK);
  }

  void OnContributionProcessed(bool success) {
    if (!success) {
      // TODO(zenparsing): Log error
      context().LogError(FROM_HERE) << "Unable to redeem contribution tokens";
      return Complete(false);
    }

    // TODO(zenparsing): Do we need to wait for this?
    hold_.OnTokensRedeemed(contribution_.id);

    context()
        .Get<ContributionStore>()
        .SaveContribution(contribution_)
        .Then(ContinueWith(this, &ProcessJob::OnSaved));
  }

  void OnSaved(bool) {
    // TODO(zenparsing): Call the following.
    // ledger_->ledger_client()->OnReconcileComplete
    // ledger_->database()->SaveBalanceReportInfoItem
    Complete(true);
  }

  ContributionTokenType GetContributionTokenType() {
    switch (contribution_.source) {
      case ContributionSource::kBraveVG:
        return ContributionTokenType::kVG;
      case ContributionSource::kBraveSKU:
        return ContributionTokenType::kSKU;
      default:
        NOTREACHED();
        return ContributionTokenType::kVG;
    }
  }

  PaymentVoteType GetVoteType() {
    switch (contribution_.type) {
      case ContributionType::kOneTime:
        return PaymentVoteType::kOneOffTip;
      case ContributionType::kRecurring:
        return PaymentVoteType::kRecurringTip;
      case ContributionType::kAutoContribute:
        return PaymentVoteType::kAutoContribute;
    }
  }

  Contribution contribution_;
  ContributionTokenHold hold_;
  std::unique_ptr<endpoint::PromotionServer> promotion_server_;
};

}  // namespace

const char TokenContributionProcessor::kContextKey[] =
    "token-contribution-processor";

Future<bool> TokenContributionProcessor::ProcessContribution(
    const Contribution& contribution) {
  return context().StartJob<ProcessJob>(contribution);
}

Future<bool> TokenContributionProcessor::ProcessContribution(
    const Contribution& contribution,
    ContributionTokenManager::Hold hold) {
  return context().StartJob<ProcessJob>(contribution, std::move(hold));
}

}  // namespace ledger
