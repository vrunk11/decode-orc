/*
 * File:        plugin_index_client_test.cpp
 * Module:      orc-core unit tests
 * Purpose:     Unit tests for the curated plugin index parser and client
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "../../../orc/core/include/plugin_index_client.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "../../../orc/core/include/plugin_index.h"

namespace orc_unit_test {
namespace {

// Deterministic, network-free transport stub for the index client.
class StubFetcher : public orc::IHttpFetcher {
 public:
  orc::HttpFetchResult result;
  mutable int calls = 0;
  mutable std::string last_url;

  orc::HttpFetchResult fetch(const std::string& url) const override {
    ++calls;
    last_url = url;
    return result;
  }
};

orc::HttpFetchResult ok_body(std::string body) {
  orc::HttpFetchResult r;
  r.success = true;
  r.status_code = 200;
  r.body = std::move(body);
  return r;
}

orc::HttpFetchResult transport_error() {
  orc::HttpFetchResult r;
  r.success = false;
  r.error_message = "Failed to fetch URL: could not resolve host";
  return r;
}

const char* kValidIndex = R"yaml(
registry_schema: 1
plugins:
  - id: acme.deinterlace
    display_name: ACME Deinterlacer
    description: High quality motion-adaptive deinterlacing
    maintainer: ACME Corp
    license_spdx: GPL-3.0-or-later
    source_repo_url: https://github.com/acme/orc-plugin_deinterlace
    tags: [transform, video]
    artifacts:
      - platform: linux
        host_abi: 8
        url: https://example.invalid/orc-plugin_deinterlace_linux_abi8.so
        sha256: 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
        plugin_version: 1.0.0
      - platform: windows
        host_abi: 8
        url: https://example.invalid/orc-plugin_deinterlace_windows_abi8.dll
        sha256: fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210
        plugin_version: 1.0.0
)yaml";

}  // namespace

TEST(PluginIndexParseTest, ReadsStructuredEntriesAndArtifacts) {
  const auto parsed = orc::parse_plugin_index_yaml(kValidIndex);
  ASSERT_TRUE(parsed.success);
  ASSERT_EQ(parsed.index.schema_version, 1);
  ASSERT_EQ(parsed.index.entries.size(), 1U);

  const auto& entry = parsed.index.entries.front();
  EXPECT_EQ(entry.id, "acme.deinterlace");
  EXPECT_EQ(entry.display_name, "ACME Deinterlacer");
  EXPECT_EQ(entry.license_spdx, "GPL-3.0-or-later");
  ASSERT_EQ(entry.tags.size(), 2U);
  EXPECT_EQ(entry.tags[0], "transform");
  ASSERT_EQ(entry.artifacts.size(), 2U);
  EXPECT_EQ(entry.artifacts[0].platform, "linux");
  EXPECT_EQ(entry.artifacts[0].host_abi, 8U);
  EXPECT_EQ(entry.artifacts[0].plugin_version, "1.0.0");
  EXPECT_FALSE(entry.artifacts[0].sha256.empty());
}

TEST(PluginIndexParseTest, IgnoresUnknownFieldsWithinKnownSchemaMajor) {
  // A newer minor revision adds fields the host does not know; they must be
  // ignored, not rejected.
  const char* yaml = R"yaml(
registry_schema: 1
index_generated_at: 2026-07-17T00:00:00Z
plugins:
  - id: acme.tool
    display_name: Tool
    license_spdx: MIT
    future_top_level_field: whatever
    artifacts:
      - platform: linux
        host_abi: 8
        url: https://example.invalid/a.so
        sha256: 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
        future_artifact_field: 42
)yaml";
  const auto parsed = orc::parse_plugin_index_yaml(yaml);
  ASSERT_TRUE(parsed.success);
  ASSERT_EQ(parsed.index.entries.size(), 1U);
  EXPECT_EQ(parsed.index.entries.front().id, "acme.tool");
}

TEST(PluginIndexParseTest, ToleratesNewerSchemaMajorWithWarning) {
  const char* yaml = R"yaml(
registry_schema: 2
plugins:
  - id: acme.tool
    license_spdx: MIT
    artifacts:
      - platform: linux
        host_abi: 8
        url: https://example.invalid/a.so
        sha256: 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
)yaml";
  const auto parsed = orc::parse_plugin_index_yaml(yaml);
  ASSERT_TRUE(parsed.success);
  EXPECT_EQ(parsed.index.schema_version, 2);
  EXPECT_FALSE(parsed.warnings.empty());
}

TEST(PluginIndexParseTest, MalformedYamlFails) {
  const auto parsed = orc::parse_plugin_index_yaml("plugins: [ : : :");
  EXPECT_FALSE(parsed.success);
  EXPECT_FALSE(parsed.error_message.empty());
}

TEST(PluginIndexParseTest, EmptyIndexIsValid) {
  const auto parsed = orc::parse_plugin_index_yaml("registry_schema: 1\n");
  EXPECT_TRUE(parsed.success);
  EXPECT_TRUE(parsed.index.entries.empty());
}

TEST(PluginIndexParseTest, WarnsOnMissingShaAndLicense) {
  const char* yaml = R"yaml(
registry_schema: 1
plugins:
  - id: acme.tool
    artifacts:
      - platform: linux
        host_abi: 8
        url: https://example.invalid/a.so
)yaml";
  const auto parsed = orc::parse_plugin_index_yaml(yaml);
  ASSERT_TRUE(parsed.success);
  EXPECT_GE(parsed.warnings.size(), 2U);  // missing license + missing sha256
}

TEST(PluginIndexResolveTest, ExactPlatformAndAbiMatch) {
  const auto parsed = orc::parse_plugin_index_yaml(kValidIndex);
  const auto resolution = orc::PluginIndexClient::resolve_artifact(
      parsed.index.entries.front(), "linux", 8);
  EXPECT_TRUE(resolution.found);
  EXPECT_FALSE(resolution.abi_incompatible);
  EXPECT_EQ(resolution.artifact.host_abi, 8U);
  EXPECT_EQ(resolution.artifact.platform, "linux");
}

TEST(PluginIndexResolveTest, PlatformPresentButAbiMismatchIsIncompatible) {
  const auto parsed = orc::parse_plugin_index_yaml(kValidIndex);
  const auto resolution = orc::PluginIndexClient::resolve_artifact(
      parsed.index.entries.front(), "linux", 9);
  EXPECT_FALSE(resolution.found);
  EXPECT_TRUE(resolution.platform_available);
  EXPECT_TRUE(resolution.abi_incompatible);
  EXPECT_NE(resolution.message.find("Orc ABI 9"), std::string::npos);
}

TEST(PluginIndexResolveTest, NoBuildForPlatform) {
  const auto parsed = orc::parse_plugin_index_yaml(kValidIndex);
  const auto resolution = orc::PluginIndexClient::resolve_artifact(
      parsed.index.entries.front(), "macos", 8);
  EXPECT_FALSE(resolution.found);
  EXPECT_FALSE(resolution.platform_available);
  EXPECT_FALSE(resolution.abi_incompatible);
}

TEST(PluginIndexResolveTest, AbiAgnosticArtifactIsFallback) {
  const char* yaml = R"yaml(
registry_schema: 1
plugins:
  - id: acme.tool
    license_spdx: MIT
    artifacts:
      - platform: linux
        host_abi: 0
        url: https://example.invalid/a.so
        sha256: 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
)yaml";
  const auto parsed = orc::parse_plugin_index_yaml(yaml);
  const auto resolution = orc::PluginIndexClient::resolve_artifact(
      parsed.index.entries.front(), "linux-x86_64", 8);
  EXPECT_TRUE(resolution.found);
  EXPECT_EQ(resolution.artifact.host_abi, 0U);
}

TEST(PluginIndexSearchTest, CaseInsensitiveAcrossFields) {
  const auto parsed = orc::parse_plugin_index_yaml(kValidIndex);
  EXPECT_EQ(orc::PluginIndexClient::search(parsed.index, "DEINTERLACE").size(),
            1U);
  EXPECT_EQ(orc::PluginIndexClient::search(parsed.index, "motion").size(), 1U);
  EXPECT_EQ(orc::PluginIndexClient::search(parsed.index, "video").size(), 1U);
  EXPECT_EQ(orc::PluginIndexClient::search(parsed.index, "nomatch").size(), 0U);
  EXPECT_EQ(orc::PluginIndexClient::search(parsed.index, "").size(), 1U);
}

TEST(PluginIndexSearchTest, FindByExactId) {
  const auto parsed = orc::parse_plugin_index_yaml(kValidIndex);
  EXPECT_NE(orc::PluginIndexClient::find(parsed.index, "acme.deinterlace"),
            nullptr);
  EXPECT_EQ(orc::PluginIndexClient::find(parsed.index, "missing"), nullptr);
}

TEST(PluginIndexClientTest, FetchSuccessParsesAndWritesCache) {
  StubFetcher fetcher;
  fetcher.result = ok_body(kValidIndex);
  std::optional<std::string> saved;
  orc::PluginIndexClient client(
      fetcher, []() -> std::optional<std::string> { return std::nullopt; },
      [&saved](const std::string& body) { saved = body; });

  const auto outcome = client.refresh("https://example.invalid/index.yaml");
  EXPECT_TRUE(outcome.success);
  EXPECT_FALSE(outcome.from_cache);
  EXPECT_FALSE(outcome.offline);
  ASSERT_EQ(outcome.index.entries.size(), 1U);
  ASSERT_TRUE(saved.has_value());
  EXPECT_EQ(*saved, kValidIndex);
  EXPECT_EQ(fetcher.last_url, "https://example.invalid/index.yaml");
}

TEST(PluginIndexClientTest, NetworkFailureFallsBackToCache) {
  StubFetcher fetcher;
  fetcher.result = transport_error();
  bool saver_called = false;
  orc::PluginIndexClient client(
      fetcher,
      []() -> std::optional<std::string> { return std::string(kValidIndex); },
      [&saver_called](const std::string&) { saver_called = true; });

  const auto outcome = client.refresh("https://example.invalid/index.yaml");
  EXPECT_TRUE(outcome.success);
  EXPECT_TRUE(outcome.from_cache);
  EXPECT_TRUE(outcome.offline);
  ASSERT_EQ(outcome.index.entries.size(), 1U);
  EXPECT_FALSE(saver_called);  // never overwrite cache from a failed fetch
}

TEST(PluginIndexClientTest, NetworkFailureAndNoCacheReportsError) {
  StubFetcher fetcher;
  fetcher.result = transport_error();
  orc::PluginIndexClient client(
      fetcher, []() -> std::optional<std::string> { return std::nullopt; },
      [](const std::string&) {});

  const auto outcome = client.refresh("https://example.invalid/index.yaml");
  EXPECT_FALSE(outcome.success);
  EXPECT_TRUE(outcome.offline);
  EXPECT_FALSE(outcome.error_message.empty());
}

TEST(PluginIndexClientTest, UnparseableFetchFallsBackToCache) {
  StubFetcher fetcher;
  fetcher.result = ok_body("plugins: [ : : :");  // malformed
  orc::PluginIndexClient client(
      fetcher,
      []() -> std::optional<std::string> { return std::string(kValidIndex); },
      [](const std::string&) {});

  const auto outcome = client.refresh("https://example.invalid/index.yaml");
  EXPECT_TRUE(outcome.success);
  EXPECT_TRUE(outcome.from_cache);
  ASSERT_EQ(outcome.index.entries.size(), 1U);
}

}  // namespace orc_unit_test
