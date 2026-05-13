# We use the 'desktop-full' image to avoid downloading heavy GUI dependencies (like Gazebo and Qt)
# during the 'apt-get' phase, which previously caused network timeouts and MTU bridge issues.
FROM osrf/ros:jazzy-desktop-full

ENV DEBIAN_FRONTEND=noninteractive

# Install essential C++ build tools and ROS2 dependency managers (including rosdep).
RUN apt-get update && apt-get install -y \
    python3-colcon-common-extensions \
    python3-rosdep \
    git \
    build-essential \
    cmake \
    && rm -rf /var/lib/apt/lists/*

# Initialize and update rosdep so we can easily install missing dependencies for our custom packages later.
# '|| true' is used to prevent build failure if rosdep is already initialized by the base image.
RUN rosdep init || true \
    && rosdep update || true

# --- MAVROS ---
# MAVROS is the ROS 2 bridge between MAVLink (ArduPilot SITL) and the ROS 2 graph.
# It translates MAVLink telemetry and commands into standard ROS 2 topics and services,
# allowing our C++ nodes to control the vehicle without a custom MAVLink implementation.
#
# 'mavros-extras' provides additional plugins (e.g. visual odometry, companion process status).
RUN apt-get update && apt-get install -y \
    ros-jazzy-mavros \
    ros-jazzy-mavros-extras \
    && rm -rf /var/lib/apt/lists/*

# GeographicLib datasets are required by MAVROS for geodetic transformations (e.g. EKF origin).
# install_geographiclib_datasets.sh is bundled with the mavros package.
RUN /opt/ros/jazzy/lib/mavros/install_geographiclib_datasets.sh

# Note on Workspace: We DO NOT explicitly create a /ros2_ws here.
# VS Code DevContainers automatically mounts the host's project folder to /workspaces/sentinel-uav
# Creating a separate workspace would break local-to-container code synchronization.

# Automatically source the ROS2 environment variables every time a new terminal is opened.
# This prevents the "command not found" error for ros2 CLI tools.
RUN echo "source /opt/ros/jazzy/setup.bash" >> /etc/bash.bashrc

# --- ArduPilot Gazebo Plugin & SITL ---
# We install BOTH the plugin and the pre-compiled SITL binary inside the container.
# Keeping SITL inside ensures our DevContainer remains 100% self-contained and reproducible,
# avoiding the need for host-machine installations.

# 1. Add the official Gazebo repository to get the Harmonic development headers, then install dependencies.
# We also include GStreamer development packages because the ardupilot_gazebo plugin explicitly requires them to compile.
# MAVProxy and its dependencies (pymavlink, future, empy) are included here as the primary GCS tool
# for manual validation, arming, and flight testing during development.
RUN apt-get update && apt-get install -y wget curl gnupg lsb-release python3-pip \
    && curl -sSL https://packages.osrfoundation.org/gazebo.gpg -o /usr/share/keyrings/pkgs-osrf-archive-keyring.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/pkgs-osrf-archive-keyring.gpg] http://packages.osrfoundation.org/gazebo/ubuntu-stable noble main" | tee /etc/apt/sources.list.d/gazebo-stable.list \
    && apt-get update \
    && apt-get install -y \
    libgz-sim8-dev \
    rapidjson-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    && pip3 install --break-system-packages \
    future \
    empy \
    pymavlink \
    MAVProxy \
    && rm -rf /var/lib/apt/lists/*

# 2. Download the pre-compiled ArduCopter SITL binary for rapid startup without bloating build times.
#
# IMPORTANT: This binary must be launched with '--model JSON', not '--model gazebo-iris'.
# The ardupilot_gazebo plugin uses the JSON backend protocol. Using the legacy binary
# protocol produces "Incorrect protocol magic" errors and no Gazebo connection.
#
# Verified launch command:
#   arducopter --model JSON --defaults /opt/ardupilot_gazebo/config/gazebo-iris-gimbal.parm -I0
RUN wget https://firmware.ardupilot.org/Copter/stable/SITL_x86_64_linux_gnu/arducopter -O /usr/local/bin/arducopter \
    && chmod +x /usr/local/bin/arducopter

# 3. Compile the ArduPilot-Gazebo plugin from a specific, stable commit hash to prevent future breakages.
ARG ARDUPILOT_GZ_COMMIT=082a0fe231f6e63bc8d1598f1cba461d9e2ea7f5
RUN git clone https://github.com/ArduPilot/ardupilot_gazebo.git /opt/ardupilot_gazebo \
    && cd /opt/ardupilot_gazebo \
    && git checkout ${ARDUPILOT_GZ_COMMIT} \
    && mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    && make -j$(nproc)

# 4. Export Gazebo plugin and resource paths so 'gz sim' can discover them automatically.
# Note: We intentionally do not chain ${GZ_SIM_SYSTEM_PLUGIN_PATH} here as it is undefined
# at build time. A single plugin path is sufficient for this project.
ENV GZ_SIM_SYSTEM_PLUGIN_PATH=/opt/ardupilot_gazebo/build
ENV GZ_SIM_RESOURCE_PATH=/opt/ardupilot_gazebo/models:/opt/ardupilot_gazebo/worlds