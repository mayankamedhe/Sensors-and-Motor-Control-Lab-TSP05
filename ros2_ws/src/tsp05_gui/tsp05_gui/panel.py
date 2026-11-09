"""rqt plugin wrapper for the TSP05 control panel."""
from qt_gui.plugin import Plugin

from tsp05_gui.panel_widget import PanelWidget


class Tsp05Panel(Plugin):
    def __init__(self, context):
        super().__init__(context)
        self.setObjectName("Tsp05Panel")
        self._widget = PanelWidget(context.node)
        if context.serial_number() > 1:
            self._widget.setWindowTitle(f"{self._widget.windowTitle()} ({context.serial_number()})")
        context.add_widget(self._widget)

    def shutdown_plugin(self):
        self._widget.shutdown()
