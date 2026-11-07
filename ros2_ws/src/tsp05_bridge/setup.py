from glob import glob

from setuptools import find_packages, setup

package_name = "tsp05_bridge"

setup(
    name=package_name,
    version="0.1.0",
    packages=find_packages(exclude=["test"]),
    data_files=[
        ("share/ament_index/resource_index/packages", ["resource/" + package_name]),
        ("share/" + package_name, ["package.xml"]),
        ("share/" + package_name + "/launch", glob("launch/*.launch.py")),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="Aaron Chou",
    maintainer_email="uuuloon777@gmail.com",
    description="ROS 2 serial bridge between the TSP05 ESP32 firmware and the laptop GUI.",
    license="MIT",
    tests_require=["pytest"],
    entry_points={
        "console_scripts": [
            "serial_bridge = tsp05_bridge.serial_bridge:main",
            "fake_esp32 = tsp05_bridge.fake_esp32:main",
        ],
    },
)
