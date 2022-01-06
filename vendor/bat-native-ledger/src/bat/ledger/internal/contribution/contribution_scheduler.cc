/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/contribution/contribution_scheduler.h"

#include <algorithm>
#include <string>
#include <vector>

#include "bat/ledger/internal/contribution/auto_contribute_processor.h"
#include "bat/ledger/internal/contribution/contribution_data.h"
#include "bat/ledger/internal/contribution/contribution_router.h"
#include "bat/ledger/internal/contribution/contribution_store.h"
#include "bat/ledger/internal/core/bat_ledger_job.h"
#include "bat/ledger/internal/core/delay_generator.h"
#include "bat/ledger/internal/core/job_store.h"
#include "bat/ledger/internal/core/user_prefs.h"
#include "bat/ledger/internal/core/value_converters.h"
#include "bat/ledger/internal/ledger_impl.h"

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

// TODO(zenparsing): It's a little odd how we use this struct only to provide a
// serializable version of |PublisherActivity|. There are two other options:
// 1. Make |PublisherActivity| serializable, or 2. Provide mapping functions to
// |StructValueReader|.
struct ActivityState {
  std::string publisher_id;
  int64_t visits = 0;
  double duration = 0;

  auto ToValue() const {
    ValueWriter w;
    w.Write("publisher_id", publisher_id);
    w.Write("visits", visits);
    w.Write("duration", duration);
    return w.Finish();
  }

  static auto FromValue(const base::Value& value) {
    StructValueReader<ActivityState> r(value);
    r.Read("publisher_id", &ActivityState::publisher_id);
    r.Read("visits", &ActivityState::visits);
    r.Read("duration", &ActivityState::duration);
    return r.Finish();
  }
};

struct ScheduledContributionState {
  std::vector<RecurringContributionState> contributions;
  std::vector<ActivityState> activity;

  auto ToValue() const {
    ValueWriter w;
    w.Write("contributions", contributions);
    w.Write("activity", activity);
    return w.Finish();
  }

  static auto FromValue(const base::Value& value) {
    StructValueReader<ScheduledContributionState> r(value);
    r.Read("contributions", &ScheduledContributionState::contributions);
    r.Read("activity", &ScheduledContributionState::activity);
    return r.Finish();
  }
};

class ScheduledContributionJob
    : public ResumableJob<bool, ScheduledContributionState> {
 public:
  static constexpr char kJobType[] = "scheduled-contribution";

 protected:
  void Resume() override {
    contribution_iter_ = state().contributions.begin();
    SendNext();
  }

  void OnStateInvalid() override { Complete(false); }

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
        .Then(
            ContinueWith(this, &ScheduledContributionJob::OnContributionSent));
  }

  void OnContributionSent(bool success) {
    if (!success) {
      // TODO(zenparsing): Comment explaining why we don't fail here.
      context().LogError(FROM_HERE) << "Unable to send recurring contribution";
    }

    contribution_iter_->completed = true;
    SaveState();
    SendNextAfterDelay();
  }

  void SendNextAfterDelay() {
    // TODO(zenparsing): RandomDelay?
    context()
        .Get<DelayGenerator>()
        .Delay(FROM_HERE, kContributionDelay)
        .DiscardValueThen(
            ContinueWith(this, &ScheduledContributionJob::SendNext));
  }

  void StartAutoContribute() {
    context().Get<ContributionStore>().GetPublisherActivity().Then(
        ContinueWith(this, &ScheduledContributionJob::OnActivityRead));
  }

  void OnActivityRead(std::vector<PublisherActivity> activity) {
    context().Get<ContributionStore>().ResetPublisherActivity();

    for (auto& entry : activity) {
      state().activity.push_back(
          ActivityState{.publisher_id = entry.publisher_id,
                        .visits = entry.visits,
                        .duration = entry.duration.InSecondsF()});
    }

    SaveState();

    auto& prefs = context().Get<UserPrefs>();

    if (!prefs.ac_enabled()) {
      context().LogVerbose(FROM_HERE) << "Auto contribute not enabled";
      return Complete(true);
    }

    if (!context().options().auto_contribute_allowed) {
      context().LogVerbose(FROM_HERE)
          << "Auto contribute not allowed for this client";
      return Complete(true);
    }

    auto source = context().Get<ContributionRouter>().GetCurrentSource();
    double ac_amount = prefs.ac_amount();

    // TODO(zenparsing): Remove the dependency on state interface.
    if (ac_amount <= 0) {
      ac_amount = context().GetLedgerImpl()->state()->GetAutoContributeChoice();
    }

    context().Get<AutoContributeProcessor>().SendContributions(
        source, activity, prefs.ac_minimum_visits(),
        prefs.ac_minimum_duration(), ac_amount);

    // TODO(zenparsing): Note about why we don't wait for AC.
    Complete(true);
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
        .Then(ContinueWith(this, &SchedulerJob::OnDelayElapsed));
  }

  void OnDelayElapsed(bool) {
    context().Get<ContributionStore>().GetRecurringContributions().Then(
        ContinueWith(this, &SchedulerJob::OnContributionsRead));
  }

  void OnContributionsRead(std::vector<RecurringContribution> contributions) {
    context().Get<ContributionStore>().UpdateLastScheduledContributionTime();

    ScheduledContributionState state;
    for (auto& contribution : contributions) {
      state.contributions.push_back(
          RecurringContributionState{.publisher_id = contribution.publisher_id,
                                     .amount = contribution.amount});
    }

    // TODO(zenparsing): Should there only ever be one of these jobs running at
    // a time? If we did want that, we would store a flag in this job.
    context().LogVerbose(FROM_HERE) << "Starting recurring contributions";

    context().Get<JobStore>().StartJobWithState<ScheduledContributionJob>(
        state);

    ScheduleNext();
  }
};

}  // namespace

const char ContributionScheduler::kContextKey[] = "contribution-scheduler";

Future<bool> ContributionScheduler::Initialize() {
  context().Get<JobStore>().ResumeJobs<ScheduledContributionJob>();
  context().StartJob<SchedulerJob>();
  return Future<bool>::Completed(true);
}

}  // namespace ledger
