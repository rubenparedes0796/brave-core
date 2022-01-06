/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This ContributionSource Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/rewards_wallet/anonymous_wallet_manager.h"

#include <memory>

#include "bat/ledger/internal/core/bat_ledger_job.h"
#include "bat/ledger/internal/endpoint/promotion/promotion_server.h"

namespace ledger {

namespace {

class GetBalanceJob : public BATLedgerJob<absl::optional<double>> {
 public:
  void Start() {
    promotion_server_.reset(
        new endpoint::PromotionServer(context().GetLedgerImpl()));

    promotion_server_->get_wallet_balance()->Request(
        CreateLambdaCallback(this, &GetBalanceJob::OnResponse));
  }

 private:
  void OnResponse(mojom::Result result, mojom::BalancePtr balance) {
    // TODO(zenparsing): The server will return a 400 error if the user's wallet
    // doesn't have an anonymous account. How do we work around that?
    /*
    if (result != mojom::Result::LEDGER_OK || !balance)
      return Complete({});
    */
    if (result != mojom::Result::LEDGER_OK || !balance)
      return Complete(0);

    return Complete(balance->total);
  }

  std::unique_ptr<endpoint::PromotionServer> promotion_server_;
};

}  // namespace

const char AnonymousWalletManager::kContextKey[] = "anonymous-wallet-manager";

Future<absl::optional<double>> AnonymousWalletManager::GetBalance() {
  return context().StartJob<GetBalanceJob>();
}

}  // namespace ledger
