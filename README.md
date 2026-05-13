# sentinel-uav

Sentinel UAV is a hobby robotics project for learning production-minded UAV software with ROS 2, Gazebo, ArduPilot SITL, C++, and DDS.

The first goal is not autonomy. The first goal is a reproducible simulation foundation that can be rebuilt, inspected, and explained clearly.

## Current Foundation

The development environment is a VS Code Dev Container based on Ubuntu Noble and ROS 2 Jazzy.

| Component          | Current choice                                                          |
| ------------------ | ----------------------------------------------------------------------- |
| OS                 | Ubuntu 24.04 Noble                                                      |
| ROS 2              | Jazzy                                                                   |
| Simulator          | Gazebo Harmonic / `gz-sim8`                                             |
| Flight stack       | ArduPilot ArduCopter SITL                                               |
| ROS-DDS bridge     | Micro XRCE-DDS Agent `v2.4.3`                                           |
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
ArduPilot ArduCopter SITL
        |
        v
Micro XRCE-DDS Agent
        |
        v
ROS 2 Jazzy graph
        |
        v
Sentinel UAV C++ nodes
```

This repository currently contains the environment and an empty starter ROS 2 package:

```text
src/sentinel_uav_core
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
- use host networking for ROS 2 / DDS / SITL discovery during early development
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

Check Micro XRCE-DDS Agent:

```bash
MicroXRCEAgent --help | head -n 3
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
sentinel_uav_core    src/sentinel_uav_core    (ros.ament_cmake)
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

**Terminal 3 — Micro XRCE-DDS Agent:**

```bash
MicroXRCEAgent udp4 -p 2019
```

**Terminal 4 — MAVProxy (manual GCS for validation):**

```bash
mavproxy.py --master tcp:127.0.0.1:5760
```

Basic flight validation sequence:

```
mode guided
arm throttle
takeoff 10
mode land
```

## Next Milestones

1. ~~Add a minimal Gazebo + ArduPilot SITL launch path.~~ ✅
2. Start Micro XRCE-DDS Agent and verify ROS 2 topics from SITL.
3. Add a small C++ observer node for vehicle health/state.
4. Add the first simple control milestone: arm, take off, land.
5. Build mission behavior after the basic loop is observable and testable.
