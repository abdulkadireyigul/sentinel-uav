# Interface Contract

This document defines node-level communication contracts for Sentinel UAV.

## Document Status

- Version: v0.1-draft
- Status: Draft
- Last updated: 2026-05-18
- Validation state: Runtime-validated for M0/M1 bringup and observer topics

## Contract Change Policy

- Contract changes must be made before or together with implementation changes.
- Breaking changes require contract version bump and migration notes.
- Deprecated fields/interfaces should stay documented until removed.

## Contract Versioning

- Current contract version: 0.1.0-draft
- Breaking change rule: bump major/minor and add migration note

## Planned Nodes (Phase 1)

- bringup_orchestrator (implemented)
- observer_node (implemented)
- control_node (implemented, M2 abort hook skeleton)
- mission_fsm_node (planned)
- red_ball_detector_node (planned)
- visual_approach_node (planned)

## Package Ownership (v0.1)

- sentinel_uav_bringup: launch/config and bringup_orchestrator
- sentinel_uav_core: observer and mission/control runtime nodes

## External Interfaces

### MAVROS Inputs (consumed)

- /mavros/state (vehicle mode, arm state, connection)
- /mavros/local_position/pose (vehicle pose)
- /mavros/battery (battery telemetry)

### MAVROS Services (consumed)

- /mavros/set_mode
- /mavros/cmd/arming
- /mavros/cmd/takeoff
- /mavros/cmd/land

## Internal Interfaces (planned)

### /sentinel/mission/state

- Direction: publish
- Owner: mission_fsm_node
- Purpose: expose current mission phase and transition reason
- Suggested message: custom mission state message or std_msgs/String (temporary)

### /sentinel/health/status

- Direction: publish
- Owner: observer_node
- Purpose: expose readiness, warning, and fault summary
- Current v0.1 payload: std_msgs/String with JSON body
- Planned future format: diagnostic_msgs/DiagnosticArray or custom health message

### /sentinel/target/red_ball

- Direction: publish
- Owner: red_ball_detector_node
- Purpose: target detection output for approach logic
- Suggested payload fields: detected flag, confidence, pixel center, estimated depth

### /sentinel/mission/abort

- Direction: subscribe/service
- Owner: control_node (current), mission_fsm_node (future owner)
- Purpose: trigger Hold -> Land from any state

### /sentinel/mission/abort_status

- Direction: publish
- Owner: control_node
- Purpose: emit deterministic abort flow stage with bounded request handling
- v0.1 state vocabulary:
	- `abort_requested`
	- `hold_requested`
	- `hold_succeeded` or `hold_failed`
	- `land_requested`
	- `land_succeeded` or `land_failed`
	- `abort_completed` or `abort_failed`
- v0.1 event statuses (non-state diagnostics):
	- `abort_ignored_in_progress`
	- `hold_service_unavailable`, `hold_request_timeout`, `hold_request_exception`, `hold_command_rejected`
	- `land_service_unavailable`, `land_request_timeout`, `land_request_exception`, `land_command_rejected`
	- `abort_transition_invalid`
- Valid transition path:
	- `abort_requested -> hold_requested -> (hold_succeeded|hold_failed) -> land_requested -> (land_succeeded|land_failed) -> (abort_completed|abort_failed)`

### /sentinel/control/arm

- Direction: subscribe
- Owner: control_node
- Purpose: trigger arm primitive (`true` only in v0.1)

### /sentinel/control/takeoff_altitude

- Direction: subscribe
- Owner: control_node
- Purpose: trigger takeoff primitive to requested altitude (meters)

### /sentinel/control/goto_local_pose

- Direction: subscribe
- Owner: control_node
- Purpose: trigger local-frame goto primitive to requested pose target

### /sentinel/control/land

- Direction: subscribe
- Owner: control_node
- Purpose: trigger land primitive (`true` only in v0.1)

### /sentinel/control/status

- Direction: publish
- Owner: control_node
- Purpose: emit primitive command lifecycle and precondition outcomes
- v0.1 examples:
	- `arm_requested`, `arm_succeeded`, `arm_request_timeout`
	- `takeoff_requested`, `takeoff_guided_mode_set`, `takeoff_succeeded`
	- `goto_requested`, `goto_active`, `goto_succeeded`, `goto_request_timeout`
	- `land_requested`, `land_succeeded`
	- `<command>_precondition_no_state`, `<command>_precondition_state_stale`, `<command>_precondition_fcu_disconnected`
	- `<command>_precondition_no_pose`, `<command>_precondition_pose_stale`
	- `<command>_rejected_busy`, `<command>_rejected_abort_in_progress`

## Reliability Requirements

- Service calls must have bounded timeout.
- Mission FSM must reject transition if required inputs are stale.
- All critical transitions must emit structured log lines.

## Numeric Runtime Constraints (v0.1 Accepted)

- Command publish rate: 10 Hz
- Mode switch timeout: 5 s
- Arm timeout: 8 s
- Takeoff timeout: 30 s
- Goto timeout: 120 s
- Detect timeout: 90 s
- Approach timeout: 90 s
- Return/land timeout: 180 s
- Global mission timeout: 600 s
- FCU link-loss trigger: >2.0 s
- Pose staleness trigger: >1.0 s

## Safety Envelopes (v0.1 Accepted)

- Geofence radius: 60.0 m
- Min altitude: 2.0 m
- Max altitude: 20.0 m
- Max horizontal speed: 2.0 m/s
- Max vertical speed: 1.0 m/s
- Max yaw rate: 20 deg/s
- Low-voltage warning (4S baseline): 14.4 V
- Critical-voltage abort (4S baseline): 14.0 V
- Return reserve minimum: 30 percent

## Open Questions

- Keep custom messages for v0.1, or start with existing message types for faster iteration?
- Is depth from monocular approximation enough for phase 1 approach stop condition?

## Changelog

- 2026-05-18: Initial interface contract draft created.
- 2026-05-18: Added accepted v0.1 numeric runtime and safety constraints.
- 2026-05-18: Marked M0/M1 interfaces as implemented and runtime-validated.
- 2026-05-18: Added M2 control_node abort hook interfaces.
- 2026-05-18: Hardened abort status contract with explicit state vocabulary and valid transition path.
- 2026-05-18: Added M2 control primitive command/status interfaces for arm/takeoff/land.
- 2026-05-18: Added M2 goto_local_pose interface and goto lifecycle status examples.
