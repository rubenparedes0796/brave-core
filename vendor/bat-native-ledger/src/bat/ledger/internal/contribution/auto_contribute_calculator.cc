/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/contribution/auto_contribute_calculator.h"

#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "bat/ledger/internal/core/randomizer.h"

namespace ledger {

namespace {

double ConvertSecondsToScore(double seconds, base::TimeDelta minimum) {
  double c = seconds * 100;
  double a = 15'000 - c;
  double b = 2 * c - 15'000;
  return (-b + std::sqrt((b * b) + (4 * a * c))) / (2 * a);
}

}  // namespace

const char AutoContributeCalculator::kContextKey[] =
    "auto-contribute-calculator";

std::map<std::string, double> AutoContributeCalculator::CalculateWeights(
    const std::vector<PublisherActivity>& publishers,
    int min_visits,
    base::TimeDelta min_duration) {
  std::map<std::string, double> publisher_map;

  // Get total duration for each qualified publisher.
  for (auto& activity : publishers) {
    if (activity.visits >= min_visits && activity.duration >= min_duration) {
      publisher_map[activity.publisher_id] += activity.duration.InSecondsF();
    }
  }

  // Convert durations into "scores".
  double total_score = 0;
  for (auto& pair : publisher_map) {
    double score = ConvertSecondsToScore(pair.second, min_duration);
    pair.second = score;
    total_score += score;
  }

  // Convert "scores" into weights.
  for (auto& pair : publisher_map) {
    pair.second = pair.second / total_score;
  }

  return publisher_map;
}

std::map<std::string, size_t> AutoContributeCalculator::AllocateVotes(
    const std::map<std::string, double>& publisher_weights,
    size_t total_votes) {
  std::map<std::string, size_t> votes;
  for (auto& pair : publisher_weights) {
    votes[pair.first] = 0;
  }

  size_t votes_remaining = total_votes;
  while (votes_remaining > 0) {
    double random01 = context().Get<Randomizer>().Uniform01();
    double upper_bound = 0;
    for (auto& pair : publisher_weights) {
      upper_bound += pair.second;
      if (upper_bound >= random01) {
        votes[pair.first] += 1;
        votes_remaining -= 1;
        break;
      }
    }
  }

  return votes;
}

}  // namespace ledger
