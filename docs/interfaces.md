# Interface Contract

This document defines node-level communication contracts for Sentinel UAV.

## Document Status

- Version: v0.1-draft
- Status: Draft
- Last updated: 2026-05-18
- Validation state: Not yet runtime-validated

## Contract Change Policy

- Contract changes must be made before or together with implementation changes.
- Breaking changes require contract version bump and migration notes.
- Deprecated fields/interfaces should stay documented until removed.

## Contract Versioning

- Current contract version: 0.1.0-draft
- Breaking change rule: bump major/minor and add migration note

## Planned Nodes (Phase 1)

- bringup_orchestrator (planned)
- observer_node (planned)
- control_node (planned)
- mission_fsm_node (planned)
- red_ball_detector_node (planned)
- visual_approach_node (planned)

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
- Suggested message: diagnostic_msgs/DiagnosticArray or custom health message

### /sentinel/target/red_ball

- Direction: publish
- Owner: red_ball_detector_node
- Purpose: target detection output for approach logic
- Suggested payload fields: detected flag, confidence, pixel center, estimated depth

### /sentinel/mission/abort

- Direction: subscribe/service
- Owner: mission_fsm_node
- Purpose: trigger Hold -> Land from any state

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
