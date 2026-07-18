/*
 * File:        cc_sink_output_integration_test.cpp
 * Module:      orc-core-tests
 * Purpose:     Integration test proving CCSinkStageDeps drives the host
 *              observation service and produces real closed-caption output.
 *
 * Unit tests may not touch the file system (see TESTING.md), and the CC sink
 * writes its SCC / plain-text file directly through std::ofstream with no
 * writer seam, so end-to-end output coverage lives here in the integration
 * tier where real files are permitted.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <orc/stage/observation/observation_context.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

#include "cc_sink_stage_deps.h"
#include "mock_video_frame_representation.h"
#include "observation_service_interface_mock.h"

namespace orc_unit_test {
namespace {
using testing::_;
using testing::ByMove;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

constexpr orc::FrameID kFrameCount = 4;

// Stand-in for the host "closed_caption" observer: writes a decodable EIA-608
// pair onto field 0 of every processed frame (data0 = 0x14 caption control
// code, data1 = a printable byte), mirroring what ClosedCaptionObserver would
// deposit for a frame that carries captions.
void write_caption_observations(const orc::VideoFrameRepresentation& /*rep*/,
                                orc::FrameID frame_id,
                                orc::IObservationContext& context) {
  const orc::FieldID field0(frame_id * 2);
  context.set(field0, "closed_caption", "present", true);
  context.set(field0, "closed_caption", "data0", static_cast<int32_t>(0x14));
  context.set(field0, "closed_caption", "data1", static_cast<int32_t>(0x2C));
  context.set(field0, "closed_caption", "parity0_valid", true);
  context.set(field0, "closed_caption", "parity1_valid", true);

  // Field 1 carries no NTSC captions.
  const orc::FieldID field1(frame_id * 2 + 1);
  context.set(field1, "closed_caption", "present", false);
}

// RAII cleanup for a temporary output file.
struct ScopedTempFile {
  std::filesystem::path path;
  explicit ScopedTempFile(const std::string& name)
      : path(std::filesystem::temp_directory_path() / name) {
    std::filesystem::remove(path);
  }
  ~ScopedTempFile() {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }
  std::string str() const { return path.string(); }
};

std::string read_file(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

void configure_ntsc_frames(NiceMock<MockVideoFrameRepresentation>& vfr) {
  ON_CALL(vfr, frame_range())
      .WillByDefault(Return(orc::FrameIDRange{0, kFrameCount - 1}));
  ON_CALL(vfr, has_frame(_)).WillByDefault(Return(true));
  orc::FrameDescriptor desc;
  desc.system = orc::VideoSystem::NTSC;
  ON_CALL(vfr, get_frame_descriptor(_))
      .WillByDefault(Return(std::optional<orc::FrameDescriptor>(desc)));
}
}  // namespace

TEST(CCSinkOutputIntegrationTest, Scc_ProducesCaptionBytesAndReusesOneSession) {
  NiceMock<MockObservationService> service;
  NiceMock<MockVideoFrameRepresentation> vfr;
  configure_ntsc_frames(vfr);

  // The observer session must be created exactly once and reused for every
  // frame (the field-pairing contract Phase 2 called for).
  auto handle = std::make_unique<NiceMock<MockObserverHandle>>();
  auto* handle_ptr = handle.get();
  EXPECT_CALL(*handle_ptr, process_frame(_, _, _))
      .Times(static_cast<int>(kFrameCount))
      .WillRepeatedly(Invoke(&write_caption_observations));
  std::unique_ptr<orc::IObserverHandle> handle_base = std::move(handle);
  EXPECT_CALL(service, create_observer("closed_caption"))
      .Times(1)
      .WillOnce(Return(ByMove(std::move(handle_base))));

  orc::ObservationContext context;
  orc::CCSinkStageDeps deps(&service);
  deps.init(orc::TriggerProgressCallback{}, nullptr);

  ScopedTempFile out("orc_cc_sink_scc_it.scc");
  orc::CCExportOptions options;
  options.output_path = out.str();
  options.export_format = orc::CCExportFormat::SCC;

  const orc::CCExportResult result = deps.export_cc(&vfr, context, options);

  EXPECT_TRUE(result.success) << result.message;
  EXPECT_EQ(result.cc_frames_exported, static_cast<int32_t>(kFrameCount));

  const std::string contents = read_file(out.path);
  EXPECT_THAT(contents, testing::HasSubstr("Scenarist_SCC V1.0"));
  // 0x14 -> odd-parity 0x94, 0x2C -> odd-parity 0x2C.
  EXPECT_THAT(contents, testing::HasSubstr("942c"));
}

TEST(CCSinkOutputIntegrationTest, PlainText_RunsObserverPerFrameAndWritesFile) {
  NiceMock<MockObservationService> service;
  NiceMock<MockVideoFrameRepresentation> vfr;
  configure_ntsc_frames(vfr);

  auto handle = std::make_unique<NiceMock<MockObserverHandle>>();
  auto* handle_ptr = handle.get();
  EXPECT_CALL(*handle_ptr, process_frame(_, _, _))
      .Times(static_cast<int>(kFrameCount))
      .WillRepeatedly(Invoke(&write_caption_observations));
  std::unique_ptr<orc::IObserverHandle> handle_base = std::move(handle);
  EXPECT_CALL(service, create_observer("closed_caption"))
      .Times(1)
      .WillOnce(Return(ByMove(std::move(handle_base))));

  orc::ObservationContext context;
  orc::CCSinkStageDeps deps(&service);
  deps.init(orc::TriggerProgressCallback{}, nullptr);

  ScopedTempFile out("orc_cc_sink_text_it.txt");
  orc::CCExportOptions options;
  options.output_path = out.str();
  options.export_format = orc::CCExportFormat::PLAIN_TEXT;

  const orc::CCExportResult result = deps.export_cc(&vfr, context, options);

  EXPECT_TRUE(result.success) << result.message;
  EXPECT_EQ(result.cc_frames_exported, static_cast<int32_t>(kFrameCount));
  EXPECT_TRUE(std::filesystem::exists(out.path));
}

TEST(CCSinkOutputIntegrationTest,
     NullService_WritesValidHeaderWithoutObserver) {
  NiceMock<MockVideoFrameRepresentation> vfr;
  configure_ntsc_frames(vfr);

  orc::ObservationContext context;
  // Older host: no observation service available.
  orc::CCSinkStageDeps deps(nullptr);
  deps.init(orc::TriggerProgressCallback{}, nullptr);

  ScopedTempFile out("orc_cc_sink_nullsvc_it.scc");
  orc::CCExportOptions options;
  options.output_path = out.str();
  options.export_format = orc::CCExportFormat::SCC;

  const orc::CCExportResult result = deps.export_cc(&vfr, context, options);

  EXPECT_TRUE(result.success) << result.message;
  EXPECT_EQ(result.cc_frames_exported, 0);

  const std::string contents = read_file(out.path);
  EXPECT_THAT(contents, testing::HasSubstr("Scenarist_SCC V1.0"));
}
}  // namespace orc_unit_test
