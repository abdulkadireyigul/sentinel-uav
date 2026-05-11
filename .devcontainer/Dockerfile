# We use the 'desktop-full' image to avoid downloading heavy GUI dependencies (like Gazebo and Qt)
# during the 'apt-get' phase, which previously caused network timeouts and MTU bridge issues.
FROM osrf/ros:jazzy-desktop-full

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

# --- NEW MILESTONE: Micro XRCE-DDS Agent ---
# We compile and install the DDS Agent from source. 
# This acts as the direct broker between ArduPilot's native DDS publisher and our ROS2 network.
RUN git clone https://github.com/eProsima/Micro-XRCE-DDS-Agent.git /opt/Micro-XRCE-DDS-Agent \
    && cd /opt/Micro-XRCE-DDS-Agent \
    && mkdir build && cd build \
    && cmake .. \
    && make \
    && make install \
    && ldconfig

# Automatically source the ROS2 environment variables every time a new terminal is opened.
# This prevents the "command not found" error for ros2 CLI tools.
RUN echo "source /opt/ros/jazzy/setup.bash" >> /root/.bashrc