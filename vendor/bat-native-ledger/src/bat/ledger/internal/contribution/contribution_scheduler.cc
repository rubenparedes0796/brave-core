/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/contribution/contribution_scheduler.h"

#include <algorithm>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "bat/ledger/internal/contribution/auto_contribute_processor.h"
#include "bat/ledger/internal/contribution/contribution_data.h"
#include "bat/ledger/internal/contribution/contribution_router.h"
#include "bat/ledger/internal/contribution/contribution_store.h"
#include "bat/ledger/internal/core/bat_ledger_job.h"
#include "bat/ledger/internal/core/delay_generator.h"
#include "bat/ledger/internal/core/future_join.h"
#include "bat/ledger/internal/core/job_store.h"
#include "bat/ledger/internal/core/user_prefs.h"
#include "bat/ledger/internal/core/value_converters.h"
#include "bat/ledger/internal/ledger_impl.h"
#include "bat/ledger/internal/publisher/publisher_service.h"

namespace ledger {

namespace {

// TODO(zenparsing): Check this. Consider making this an environment config
// instead, for easy manual testing in development or staging.
constexpr base::TimeDelta kContributionDelay = base::Seconds(45);

struct RecurringContributionState {
  std::string publisher_id;
  double amount = 0;
  bool completed = false;

  auto ToValue() const {
    ValueWriter w;
    w.Write("publisher_id", publisher_id);
    w.Write("amount", amount);
    w.Write("completed", completed);
    return w.Finish();
  }

  static auto FromValue(const base::Value& value) {
    StructValueReader<RecurringContributionState> r(value);
    r.Read("publisher_id", &RecurringContributionState::publisher_id);
    r.Read("amount", &RecurringContributionState::amount);
    r.Read("completed", &RecurringContributionState::completed);
    return r.Finish();
  }
};

struct ScheduledContributionState {
  std::vector<RecurringContributionState> contributions;
  std::vector<PublisherActivity> activity;
  std::string error;

  auto ToValue() const {
    ValueWriter w;
    w.Write("contributions", contributions);
    w.Write("activity", activity);
    w.Write("error", error);
    return w.Finish();
  }

  static auto FromValue(const base::Value& value) {
    StructValueReader<ScheduledContributionState> r(value);
    r.Read("contributions", &ScheduledContributionState::contributions);
    r.Read("activity", &ScheduledContributionState::activity);
    r.Read("error", &ScheduledContributionState::error);
    return r.Finish();
  }
};

class ContributionJob : public ResumableJob<bool, ScheduledContributionState> {
 public:
  static constexpr char kJobType[] = "scheduled-contribution";

 protected:
  void Resume() override {
    contribution_iter_ = state().contributions.begin();
    SendNext();
  }

  void OnStateInvalid() override { CompleteWithError("Invalid job state"); }

 private:
  void SendNext() {
    contribution_iter_ =
        std::find_if(contribution_iter_, state().contributions.end(),
                     [](const RecurringContributionState& contribution) {
                       return !contribution.completed;
                     });

    if (contribution_iter_ == state().contributions.end()) {
      return StartAutoContribute();
    }

    context()
        .Get<ContributionRouter>()
        .SendContribution(ContributionType::kRecurring,
                          contribution_iter_->publisher_id,
                          contribution_iter_->amount)
        .Then(ContinueWith(this, &ContributionJob::OnContributionSent));
  }

  void OnContributionSent(bool success) {
    if (!success) {
      // If we are unable to send this contribution for any reason, assume that
      // the failure is unrecoverable (e.g. the publisher is not registered or
      // verified with a matching wallet provider) and continue on with the next
      // recurring contribution.
      context().LogError(FROM_HERE) << "Unable to send recurring contribution";
    }

    contribution_iter_->completed = true;
    SaveState();
    SendNextAfterDelay();
  }

  void SendNextAfterDelay() {
    context()
        .Get<DelayGenerator>()
        .RandomDelay(FROM_HERE, kContributionDelay)
        .DiscardValueThen(ContinueWith(this, &ContributionJob::SendNext));
  }

