/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/payments/get_order_endpoint.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "bat/ledger/internal/core/environment_config.h"
#include "bat/ledger/internal/core/value_converters.h"

namespace ledger {

namespace {

struct ResponseItem {
  std::string id;
  std::string sku;
  int quantity;
  double price;

  static auto FromValue(const base::Value& value) {
    StructValueReader<ResponseItem> r(value);
    r.Read("id", &ResponseItem::id);
    r.Read("sku", &ResponseItem::sku);
    r.Read("quantity", &ResponseItem::quantity);
    r.Read("price", &ResponseItem::price);
    return r.Finish();
  }
};

struct ResponseData {
  std::string id;
  absl::optional<PaymentOrderStatus> status;
  double total_price = 0;
  std::vector<ResponseItem> items;

  static auto FromValue(const base::Value& value) {
    StructValueReader<ResponseData> r(value);
    r.Read("id", &ResponseData::id);
    r.Read("status", &ResponseData::status);
    r.Read("totalPrice", &ResponseData::total_price);
    r.Read("items", &ResponseData::items);
    return r.Finish();
  }
};

}  // namespace

const char GetOrderEndpoint::kContextKey[] = "payments-get-order-endpoint";

URLRequest GetOrderEndpoint::MapRequest(const std::string& order_id) {
  std::string host = context().Get<EnvironmentConfig>().payment_service_host();
  return URLRequest::Get("https://" + host + "/v1/orders/" + order_id);
}

absl::optional<PaymentOrder> GetOrderEndpoint::MapResponse(
    const URLResponse& response) {
  // TODO(zenparsing): This is all duplicated from PostOrderEndpoint.
  if (!response.Succeeded()) {
    context().LogError(FROM_HERE) << "HTTP " << response.status_code();
    return {};
  }

  auto data = ResponseData::FromValue(response.ReadBodyAsJSON());
  if (!data) {
    context().LogError(FROM_HERE) << "Invalid JSON response";
    return {};
  }

  PaymentOrder order;
  order.id = data->id;
  order.total_price = data->total_price;
  if (data->status) {
    order.status = *data->status;
  }

  for (auto& item : data->items) {
    order.items.push_back(PaymentOrderItem{.id = std::move(item.id),
                                           .sku = std::move(item.sku),
                                           .quantity = item.quantity,
                                           .price = item.price});
  }

  return order;
}

}  // namespace ledger
