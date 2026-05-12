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

# --- Micro XRCE-DDS Agent ---
# We compile and install the DDS Agent from source. 
# This acts as the direct broker between ArduPilot's native DDS publisher and our ROS2 network.
#
# Version is pinned via ARG to ensure reproducible builds.
# '--depth 1' skips full git history to reduce build time and image size.
# '-j$(nproc)' parallelizes compilation across all available CPU cores.
ARG XRCE_DDS_VERSION=v2.4.3
RUN git clone --branch ${XRCE_DDS_VERSION} --depth 1 \
    https://github.com/eProsima/Micro-XRCE-DDS-Agent.git \
    /opt/Micro-XRCE-DDS-Agent \
    && cd /opt/Micro-XRCE-DDS-Agent \
    && mkdir build && cd build \
    && cmake .. \
    && make -j$(nproc) \
    && make install \
    && ldconfig

# Note on Workspace: We DO NOT explicitly create a /ros2_ws here.
# VS Code DevContainers automatically mounts the host's project folder to /workspaces/sentinel-uav
# Creating a separate workspace would break local-to-container code synchronization.

# Automatically source the ROS2 environment variables every time a new terminal is opened.
# This prevents the "command not found" error for ros2 CLI tools.
RUN echo "source /opt/ros/jazzy/setup.bash" >> /etc/bash.bashrc

# --- NEW MILESTONE: ArduPilot Gazebo Plugin & SITL ---
# We install BOTH the plugin and the pre-compiled SITL binary inside the container.
# Keeping SITL inside ensures our DevContainer remains 100% self-contained and reproducible,
# avoiding the need for host-machine installations.

# 1. Add the official Gazebo repository to get the Harmonic development headers, then install dependencies.
# We also include GStreamer development packages because the ardupilot_gazebo plugin explicitly requires them to compile.
RUN apt-get update && apt-get install -y wget curl gnupg lsb-release \
    && curl -sSL https://packages.osrfoundation.org/gazebo.gpg -o /usr/share/keyrings/pkgs-osrf-archive-keyring.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/pkgs-osrf-archive-keyring.gpg] http://packages.osrfoundation.org/gazebo/ubuntu-stable noble main" | tee /etc/apt/sources.list.d/gazebo-stable.list \
    && apt-get update \
    && apt-get install -y \
    libgz-sim8-dev \
    rapidjson-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    && rm -rf /var/lib/apt/lists/*

# 2. Download the pre-compiled ArduCopter SITL binary for rapid startup without bloating build times.
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
ENV GZ_SIM_SYSTEM_PLUGIN_PATH=/opt/ardupilot_gazebo/build
ENV GZ_SIM_RESOURCE_PATH=/opt/ardupilot_gazebo/models:/opt/ardupilot_gazebo/worlds
