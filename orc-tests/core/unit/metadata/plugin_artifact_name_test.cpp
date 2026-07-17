/*
 * File:        plugin_artifact_name_test.cpp
 * Module:      orc-core unit tests
 * Purpose:     Unit tests for release-artifact name validation, ABI-tag
 *              parsing, and host-ABI-aware asset selection
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "../../../orc/core/include/plugin_artifact_name.h"

#include <gtest/gtest.h>

namespace orc_unit_test {

using orc::parse_release_asset_name;
using orc::ReleaseAssetCandidate;
using orc::select_release_asset;

// --- Validation / parsing --------------------------------------------------

TEST(PluginArtifactNameTest, ParsesLegacyUntaggedName) {
  const auto parsed = parse_release_asset_name("orc-plugin_my-stage_linux.so");
  EXPECT_TRUE(parsed.valid);
  EXPECT_EQ(parsed.ext, "so");
  EXPECT_FALSE(parsed.has_abi);
}

TEST(PluginArtifactNameTest, ParsesAbiTaggedName) {
  const auto parsed =
      parse_release_asset_name("orc-plugin_my-stage_macos_abi8.dylib");
  EXPECT_TRUE(parsed.valid);
  EXPECT_EQ(parsed.ext, "dylib");
  EXPECT_TRUE(parsed.has_abi);
  EXPECT_EQ(parsed.abi, 8U);
}

TEST(PluginArtifactNameTest, ParsesWindowsDll) {
  const auto parsed =
      parse_release_asset_name("orc-plugin_stage_windows_abi12.dll");
  EXPECT_TRUE(parsed.valid);
  EXPECT_EQ(parsed.ext, "dll");
  EXPECT_TRUE(parsed.has_abi);
  EXPECT_EQ(parsed.abi, 12U);
}

TEST(PluginArtifactNameTest, RejectsWrongPrefix) {
  EXPECT_FALSE(orc::is_valid_release_asset_name("plugin_my-stage_linux.so"));
}

TEST(PluginArtifactNameTest, RejectsWrongExtension) {
  EXPECT_FALSE(orc::is_valid_release_asset_name("orc-plugin_stage_linux.txt"));
}

TEST(PluginArtifactNameTest, RejectsMissingPlatformSegment) {
  // Only a single segment after the prefix — no stage/platform split.
  EXPECT_FALSE(orc::is_valid_release_asset_name("orc-plugin_stage.so"));
}

// --- Selection -------------------------------------------------------------

TEST(PluginArtifactNameTest, Select_PrefersHostAbiTaggedAsset) {
  const std::vector<ReleaseAssetCandidate> candidates = {
      {"u/legacy", "orc-plugin_stage_linux.so"},
      {"u/abi8", "orc-plugin_stage_linux_abi8.so"},
      {"u/abi7", "orc-plugin_stage_linux_abi7.so"},
  };
  const auto sel = select_release_asset(candidates, "linux", 8);
  ASSERT_EQ(sel.index, 1);  // the _abi8 asset
  EXPECT_EQ(candidates[sel.index].name, "orc-plugin_stage_linux_abi8.so");
  EXPECT_FALSE(sel.used_legacy_untagged);
  EXPECT_FALSE(sel.abi_mismatch);
  EXPECT_FALSE(sel.missing_platform_token);
}

TEST(PluginArtifactNameTest, Select_FallsBackToLegacyWhenNoAbiTag) {
  const std::vector<ReleaseAssetCandidate> candidates = {
      {"u/legacy", "orc-plugin_stage_linux.so"},
      {"u/abi7", "orc-plugin_stage_linux_abi7.so"},
  };
  const auto sel = select_release_asset(candidates, "linux", 8);
  ASSERT_EQ(sel.index, 0);  // prefer legacy over a wrong-ABI tag
  EXPECT_TRUE(sel.used_legacy_untagged);
  EXPECT_FALSE(sel.abi_mismatch);
}

TEST(PluginArtifactNameTest, Select_MismatchOnlyIsLastResort) {
  const std::vector<ReleaseAssetCandidate> candidates = {
      {"u/abi7", "orc-plugin_stage_linux_abi7.so"},
  };
  const auto sel = select_release_asset(candidates, "linux", 8);
  ASSERT_EQ(sel.index, 0);
  EXPECT_TRUE(sel.abi_mismatch);
  EXPECT_FALSE(sel.used_legacy_untagged);
}

TEST(PluginArtifactNameTest, Select_NoMatchForPlatform) {
  const std::vector<ReleaseAssetCandidate> candidates = {
      {"u/win", "orc-plugin_stage_windows_abi8.dll"},
  };
  // No .so asset for a linux host.
  const auto sel = select_release_asset(candidates, "linux", 8);
  EXPECT_EQ(sel.index, -1);
}

TEST(PluginArtifactNameTest, Select_PrefersPlatformTokenOverAbi) {
  // A wrong-platform-token asset tagged for the host ABI must not beat a
  // correctly-tokened legacy asset: platform correctness dominates.
  const std::vector<ReleaseAssetCandidate> candidates = {
      {"u/tokenless", "orc-plugin_stage_generic_abi8.so"},
      {"u/linux", "orc-plugin_stage_linux.so"},
  };
  const auto sel = select_release_asset(candidates, "linux", 8);
  ASSERT_EQ(sel.index, 1);
  EXPECT_FALSE(sel.missing_platform_token);
  EXPECT_TRUE(sel.used_legacy_untagged);
}

}  // namespace orc_unit_test
