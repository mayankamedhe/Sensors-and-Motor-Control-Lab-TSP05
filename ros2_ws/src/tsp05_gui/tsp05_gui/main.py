"""Open the TSP05 control panel as a standalone rqt window: ros2 run tsp05_gui tsp05_gui"""
import sys

from rqt_gui.main import Main


def main():
    sys.exit(Main().main(sys.argv, standalone="tsp05_gui.panel.Tsp05Panel"))


if __name__ == "__main__":
    main()
