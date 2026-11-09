from setuptools import find_packages, setup

package_name = "tsp05_gui"

setup(
    name=package_name,
    version="0.1.0",
    packages=find_packages(),
    data_files=[
        ("share/ament_index/resource_index/packages", ["resource/" + package_name]),
        ("share/" + package_name, ["package.xml", "plugin.xml"]),
        ("share/" + package_name + "/launch", ["launch/system.launch.py"]),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="Mayank Chandak",
    maintainer_email="mayank.chandak21@gmail.com",
    description="rqt control panel for the TSP05 ESP32 sensor and motor system.",
    license="MIT",
    entry_points={"console_scripts": ["tsp05_gui = tsp05_gui.main:main"]},
)
