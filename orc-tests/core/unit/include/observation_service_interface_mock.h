/*
 * File:        observation_service_interface_mock.h
 * Module:      orc-core-tests
 * Purpose:     gMock doubles for IObservationService / IObserverHandle
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef DECODE_ORC_ROOT_OBSERVATION_SERVICE_INTERFACE_MOCK_H
#define DECODE_ORC_ROOT_OBSERVATION_SERVICE_INTERFACE_MOCK_H

#include <gmock/gmock.h>
#include <orc/stage/observation/observation_service_interface.h>

#include <memory>
#include <string>
#include <vector>

// Distinct namespace from the module-under-test so tests may reuse the
// production class names locally without collision.
namespace orc_unit_test {

/**
 * @brief gMock double for the host-owned observer session (IObserverHandle).
 *
 * Inject via MockObservationService::create_observer(); tests assert the
 * per-frame process_frame() call sequence a ported deps class drives.
 */
class MockObserverHandle : public orc::IObserverHandle {
 public:
  MOCK_METHOD(void, process_frame,
              (const orc::VideoFrameRepresentation& representation,
               orc::FrameID frame_id, orc::IObservationContext& context),
              (override));
};

/**
 * @brief gMock double for the plugin-facing IObservationService.
 *
 * Matches the production contract: create_observer() hands back an owning
 * handle (or nullptr for an unknown id) and run_observer() reports whether the
 * id was recognised. Tests wire this in place of
 * plugin::get_observation_service() to exercise a deps class's service
 * interaction without the concrete observers.
 */
class MockObservationService : public orc::IObservationService {
 public:
  MOCK_METHOD(std::vector<orc::ObserverInfo>, available_observers, (),
              (const, override));

  MOCK_METHOD(std::unique_ptr<orc::IObserverHandle>, create_observer,
              (const std::string& observer_id), (const, override));

  MOCK_METHOD(bool, run_observer,
              (const std::string& observer_id,
               const orc::VideoFrameRepresentation& representation,
               orc::FrameID frame_id, orc::IObservationContext& context),
              (const, override));
};

}  // namespace orc_unit_test

#endif  // DECODE_ORC_ROOT_OBSERVATION_SERVICE_INTERFACE_MOCK_H
