/*
 * File:        http_fetcher.h
 * Module:      orc-core
 * Purpose:     Mockable HTTPS text-fetch transport seam for plugin discovery
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#pragma once

#if defined(ORC_GUI_BUILD) || defined(ORC_CLI_BUILD)
#error \
    "http_fetcher.h is a core-only header. Access remote discovery through ProjectPresenter."
#endif

#include <string>

namespace orc {

/**
 * @brief Result of a single HTTP(S) text GET.
 *
 * Success means the transport completed and the server returned a 2xx status.
 * `body` holds the response payload; `error_message` describes a transport or
 * HTTP failure when `success` is false.
 */
struct HttpFetchResult {
  bool success = false;
  long status_code = 0;  // NOLINT(google-runtime-int): mirrors libcurl API
  std::string body;
  std::string error_message;
};

/**
 * @brief Injectable transport for fetching small text resources over HTTPS.
 *
 * Introduced so plugin-index discovery can be unit-tested without network
 * access: production code uses CurlHttpFetcher; tests inject a stub. Thread
 * safety is implementation-defined; the libcurl-backed default is safe to call
 * from a single thread at a time.
 */
class IHttpFetcher {
 public:
  virtual ~IHttpFetcher() = default;

  /**
   * @brief Perform an HTTPS GET and return the decoded text body.
   * @param url Absolute URL to fetch.
   */
  virtual HttpFetchResult fetch(const std::string& url) const = 0;
};

}  // namespace orc
