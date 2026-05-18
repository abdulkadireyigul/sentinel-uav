# Acceptance Test Matrix

This document defines how mission acceptance criteria are validated and what evidence is required.

## Document Status

- Version: v0.1-draft
- Status: Draft
- Last updated: 2026-05-18
- Validation state: SITL matrix not executed yet

## Test Governance

- Every criterion must have: setup, steps, measurable pass condition, and evidence.
- If behavior changes, update this matrix in the same change.
- Missing evidence means not validated.

## Evidence Rules

- Required evidence types: terminal logs, ROS topic snapshots, and short run summary.
- Recommended artifact naming: TC-<id>-<date>-<run>.md or .txt
- Evidence location (planned): logs/validation/

## Baseline Validation Targets (v0.1 Accepted)

- Repeats per test case: minimum 5
- Non-critical test target success rate: >=80 percent
- Critical test required success rate (takeoff, abort, land): 100 percent
- Altitude tolerance: +/-0.5 m
- Position tolerance: +/-2.0 m
- Hold-time tolerance: +/-0.5 s
- Approach-distance tolerance: +/-0.2 m

## Matrix

| ID | Criterion | Setup | Procedure | Pass Condition | Required Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- |
| TC-01 | Takeoff | SITL + MAVROS + mission/control stack up | Send arm then publish `/sentinel/control/takeoff_altitude` | Reach target altitude within +/-0.5 m and hold 10 s | altitude topic/log + `/sentinel/control/status` log | Planned |
| TC-02 | Goto Target | TC-01 complete | Publish `/sentinel/control/goto_local_pose` target | Enter 2.0 m radius around target within timeout | pose topic/log + `/sentinel/control/status` log | Planned |
| TC-03 | Panoramic Scan | TC-02 complete | Run scan state | Complete 360 deg scan profile | yaw trend log + scan complete event | Planned |
| TC-04 | Red Ball Detection | TC-03 complete + visible red target | Run detector during scan | Detection confidence >=0.75 and false positives <=10 percent | detection topic/log + sample frames | Planned |
| TC-05 | Visual Approach | TC-04 complete | Execute approach controller | Stop at sqrt(2) m total with ~1.0 m horiz and ~1.0 m vertical components (+/-0.2 m tolerance) | distance estimate log + stop event | Planned |
| TC-06 | Wait and Return/Land | TC-05 complete | Hold 10 s then return and land | Hold completed; vehicle lands at home area | hold timer log + landing confirmation | Planned |
| TC-07 | Abort Hold->Land | Trigger from each mission state | Send abort; capture `/sentinel/mission/abort_status` sequence | Sequence includes `abort_requested -> hold_requested -> (hold_succeeded|hold_failed) -> land_requested -> (land_succeeded|land_failed) -> (abort_completed|abort_failed)` and terminates within timeout budget; FCU loss >2.0 s also triggers abort path | abort_status topic log + mission transition logs + land confirmation | Planned |

## Run Log Template

Use this template for each test run:

- Run ID:
- Date:
- Commit/branch:
- Scenario:
- Result: Pass/Fail
- Notes:
- Evidence paths:

## Open Questions

- Final detector robustness target under low-light and glare scenarios.
- Whether additional evidence type (bag replay summary) should be mandatory.

## Changelog

- 2026-05-18: Initial acceptance matrix draft created.
- 2026-05-18: Added accepted v0.1 numeric tolerances, confidence, and run targets.
- 2026-05-18: Refined TC-07 with deterministic abort status sequence and timeout-aware evidence.
- 2026-05-18: Updated TC-01 and TC-02 procedures to use M2 control primitive interfaces.
