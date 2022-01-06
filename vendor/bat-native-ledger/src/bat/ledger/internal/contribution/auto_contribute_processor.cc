/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/contribution/auto_contribute_processor.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "bat/ledger/internal/contribution/auto_contribute_calculator.h"
#include "bat/ledger/internal/contribution/contribution_token_manager.h"
#include "bat/ledger/internal/contribution/contribution_token_vendor.h"
#include "bat/ledger/internal/contribution/token_contribution_processor.h"
#include "bat/ledger/internal/core/bat_ledger_job.h"
#include "bat/ledger/internal/core/delay_generator.h"
#include "bat/ledger/internal/core/job_store.h"
#include "bat/ledger/internal/core/value_converters.h"
#include "bat/ledger/internal/external_wallet/external_wallet_manager.h"

namespace ledger {

namespace {

// TODO(zenparsing): Check these values.
constexpr base::TimeDelta kContributionDelay = base::Seconds(45);
constexpr base::TimeDelta kMinRetryDelay = base::Seconds(15);
constexpr base::TimeDelta kMaxRetryDelay = base::Minutes(30);

struct PublisherState {
  std::string publisher_id;
  double weight = 0;
  int votes = 0;
  bool completed = false;

  auto ToValue() const {
    ValueWriter w;
    w.Write("publisher_id", publisher_id);
    w.Write("weight", weight);
    w.Write("votes", votes);
    w.Write("completed", completed);
    return w.Finish();
  }

  static auto FromValue(const base::Value& value) {
    StructValueReader<PublisherState> r(value);
    r.Read("publisher_id", &PublisherState::publisher_id);
    r.Read("weight", &PublisherState::weight);
    r.Read("votes", &PublisherState::votes);
    r.Read("completed", &PublisherState::completed);
    return r.Finish();
  }
};

enum class ACStatus { kPending, kPurchasing, kPurchased, kSending, kComplete };

std::string StringifyEnum(ACStatus value) {
  switch (value) {
    case ACStatus::kPending:
      return "pending";
    case ACStatus::kPurchasing:
      return "purchasing";
    case ACStatus::kPurchased:
      return "purchased";
    case ACStatus::kSending:
      return "sending";
    case ACStatus::kComplete:
      return "complete";
  }
}

void ParseEnum(const std::string& s, absl::optional<ACStatus>* value) {
  DCHECK(value);
  if (s == "pending") {
    *value = ACStatus::kPending;
  } else if (s == "purchasing") {
    *value = ACStatus::kPurchasing;
  } else if (s == "purchased") {
    *value = ACStatus::kPurchased;
  } else if (s == "sending") {
    *value = ACStatus::kSending;
  } else if (s == "complete") {
    *value = ACStatus::kComplete;
  } else {
    *value = {};
  }
}

struct ACState {
  ACStatus status = ACStatus::kPending;
  ContributionSource source;
  std::vector<PublisherState> publishers;
  double amount = 0;
  std::string purchase_job_id;
  std::vector<int64_t> reserved_tokens;

  auto ToValue() const {
    ValueWriter w;
    w.Write("status", status);
    w.Write("source", source);
    w.Write("publishers", publishers);
    w.Write("amount", amount);
    w.Write("purchase_job_id", purchase_job_id);
    w.Write("reserved_tokens", reserved_tokens);
    return w.Finish();
  }

  static auto FromValue(const base::Value& value) {
    StructValueReader<ACState> r(value);
    r.Read("status", &ACState::status);
    r.Read("source", &ACState::source);
    r.Read("publishers", &ACState::publishers);
    r.Read("amount", &ACState::amount);
    r.Read("purchase_job_id", &ACState::purchase_job_id);
    r.Read("reserved_tokens", &ACState::reserved_tokens);
    return r.Finish();
  }
};

class ACJob : public ResumableJob<bool, ACState> {
 public:
  static constexpr char kJobType[] = "auto-contribute";

 protected:
  void Resume() override {
    DCHECK(!state().publishers.empty());

    publisher_iter_ = state().publishers.begin();
    switch (state().status) {
      case ACStatus::kPending:
        return AquireTokens();
      case ACStatus::kPurchasing:
        return CompletePurchase();
      case ACStatus::kPurchased:
        return ReserveTokens();
      case ACStatus::kSending:
        return ReserveAllocatedTokens();
      case ACStatus::kComplete:
        return Complete(true);
    }
  }

  void OnStateInvalid() override {
    context().LogError(FROM_HERE) << "Unable to load state for auto "
                                     "contribute job";
    Complete(false);
  }

 private:
  void AquireTokens() {
    switch (state().source) {
      case ContributionSource::kBraveVG:
        ReserveTokens();
        break;
      case ContributionSource::kBraveSKU:
        context().LogError(FROM_HERE) << "Cannot perform auto contribute with "
                                      << "SKU tokens";
        return Complete(false);
      case ContributionSource::kExternal:
        context().Get<ExternalWalletManager>().GetBalance().Then(
            ContinueWith(this, &ACJob::OnExternalBalanceRead));
        break;
    }
  }

  void OnExternalBalanceRead(absl::optional<double> balance) {
    if (!balance || *balance <= 0) {
      context().LogInfo(FROM_HERE) << "Insufficient funds for auto "
                                      "contribution";
      return Complete(true);
    }

    state().status = ACStatus::kPurchasing;
    state().purchase_job_id =
        context().Get<ContributionTokenVendor>().StartPurchase(
            std::min(state().amount, *balance));

    SaveState();
    CompletePurchase();
  }

  void CompletePurchase() {
    DCHECK(!state().purchase_job_id.empty());
    context()
        .Get<ContributionTokenVendor>()
        .CompletePurchase(state().purchase_job_id)
        .Then(ContinueWith(this, &ACJob::OnTokensPurchased));
  }

