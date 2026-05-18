from launch import LaunchDescription
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description() -> LaunchDescription:
    params_file = PathJoinSubstitution(
        [FindPackageShare('sentinel_uav_bringup'), 'config', 'mission_params.sim.yaml']
    )

    bringup = Node(
        package='sentinel_uav_bringup',
        executable='bringup_orchestrator',
        name='bringup_orchestrator',
        output='screen',
        parameters=[params_file],
    )

    observer = Node(
        package='sentinel_uav_core',
        executable='observer_node',
        name='observer_node',
        output='screen',
        parameters=[params_file],
    )

    control = Node(
        package='sentinel_uav_core',
        executable='control_node',
        name='control_node',
        output='screen',
        parameters=[params_file],
    )

    return LaunchDescription([bringup, observer, control])
