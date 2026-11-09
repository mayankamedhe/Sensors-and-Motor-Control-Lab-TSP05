"""Everything on the laptop side in one command: serial bridge + control panel.

  ros2 launch tsp05_gui system.launch.py port:=/dev/ttyUSB0
"""
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    bridge = os.path.join(get_package_share_directory("tsp05_bridge"), "launch", "bridge.launch.py")
    return LaunchDescription([
        DeclareLaunchArgument("port", default_value="/dev/ttyUSB0",
                              description="ESP32 serial device or pyserial URL"),
        IncludeLaunchDescription(PythonLaunchDescriptionSource(bridge),
                                 launch_arguments={"port": LaunchConfiguration("port")}.items()),
        Node(package="tsp05_gui", executable="tsp05_gui", output="screen"),
    ])
