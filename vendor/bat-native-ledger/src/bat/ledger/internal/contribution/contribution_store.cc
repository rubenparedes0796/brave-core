/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/contribution/contribution_store.h"

#include <string>
#include <utility>
#include <vector>

#include "bat/ledger/internal/core/bat_ledger_job.h"
#include "bat/ledger/internal/core/sql_store.h"

namespace ledger {

namespace {

const char kLastScheduledContributionKey[] = "last-scheduled-contribution";

class AddVisitJob : public BATLedgerJob<bool> {
 public:
  void Start(const std::string& publisher_id, base::TimeDelta duration) {
    publisher_id_ = publisher_id;
    duration_ = duration;

    static const char kSQL[] = R"sql(
      SELECT visits, duration
      FROM contribution_publisher
      WHERE publisher_id = ?
    )sql";

    context()
        .Get<SQLStore>()
        .Query(kSQL, publisher_id_)
        .Then(ContinueWith(this, &AddVisitJob::OnRead));
  }

 private:
  void OnRead(SQLReader reader) {
    if (!reader.Step()) {
      return Insert();
    }

    int64_t visits = reader.ColumnInt64(0) + 1;
    duration_ += base::Seconds(reader.ColumnDouble(1));

    static const char kSQL[] = R"sql(
      UPDATE contribution_publisher
      SET visits = ?, duration = ?
      WHERE publisher_id = ?
    )sql";

    context()
        .Get<SQLStore>()
        .Run(kSQL, visits, duration_.InSecondsF(), publisher_id_)
        .Then(ContinueWith(this, &AddVisitJob::OnUpdated));
  }

  void Insert() {
    static const char kSQL[] = R"sql(
      INSERT INTO contribution_publisher (publisher_id, visits, duration)
      VALUES (?, ?, ?)
    )sql";

    context()
        .Get<SQLStore>()
        .Run(kSQL, publisher_id_, 1, duration_.InSecondsF())
        .Then(ContinueWith(this, &AddVisitJob::OnUpdated));
  }

  void OnUpdated(SQLReader reader) { Complete(reader.Succeeded()); }

  std::string publisher_id_;
  base::TimeDelta duration_;
};

class GetLastContributionTimeJob : public BATLedgerJob<base::Time> {
 public:
  void Start() {
    static const char kSQL[] = R"sql(
        SELECT value FROM dictionary WHERE key = ?
    )sql";

    return context()
        .Get<SQLStore>()
        .Query(kSQL, kLastScheduledContributionKey)
        .Then(ContinueWith(this, &GetLastContributionTimeJob::OnRead));
  }

  void OnRead(SQLReader reader) {
    if (!reader.Step()) {
      context().Get<ContributionStore>().UpdateLastScheduledContributionTime();
      return Complete(base::Time::Now());
    }

    Complete(SQLStore::ParseTime(reader.ColumnString(0)));
  }
};

}  // namespace

const char ContributionStore::kContextKey[] = "contibution-store";

Future<bool> ContributionStore::SavePendingContribution(
    const std::string& publisher_id,
    double amount) {
  static const char kSQL[] = R"sql(
    INSERT INTO pending_contribution (publisher_id, amount, created_at)
    VALUES (?, ?, ?)
  )sql";

  return context()
      .Get<SQLStore>()
      .Run(kSQL, publisher_id, amount, SQLStore::TimeString())
      .Map(base::BindOnce([](SQLReader reader) { return reader.Succeeded(); }));
}

Future<bool> ContributionStore::SaveContribution(
    const ContributionRequest& contribution) {
  static const char kSQL[] = R"sql(
    INSERT OR REPLACE INTO contribution (contribution_id, contribution_type,
      publisher_id, amount, source, completed_at)
    VALUES (?, ?, ?, ?, ?, ?)
  )sql";

  std::string type_string = StringifyEnum(contribution.type);
  std::string source_string = StringifyEnum(contribution.source);

  return context()
      .Get<SQLStore>()
      .Run(kSQL, contribution.id, type_string, contribution.publisher_id,
           contribution.amount, source_string, SQLStore::TimeString())
      .Map(base::BindOnce([](SQLReader reader) { return reader.Succeeded(); }));
}

Future<bool> ContributionStore::SaveContribution(
    const ContributionRequest& contribution,
    const ExternalWalletTransferResult& transfer_result) {
  static const char kSQL[] = R"sql(
    INSERT OR REPLACE INTO contribution (contribution_id, contribution_type,
      publisher_id, amount, source, external_provider, external_transaction_id,
      completed_at)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?)
  )sql";

  std::string type_string = StringifyEnum(contribution.type);
  std::string source_string = StringifyEnum(contribution.source);
  std::string provider_string = StringifyEnum(transfer_result.provider);

  return context()
      .Get<SQLStore>()
      .Run(kSQL, contribution.id, type_string, contribution.publisher_id,
           contribution.amount, source_string, provider_string,
           transfer_result.transaction_id, SQLStore::TimeString())
      .Map(base::BindOnce([](SQLReader reader) { return reader.Succeeded(); }));
}

Future<bool> ContributionStore::AddPublisherVisit(
    const std::string& publisher_id,
    base::TimeDelta duration) {
  return context().StartJob<AddVisitJob>(publisher_id, duration);
}

Future<std::vector<PublisherActivity>>
ContributionStore::GetPublisherActivity() {
  static const char kSQL[] = R"sql(
    SELECT publisher_id, visits, duration
    FROM contribution_publisher
    WHERE duration > 0 AND auto_contribute_enabled = 1
  )sql";

  return context().Get<SQLStore>().Query(kSQL).Map(
      base::BindOnce([](SQLReader reader) {
        std::vector<PublisherActivity> publishers;
        while (reader.Step()) {
          publishers.push_back(PublisherActivity{
              .publisher_id = reader.ColumnString(0),
              .visits = reader.ColumnInt64(1),
              .duration = base::Seconds(reader.ColumnDouble(2))});
        }
        return publishers;
      }));
}

Future<bool> ContributionStore::ResetPublisherActivity() {
  static const char kSQL[] = R"sql(
    UPDATE contribution_publisher SET visits = 0, duration = 0
  )sql";

  return context().Get<SQLStore>().Run(kSQL).Map(
      base::BindOnce([](SQLReader reader) { return reader.Succeeded(); }));
}

Future<std::vector<RecurringContribution>>
ContributionStore::GetRecurringContributions() {
  static const char kSQL[] = R"sql(
    SELECT publisher_id, recurring_amount
    FROM contribution_publisher
    WHERE recurring_amount > 0
  )sql";

  return context().Get<SQLStore>().Query(kSQL).Map(
      base::BindOnce([](SQLReader reader) {
        std::vector<RecurringContribution> contributions;
        while (reader.Step()) {
          contributions.push_back(
              RecurringContribution{.publisher_id = reader.ColumnString(0),
                                    .amount = reader.ColumnDouble(1)});
        }
        return contributions;
      }));
}

Future<base::Time> ContributionStore::GetLastScheduledContributionTime() {
  return context().StartJob<GetLastContributionTimeJob>();
}

Future<bool> ContributionStore::UpdateLastScheduledContributionTime() {
  static const char kSQL[] = R"sql(
    INSERT OR REPLACE INTO dictionary (key, value) VALUES (?, ?)
  )sql";

  return context()
      .Get<SQLStore>()
      .Run(kSQL, kLastScheduledContributionKey, SQLStore::TimeString())
      .Map(base::BindOnce([](SQLReader reader) { return reader.Succeeded(); }));
}

}  // namespace ledger