  void StartAutoContribute() {
    if (!context().Get<UserPrefs>().ac_enabled()) {
      context().LogVerbose(FROM_HERE) << "Auto contribute is not enabled";
      return Complete(true);
    }

    if (!context().options().auto_contribute_allowed) {
      context().LogVerbose(FROM_HERE)
          << "Auto contribute is not allowed for this client";
      return Complete(true);
    }

    // Load publisher data for each publisher that is in the activity list.
    // Publishers will be removed from the activity list if they are not yet
    // registered.
    std::vector<std::string> publisher_ids;
    for (auto& entry : state().activity) {
      publisher_ids.push_back(entry.publisher_id);
    }

    context()
        .Get<PublisherService>()
        .GetPublishers(publisher_ids)
        .Then(ContinueWith(this, &ContributionJob::OnPublishersLoaded));
  }

  void OnPublishersLoaded(std::map<std::string, Publisher> publishers) {
    std::vector<PublisherActivity> filtered_activity;
    for (auto& entry : state().activity) {
      auto iter = publishers.find(entry.publisher_id);
      if (iter != publishers.end() && iter->second.registered) {
        filtered_activity.push_back(entry);
      }
    }

    auto& prefs = context().Get<UserPrefs>();
    auto source = context().Get<ContributionRouter>().GetCurrentSource();

    context().Get<AutoContributeProcessor>().SendContributions(
        source, filtered_activity, prefs.ac_minimum_visits(),
        prefs.ac_minimum_duration(), GetAutoContributeAmount());

    // Auto-contribute is an independent process that maintains its own
    // resumable state. Once we've started AC this job is complete.
    Complete(true);
  }

  double GetAutoContributeAmount() {
    double ac_amount = context().Get<UserPrefs>().ac_amount();
    if (ac_amount > 0) {
      return ac_amount;
    }

    // TODO(zenparsing): Remove the dependency on state interface. This
    // information actually comes from the Rewards backend configuration. We'll
    // need a new service to provide us with this data.
    return context().GetLedgerImpl()->state()->GetAutoContributeChoice();
  }

  void CompleteWithError(const std::string& error) {
    context().LogError(FROM_HERE) << error;
    state().error = error;
    Complete(false);
  }

  std::vector<RecurringContributionState>::iterator contribution_iter_;
};

class SchedulerJob : public BATLedgerJob<bool> {
 public:
  void Start() { ScheduleNext(); }

 private:
  void ScheduleNext() {
    context().Get<ContributionStore>().GetLastScheduledContributionTime().Then(
        ContinueWith(this, &SchedulerJob::OnLastTimeRead));
  }

  void OnLastTimeRead(base::Time time) {
    // TODO(zenparsing): I would like to remove this option, and instead provide
    // a way to trigger a scheduled contribution manually.
    base::Time next = time + context().options().contribution_interval;
    context()
        .Get<DelayGenerator>()
        .Delay(FROM_HERE, next - base::Time::Now())
        .DiscardValueThen(ContinueWith(this, &SchedulerJob::OnDelayElapsed));
  }

  void OnDelayElapsed() {
    auto& store = context().Get<ContributionStore>();
    JoinFutures(store.GetRecurringContributions(), store.GetPublisherActivity())
        .Then(ContinueWith(this, &SchedulerJob::OnStoreRead));
  }

  void OnStoreRead(std::tuple<std::vector<RecurringContribution>,
                              std::vector<PublisherActivity>> data) {
    auto& [contributions, activity] = data;

    ScheduledContributionState state;

    for (auto& contribution : contributions) {
      state.contributions.push_back(
          {.publisher_id = std::move(contribution.publisher_id),
           .amount = contribution.amount});
    }

    state.activity = std::move(activity);

    auto& store = context().Get<ContributionStore>();
    store.UpdateLastScheduledContributionTime();
    store.ResetPublisherActivity();

    context().LogVerbose(FROM_HERE) << "Starting recurring contributions";

    // TODO(zenparsing): Should there only ever be one of these jobs running at
    // a time? If we did want that, we would store a flag in this job (or wait
    // for the contribution job to complete).
    context().Get<JobStore>().StartJobWithState<ContributionJob>(state);

    ScheduleNext();
  }
};

}  // namespace

const char ContributionScheduler::kContextKey[] = "contribution-scheduler";

Future<bool> ContributionScheduler::Initialize() {
  context().Get<JobStore>().ResumeJobs<ContributionJob>();
  context().StartJob<SchedulerJob>();
  return Future<bool>::Completed(true);
}

}  // namespace ledger