  void OnTokensPurchased(bool success) {
    if (!success) {
      context().LogError(FROM_HERE) << "Error purchasing contribution tokens";
      return Complete(false);
    }
    state().status = ACStatus::kPurchased;
    SaveState();
    ReserveTokens();
  }

  void ReserveTokens() {
    context()
        .Get<ContributionTokenManager>()
        .ReserveTokens(GetTokenType(), state().amount)
        .Then(ContinueWith(this, &ACJob::OnTokensReserved));
  }

  void OnTokensReserved(ContributionTokenHold hold) {
    hold_ = std::move(hold);

    // TODO(zenparsing): There's an error condition here. If a purchase worked
    // but we aren't able to hold anything for some reason.
    if (hold_.tokens().empty()) {
      context().LogInfo(FROM_HERE) << "No tokens available for auto "
                                      "contribution";
      return Complete(true);
    }

    for (auto& token : hold_.tokens()) {
      state().reserved_tokens.push_back(token.id);
    }

    // TODO(zenparsing): Before allocating, we need to filter out any publishers
    // that are not registered.

    // TODO(zenparsing): This dance seems a little unwieldy.
    std::map<std::string, double> weights;
    for (auto& publisher_state : state().publishers) {
      weights[publisher_state.publisher_id] = publisher_state.weight;
    }

    auto votes = context().Get<AutoContributeCalculator>().AllocateVotes(
        weights, hold_.tokens().size());

    for (auto& publisher_state : state().publishers) {
      publisher_state.votes = votes[publisher_state.publisher_id];
    }

    state().status = ACStatus::kSending;
    SaveState();

    SendNext();
  }

  void ReserveAllocatedTokens() {
    context()
        .Get<ContributionTokenManager>()
        .ReserveTokens(state().reserved_tokens)
        .Then(ContinueWith(this, &ACJob::OnAllocatedTokensReserved));
  }

  void OnAllocatedTokensReserved(ContributionTokenHold hold) {
    hold_ = std::move(hold);
    SendNext();
  }

  void SendNext() {
    publisher_iter_ = std::find_if(
        publisher_iter_, state().publishers.end(),
        [](const PublisherState& amount) { return !amount.completed; });

    if (publisher_iter_ == state().publishers.end()) {
      state().status = ACStatus::kComplete;
      SaveState();
      return Complete(true);
    }

    auto& publisher_state = *publisher_iter_;
    if (publisher_state.votes == 0) {
      return OnContributionProcessed(true);
    }

    auto publisher_hold = hold_.Split(publisher_state.votes);

    ContributionRequest contribution(
        ContributionType::kAutoContribute, publisher_state.publisher_id,
        GetContributionRequestSource(), publisher_hold.GetTotalValue());

    context()
        .Get<TokenContributionProcessor>()
        .ProcessContribution(contribution, std::move(publisher_hold))
        .Then(ContinueWith(this, &ACJob::OnContributionProcessed));
  }

  void OnContributionProcessed(bool success) {
    if (!success) {
      // TODO(zenparsing): This will retry forever. Is that what we want? What
      // if the tokens have already been spent for some reason? In that case we
      // should just carry on with the next publisher.
      // TODO(zenparsing): Does this delay need to be random?
      context()
          .Get<DelayGenerator>()
          .RandomDelay(FROM_HERE, backoff_.GetNextDelay())
          .DiscardValueThen(ContinueWith(this, &ACJob::SendNext));
      return;
    }

    backoff_.Reset();

    DCHECK(publisher_iter_ != state().publishers.end());
    publisher_iter_->completed = true;
    SaveState();

    context()
        .Get<DelayGenerator>()
        .RandomDelay(FROM_HERE, kContributionDelay)
        .DiscardValueThen(ContinueWith(this, &ACJob::SendNext));
  }

  ContributionTokenType GetTokenType() {
    switch (state().source) {
      case ContributionSource::kBraveVG:
        return ContributionTokenType::kVG;
      case ContributionSource::kBraveSKU:
      case ContributionSource::kExternal:
        return ContributionTokenType::kSKU;
    }
  }

  ContributionSource GetContributionRequestSource() {
    switch (state().source) {
      case ContributionSource::kExternal:
        return ContributionSource::kBraveSKU;
      default:
        return state().source;
    }
  }

  ContributionTokenHold hold_;
  std::vector<PublisherState>::iterator publisher_iter_;
  BackoffDelay backoff_{kMinRetryDelay, kMaxRetryDelay};
};

}  // namespace

const char AutoContributeProcessor::kContextKey[] = "auto-contribute-processor";

Future<bool> AutoContributeProcessor::Initialize() {
  context().Get<JobStore>().ResumeJobs<ACJob>();
  return Future<bool>::Completed(true);
}

Future<bool> AutoContributeProcessor::SendContributions(
    ContributionSource source,
    const std::vector<PublisherActivity>& activity,
    int min_visits,
    base::TimeDelta min_duration,
    double amount) {
  if (amount <= 0) {
    context().LogInfo(FROM_HERE) << "Auto contribute amount is zero";
    return Future<bool>::Completed(true);
  }

  auto weights = context().Get<AutoContributeCalculator>().CalculateWeights(
      activity, min_visits, min_duration);

  if (weights.empty()) {
    context().LogInfo(FROM_HERE) << "No publisher activity for auto contribute";
    return Future<bool>::Completed(true);
  }

  ACState state{.source = source, .amount = amount};
  for (auto& pair : weights) {
    state.publishers.push_back(
        PublisherState{.publisher_id = pair.first, .weight = pair.second});
  }

  return context().Get<JobStore>().StartJobWithState<ACJob>(state);
}

}  // namespace ledger
