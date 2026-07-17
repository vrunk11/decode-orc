/*
 * File:        curl_http_fetcher.h
 * Module:      orc-core
 * Purpose:     libcurl-backed IHttpFetcher used for plugin-index discovery
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#pragma once

#if defined(ORC_GUI_BUILD) || defined(ORC_CLI_BUILD)
#error \
    "curl_http_fetcher.h is a core-only header. Access remote discovery through ProjectPresenter."
#endif

#include <string>

#include "http_fetcher.h"

namespace orc {

/**
 * @brief Default IHttpFetcher implementation backed by libcurl.
 *
 * Fetches small text resources (the plugin index) with the same transport
 * configuration used elsewhere in the host: follow-redirects, a 60s timeout,
 * and the decode-orc user agent.
 */
class CurlHttpFetcher : public IHttpFetcher {
 public:
  HttpFetchResult fetch(const std::string& url) const override;
};

}  // namespace orc
