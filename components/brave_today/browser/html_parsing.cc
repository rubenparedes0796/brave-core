// Copyright (c) 2022 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at http://mozilla.org/MPL/2.0/.

#include "brave/components/brave_today/browser/html_parsing.h"

#include <regex>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"

namespace brave_news {

namespace {

constexpr auto kSupportedFeedTypes = base::MakeFixedFlatSet<base::StringPiece>(
    {"application/rss+xml", "application/atom+xml", "application/xml",
     "application/rss+atom", "application/json"});

constexpr auto kSupportedRels = base::MakeFixedFlatSet<base::StringPiece>(
    {"alternate", "service.feed"});

}  // namespace

std::vector<GURL> GetFeedURLsFromHTMLDocument(
    const std::string &html_body, const GURL& html_url) {
  VLOG(1) << "GetFeedURLsFromHTMLDocument";
  std::regex link_regex("(<\\s*link [^>]+>)",
      std::regex_constants::ECMAScript | std::regex_constants::icase);
  auto links_begin =
      std::sregex_iterator(html_body.begin(), html_body.end(), link_regex);
  auto links_end = std::sregex_iterator();
  std::vector<GURL> results;
  for (auto i = links_begin; i != links_end; i++) {
    std::string link_text = i->str();
    VLOG(1) << "Found link: " << link_text;
    std::regex rel_extract("rel=\"([^\"]*)\"",
        std::regex_constants::ECMAScript | std::regex_constants::icase);
    std::smatch rel_matches;
    if (!std::regex_search(link_text, rel_matches, rel_extract) || rel_matches.size() != 2u) {
      VLOG(1) << "no valid matching rel: " << rel_matches.size();
      continue;
    }
    auto rel = rel_matches[1].str();
    if (!base::IsStringASCII(rel) || !base::Contains(kSupportedRels, rel)) {
      VLOG(1) << "not valid rel: " << rel;
      continue;
    }
    std::regex type_extract("type=\"([^\"]+)\"",
        std::regex_constants::ECMAScript | std::regex_constants::icase);
    std::smatch type_matches;
    if (!std::regex_search(link_text, type_matches, type_extract) || type_matches.size() != 2u) {
      VLOG(1) << "no valid matching type: " << type_matches.size();
      continue;
    }
    auto content_type = type_matches[1].str();
    if (!base::IsStringASCII(content_type) ||
        !base::Contains(kSupportedFeedTypes, content_type)) {
      VLOG(1) << "not valid type: " << content_type;
      continue;
    }
    std::regex href_extract("href=\"([^\"]+)\"",
        std::regex_constants::ECMAScript | std::regex_constants::icase);
    std::smatch href_matches;
    if (!std::regex_search(link_text, href_matches, href_extract) || href_matches.size() != 2u) {
       VLOG(1) << "no valid matching href: " << href_matches.size();
       continue;
    }
    auto href = href_matches[1].str();
    if (href.empty() || !base::IsStringASCII(href)) {
      VLOG(1) << "not valid href: " << href;
      continue;
    }
    auto feed_url = html_url.Resolve(href);
    if (feed_url.is_valid()) {
      results.emplace_back(feed_url);
    }
  }
  return results;
}

}  // namespace brave_news