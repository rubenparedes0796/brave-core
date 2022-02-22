/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/browser/fil_tx_manager.h"

#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "brave/components/brave_wallet/browser/hd_keyring.h"
#include "brave/components/brave_wallet/browser/json_rpc_service.h"
#include "brave/components/brave_wallet/browser/keyring_service.h"
#include "brave/components/brave_wallet/browser/pref_names.h"
#include "brave/components/brave_wallet/browser/tx_service.h"
#include "brave/components/brave_wallet/browser/fil_tx_state_manager.h"
#include "brave/components/brave_wallet/browser/fil_tx_manager.h"
#include "brave/components/brave_wallet/common/brave_wallet.mojom.h"
#include "brave/components/brave_wallet/common/hex_utils.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"


namespace brave_wallet {

namespace {

void AddUnapprovedTransactionSuccessCallback(bool* callback_called,
                                             std::string* tx_meta_id,
                                             bool success,
                                             const std::string& id,
                                             const std::string& error_message) {
  EXPECT_TRUE(success);
  EXPECT_FALSE(id.empty());
  EXPECT_TRUE(error_message.empty());
  *callback_called = true;
  *tx_meta_id = id;
}


}  // namespace

class FilTxManagerUnitTest : public testing::Test {
 public:
      FilTxManagerUnitTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        browser_context_(new content::TestBrowserContext()),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &url_loader_factory_)) {}

    void SetUp() override {
      url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
          [&](const network::ResourceRequest& request) {
            url_loader_factory_.ClearResponses();
            base::StringPiece request_string(request.request_body->elements()
                                                ->at(0)
                                                .As<network::DataElementBytes>()
                                                .AsStringPiece());
            absl::optional<base::Value> request_value =
                base::JSONReader::Read(request_string);
            std::string* method = request_value->FindStringKey("method");
            ASSERT_TRUE(method);

            if (*method == "eth_estimateGas") {
              url_loader_factory_.AddResponse(
                  request.url.spec(),
                  "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":"
                  "\"0x00000000000009604\"}");
            }
          }));
  }
  std::string from() {
    return keyring_service_
        ->GetHDKeyringById(brave_wallet::mojom::kFilecoinKeyringId)
        ->GetAddress(0);
  }

  FilTxManager* fil_tx_manager() { return tx_service_->GetFilTxManager(); }

  void AddUnapprovedTransaction(
      mojom::FilTxDataPtr tx_data,
      const std::string& from,
      FilTxManager::AddUnapprovedTransactionCallback callback) {
    fil_tx_manager()->AddUnapprovedTransaction(std::move(tx_data), from,
                                               std::move(callback));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  std::unique_ptr<JsonRpcService> json_rpc_service_;
  std::unique_ptr<KeyringService> keyring_service_;
  std::unique_ptr<TxService> tx_service_;
};

TEST_F(FilTxManagerUnitTest,
       AddUnapprovedTransactionWithoutGasPriceAndGasLimit) {
  auto tx_data =
      mojom::FilTxData::New("0x06", "" /* gas_price */, "" /* gas_limit */,
                         "0xbe862ad9abfe6f22bcb087716c7d89a26051f74c",
                         "0x016345785d8a0000");
  bool callback_called = false;
  std::string tx_meta_id;

  AddUnapprovedTransaction(
      std::move(tx_data), from(),
      base::BindOnce(&AddUnapprovedTransactionSuccessCallback, &callback_called,
                     &tx_meta_id));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_called);
  auto tx_meta = fil_tx_manager()->GetTxForTesting(tx_meta_id);
  EXPECT_TRUE(tx_meta);

}

}  //  namespace brave_wallet
