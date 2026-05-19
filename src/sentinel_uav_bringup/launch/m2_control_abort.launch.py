from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description() -> LaunchDescription:
    fcu_url = LaunchConfiguration('fcu_url')

    params_file = PathJoinSubstitution(
        [FindPackageShare('sentinel_uav_bringup'), 'config', 'mission_params.sim.yaml']
    )

    mavros = Node(
        package='mavros',
        executable='mavros_node',
        output='screen',
        parameters=[
            {
                'fcu_url': fcu_url,
            }
        ],
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

    return LaunchDescription(
        [
            DeclareLaunchArgument('fcu_url', default_value='tcp://127.0.0.1:5760'),
            mavros,
            bringup,
            observer,
            control,
        ]
    )
