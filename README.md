# sentinel-uav

Sentinel UAV is a hobby robotics project for learning production-minded UAV software with ROS 2, Gazebo, ArduPilot SITL, and C++.

The first goal is not autonomy. The first goal is a reproducible simulation foundation that can be rebuilt, inspected, and explained clearly.

## Current Foundation

The development environment is a VS Code Dev Container based on Ubuntu Noble and ROS 2 Jazzy.

| Component          | Current choice                                                          |
| ------------------ | ----------------------------------------------------------------------- |
| OS                 | Ubuntu 24.04 Noble                                                      |
| ROS 2              | Jazzy                                                                   |
| Simulator          | Gazebo Harmonic / `gz-sim8`                                             |
| Flight stack       | ArduPilot ArduCopter SITL                                               |
| ROS 2 bridge       | MAVROS (`ros-jazzy-mavros`)                                             |
| Gazebo integration | `ardupilot_gazebo` pinned to `082a0fe231f6e63bc8d1598f1cba461d9e2ea7f5` |
| Main language      | C++                                                                     |

## Architecture

The intended simulation path is:

```text
Gazebo Harmonic world/model
        |
        v
ardupilot_gazebo plugin
        |
        v
ArduPilot ArduCopter SITL  <--MAVLink-->  MAVROS
                                               |
                                               v
                                        ROS 2 Jazzy graph
                                               |
                                               v
                                    Sentinel UAV C++ nodes
```

This repository currently contains the environment and starter ROS 2 packages:

```text
src/sentinel_uav_core
src/sentinel_uav_bringup
```

Mission logic will be added only after the simulation stack can be launched and verified end to end.

## Dev Container

Open the repository in VS Code and choose:

```text
Dev Containers: Reopen in Container
```

The container is configured to:

- run as the non-root `ubuntu` user
- avoid requiring a GPU
- avoid blanket `--privileged` mode
- use host networking for ROS 2 / MAVLink / SITL discovery during early development
- expose Gazebo plugin and resource paths for `ardupilot_gazebo`

## Smoke Checks

Run these commands inside the Dev Container.

Check ROS 2:

```bash
ros2 --help >/dev/null && echo ROS2_OK
```

Check Gazebo:

```bash
gz sim --versions
```

Check MAVROS:

```bash
ros2 pkg prefix mavros
```

Check ArduCopter SITL:

```bash
which arducopter
arducopter --help | head -n 3
```

Check the ROS workspace:

```bash
cd /workspaces/sentinel-uav
colcon list
```

Expected package discovery:

```text
sentinel_uav_bringup    src/sentinel_uav_bringup    (ros.ament_cmake)
sentinel_uav_core       src/sentinel_uav_core       (ros.ament_cmake)
```

## Running the Simulation

Open four terminals inside the Dev Container and run each command in order.

**Terminal 1 — Gazebo:**

```bash
gz sim -v4 -r iris_runway.sdf
```

**Terminal 2 — ArduCopter SITL:**

```bash
arducopter --model JSON --defaults /opt/ardupilot_gazebo/config/gazebo-iris-gimbal.parm -I0
```

> `--model JSON` is required. The `ardupilot_gazebo` plugin uses the JSON backend protocol.
> Using `--model gazebo-iris` produces `Incorrect protocol magic` errors and no physics connection.

**Terminal 3 — MAVROS:**

```bash
ros2 run mavros mavros_node --ros-args \
  -p fcu_url:=tcp://127.0.0.1:5760 \
  -p gcs_url:=udp://@127.0.0.1:14550 \
  -p tgt_system:=1 \
  -p tgt_component:=1
```

**Terminal 4 — MAVProxy (manual GCS for validation):**

```bash
mavproxy.py --master tcp:127.0.0.1:5760 --out udp:127.0.0.1:14550
```

> `--out udp:127.0.0.1:14550` forwards MAVLink to MAVROS on port 14550.
> Both MAVProxy and MAVROS can be active simultaneously this way.

Basic flight validation sequence (in MAVProxy):

```
mode guided
arm throttle
takeoff 10
mode land
```

Verify MAVROS topics are live:

```bash
ros2 topic list | grep mavros
ros2 topic echo /mavros/state
```

Run Sentinel M0-M1 bringup nodes (bringup_orchestrator + observer_node):

```bash
cd /workspaces/sentinel-uav
colcon build
source install/setup.bash

ros2 launch sentinel_uav_bringup m0_m1_bringup.launch.py
```

Run Sentinel M2 control skeleton (includes abort hook):

```bash
cd /workspaces/sentinel-uav
colcon build
source install/setup.bash

ros2 launch sentinel_uav_bringup m2_control_abort.launch.py
```

### M2 Abort Hook: What It Does and How to Test It

Purpose:

- This is an immediate safety path for mission cancellation.
- When `/sentinel/mission/abort` receives `true`, `control_node` starts a Hold -> Land flow.
- The flow progress is published on `/sentinel/mission/abort_status` for observability and test evidence.

Expected behavior:

- If Hold and Land services are available, status should move to `abort_completed`.
- If services are unavailable or timeout, status should move to `abort_failed`.
- A second abort trigger while one is active should publish `abort_ignored_in_progress`.

Status sequence contract:

- Main sequence:
        - `abort_requested -> hold_requested -> (hold_succeeded|hold_failed) -> land_requested -> (land_succeeded|land_failed) -> (abort_completed|abort_failed)`
- Diagnostic events may also appear:
        - `hold_service_unavailable`, `hold_request_timeout`, `hold_request_exception`, `hold_command_rejected`
        - `land_service_unavailable`, `land_request_timeout`, `land_request_exception`, `land_command_rejected`

Quick test (failure path, without MAVROS services):

Terminal A:

```bash
cd /workspaces/sentinel-uav
source install/setup.bash
ros2 launch sentinel_uav_bringup m2_control_abort.launch.py
```

Terminal B:

```bash
cd /workspaces/sentinel-uav
source install/setup.bash
ros2 topic echo /sentinel/mission/abort_status
```

Terminal C:

```bash
cd /workspaces/sentinel-uav
source install/setup.bash
ros2 topic pub --once /sentinel/mission/abort std_msgs/msg/Bool '{data: true}'
```

Typical output in this case:

- `abort_requested`
- `hold_requested`
- `hold_service_unavailable`
- `hold_failed`
- `land_requested`
- `land_service_unavailable`
- `land_failed`
- `abort_failed`

Success-path expectation (with MAVROS services ready):

- You should see the same start, then `hold_succeeded`, `land_succeeded`, and final `abort_completed`.

## Next Milestones

1. ~~Add a minimal Gazebo + ArduPilot SITL launch path.~~ ✅
2. ~~Select and integrate ROS 2 bridge (MAVROS over MAVLink).~~ ✅
3. ~~Add a small C++ observer node for vehicle health/state.~~ ✅
4. Add the first simple control milestone: arm, take off, land via ROS 2.
5. Build mission behavior after the basic loop is observable and testable.

## Planning Documents

- Mission roadmap and safety/acceptance plan: `docs/mission-roadmap.md`
- Interface contract (living draft): `docs/interfaces.md`
- Acceptance test matrix and evidence rules: `docs/test-matrix.md`

## PR Checklist Rule

- If behavior, interfaces, thresholds, or tests change, update code and all affected planning documents in the same PR with evidence links.
