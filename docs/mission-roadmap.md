# Mission Roadmap and Safety Plan

This document defines the first production-minded mission scope, safety rules, and acceptance criteria for Sentinel UAV.

## Document Status

- Version: v0.1-draft
- Status: Draft
- Last updated: 2026-05-18
- Source of truth: validated SITL test evidence and runtime logs

## Document Change Policy

- This document is a living contract, not a fixed spec.
- Any mission/safety behavior change must update this file in the same change set.
- Breaking behavior changes must be recorded under a changelog entry.
- Unknowns are allowed and should be documented explicitly instead of guessed.

## Open Questions

- Whether scan should be yaw-only or combined yaw + small lateral pattern.
- Whether monocular depth is sufficient for final approach in all lighting conditions.

## Mission Objective

Execute one autonomous mission in an open area:

1. Take off from home position.
2. Fly to a configured coordinate.
3. Perform panoramic scan.
4. Detect a red ball.
5. Approach until target-relative distance is reached.
6. Wait 10 seconds.
7. Return and land at home.

Mission complete criteria are intentionally narrow to keep phase 1 small, testable, and repeatable.

## Scope and Non-Goals

### In Scope (Phase 1)

- Single mission type only.
- Open-space operation only.
- Hold-then-land abort behavior.
- SITL-first validation with clear pass/fail thresholds.

### Out of Scope (Phase 1)

- Obstacle avoidance.
- Multi-target search.
- Dynamic replanning.
- Multi-UAV coordination.

## Critical Risks

1. State estimation reliability: position and yaw errors can invalidate mission decisions.
2. Energy budget reliability: voltage drop and reserve miscalculation can break return-to-home safety.
3. Failsafe behavior reliability: link, sensor, or software faults must always resolve into deterministic safe termination.

## Safety Policy

- Abort strategy: Hold -> Land.
- Abort must be callable from every mission state.
- Geofence and min/max altitude limits are mandatory even in open-area tests.
- Mission start requires readiness checks (FCU connected, mode change available, command services available).

## Baseline Numeric Parameters (v0.1 Accepted)

These values are accepted as the initial baseline and may be updated after SITL validation evidence.

### Mission Geometry

- Takeoff altitude: 5.0 m
- Waypoint tolerance: 2.0 m
- Home landing tolerance: 2.0 m
- Approach stop condition: 1.0 m horizontal and 1.0 m vertical (sqrt(2) approx. 1.41 m total)
- Geofence radius: 60.0 m
- Altitude envelope: min 2.0 m, max 20.0 m

### Timeouts and Timing

- Mode switch timeout: 5 s
- Arm timeout: 8 s
- Takeoff completion timeout: 30 s
- Waypoint arrival timeout: 120 s
- Scan timeout: 60 s
- Detect timeout: 90 s
- Approach timeout: 90 s
- Target hold duration: 10 s
- Return and land timeout: 180 s
- Global mission timeout: 600 s

### Motion Limits

- Max horizontal speed: 2.0 m/s
- Max vertical speed: 1.0 m/s
- Max yaw rate: 20 deg/s
- Command publish rate: 10 Hz

### Scan and Detection

- Scan turns: 1 full turn
- Scan angular speed: 15 deg/s
- Scan completion tolerance: 360 deg +/- 10 deg
- Pre-scan stabilization: 3 s
- Detection minimum confidence: 0.75
- Consecutive detection frames: 5
- False-positive budget: <=10 percent in test set
- Lost-target reacquire window: 15 s

### Abort and Energy Safety

- FCU link-loss abort trigger: >2.0 s continuous loss
- Pose staleness abort trigger: >1.0 s without update
- Low-voltage warning (4S baseline): 14.4 V
- Critical-voltage abort (4S baseline): 14.0 V
- Minimum return reserve (capacity): 30 percent

### Acceptance Measurement Tolerances

- Altitude tolerance: +/-0.5 m
- Position tolerance: +/-2.0 m
- Hold-time tolerance: +/-0.5 s
- Approach-distance tolerance: +/-0.2 m

### Test Repetition Targets

- Minimum repeats per test case: 5
- Target success rate for non-critical tests: >=80 percent
- Required success rate for critical tests (takeoff, abort, land): 100 percent

## System Hierarchy (Build Order)

1. Bring-up foundation
2. Observer and health reporting
3. Safety envelope and abort handling
4. Control primitives (arm/takeoff/goto/land)
5. Mission FSM
6. Perception (red-ball detection)
7. Visual approach controller
8. End-to-end mission validation

## Milestones

## M0 - Deterministic Bring-up

- Single reproducible startup sequence.
- Health checks for ROS graph, MAVROS topics, FCU connection, and service readiness.
- Documented failure modes for startup issues.

Status: Completed (v0.1)

## M1 - Observer V1

- Publish or log unified health snapshot (mode, armed, pose, battery, power_state).
- Event logs for state transitions and faults.

Status: Completed (v0.1)

## M2 - Control V1

- Safe arm/takeoff/land primitives with preconditions and timeouts.
- Guided goto waypoint primitive.

## M3 - Mission FSM V1

- State flow: IDLE -> TAKEOFF -> GOTO_TARGET -> SCAN -> DETECT -> APPROACH -> WAIT -> RETURN -> LAND -> COMPLETE.
- Abort path from every state to HOLD_THEN_LAND.

## M4 - Perception and Approach

- Red-ball detection confidence output.
- Camera-based approach until target-relative threshold is met.

## M5 - End-to-End Validation

- Repeatable full-mission SITL runs.
- Pass/fail report for each acceptance criterion.

## Acceptance Criteria (Phase 1)

1. Takeoff: reach target altitude within +/-0.5 m and remain stable for 10 s.
2. Goto: reach waypoint within 2.0 m radius within mission timeout.
3. Scan: complete one full 360 deg panoramic scan.
4. Detect: red-ball detection confidence >=0.75 and false positives <=10 percent for test set.
5. Approach: stop when relative target distance reaches sqrt(2) m with 1.0 m horizontal and 1.0 m vertical components.
6. Wait: hold near target for 10 s without violating safety limits.
7. Return/Land: reach home area and complete landing sequence.
8. Abort: manual or automatic abort from any state leads to Hold -> Land.

## Suggested Learning Topics

- ROS 2 lifecycle and node composition basics.
- MAVROS topic/service model and ArduPilot flight mode semantics.
- Finite-state-machine design patterns for robotics.
- Camera geometry basics (frame transforms, projection, depth assumptions).
- SITL validation strategy and test traceability.

## Working Style

- Keep changes small and reversible.
- Write test scenario before implementing each milestone.
- Update this document at each milestone completion.

## Related Documents

- Interface contract: docs/interfaces.md
- Acceptance and evidence matrix: docs/test-matrix.md

## Changelog

- 2026-05-18: Initial mission roadmap drafted as v0.1-draft.
- 2026-05-18: Baseline numeric parameters accepted and added.
- 2026-05-18: Marked M0 and M1 as completed and aligned observer payload wording.
