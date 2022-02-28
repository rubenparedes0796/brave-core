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

constexpr base::TimeDelta kPendingExpiresAfter = base::Days(90);

const char kLastScheduledContributionKey[] = "last-scheduled-contribution";

auto CreatePublisherInsertCommand(const std::string& publisher_id) {
  static const char kSQL[] = R"sql(
      INSERT OR IGNORE INTO contribution_publisher (publisher_id) VALUES (?)
  )sql";
  return SQLStore::CreateCommand(kSQL, publisher_id);
}

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

Future<std::vector<PendingContribution>>
ContributionStore::GetPendingContributions() {
  static const char kSQL[] = R"sql(
      SELECT pending_contribution_id, publisher_id, amount
      FROM pending_contribution
      WHERE created_at > ?
  )sql";

  base::Time cutoff = base::Time::Now() - kPendingExpiresAfter;

  return context()
      .Get<SQLStore>()
      .Query(kSQL, SQLStore::TimeString(cutoff))
      .Map(base::BindOnce([](SQLReader reader) {
        std::vector<PendingContribution> contributions;
        while (reader.Step()) {
          contributions.push_back(
              PendingContribution{.id = reader.ColumnInt64(0),
                                  .publisher_id = reader.ColumnString(1),
                                  .amount = reader.ColumnDouble(2)});
        }
        return contributions;
      }));
}

Future<bool> ContributionStore::DeletePendingContribution(int64_t id) {
  static const char kSQL[] = R"sql(
    DELETE FROM pending_contribution WHERE pending_contribution_id = ?
  )sql";

  return context().Get<SQLStore>().Run(kSQL, id).Map(
      base::BindOnce([](SQLReader reader) { return reader.Succeeded(); }));
}

Future<bool> ContributionStore::SaveContribution(
    const Contribution& contribution) {
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
    const Contribution& contribution,
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
  static const char kSQL[] = R"sql(
      UPDATE contribution_publisher
      SET visits = visits + 1, duration = duration + ?
      WHERE publisher_id = ?
  )sql";

  return context()
      .Get<SQLStore>()
      .RunTransaction(
          CreatePublisherInsertCommand(publisher_id),
          SQLStore::CreateCommand(kSQL, duration.InSecondsF(), publisher_id))
      .Map(base::BindOnce([](SQLReader reader) { return reader.Succeeded(); }));
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

Future<bool> ContributionStore::SetRecurringContribution(
    const std::string& publisher_id,
    double amount) {
  static const char kSQL[] = R"sql(
      UPDATE contribution_publisher
      SET recurring_amount = ?
      WHERE publisher_id = ?
  )sql";

  if (amount < 0) {
    amount = 0;
  }

  return context()
      .Get<SQLStore>()
      .RunTransaction(CreatePublisherInsertCommand(publisher_id),
                      SQLStore::CreateCommand(kSQL, amount, publisher_id))
      .Map(base::BindOnce([](SQLReader reader) { return reader.Succeeded(); }));
}

Future<bool> ContributionStore::DeleteRecurringContribution(
    const std::string& publisher_id) {
  return SetRecurringContribution(publisher_id, 0);
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
