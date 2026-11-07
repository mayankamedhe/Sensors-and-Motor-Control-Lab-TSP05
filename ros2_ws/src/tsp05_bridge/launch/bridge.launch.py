"""Start the TSP05 serial bridge, and optionally foxglove_bridge for the GUI.

  ros2 launch tsp05_bridge bridge.launch.py port:=/dev/ttyUSB0
  ros2 launch tsp05_bridge bridge.launch.py port:=/dev/ttyUSB0 foxglove:=true
"""
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    port = LaunchConfiguration("port")
    baud = LaunchConfiguration("baud")
    foxglove = LaunchConfiguration("foxglove")
    return LaunchDescription([
        DeclareLaunchArgument("port", default_value="/dev/ttyUSB0",
                              description="Serial device or pyserial URL (rfc2217://localhost:4000 for Wokwi)"),
        DeclareLaunchArgument("baud", default_value="115200"),
        DeclareLaunchArgument("foxglove", default_value="false",
                              description="Also start foxglove_bridge (ws://localhost:8765)"),
        Node(
            package="tsp05_bridge",
            executable="serial_bridge",
            namespace="tsp05",
            parameters=[{"port": port, "baud": baud}],
            output="screen",
        ),
        Node(
            package="foxglove_bridge",
            executable="foxglove_bridge",
            condition=IfCondition(foxglove),
            output="screen",
        ),
    ])
