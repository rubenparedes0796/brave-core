#ifndef BRAVE_COMPONENTS_BRAVE_TODAY_BROWSER_HTML_PARSING_H_
#define BRAVE_COMPONENTS_BRAVE_TODAY_BROWSER_HTML_PARSING_H_

#include <string>
#include <vector>

class GURL;

namespace brave_news {

std::vector<GURL> GetFeedURLsFromHTMLDocument(const std::string &html_body,
    const GURL& html_url);

}  // namespace brave_news

#endif  // BRAVE_COMPONENTS_BRAVE_TODAY_BROWSER_HTML_PARSING_H_