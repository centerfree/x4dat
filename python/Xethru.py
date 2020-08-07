from time import sleep

import numpy
import pymoduleconnector

from device.Device import Device
from device.x4m300 import simple_xep_read, clear_buffer


class Xethru(Device):
    __version__ = 1
    __delayed_launch = 3

    def __init__(self):
        Device.__init__(self)

    def reset(self, device_name: str):
        """
        Reset device with a delayed launch
        :param device_name: Device name such as "com12"
        :return: None
        """
        mc = pymoduleconnector.ModuleConnector(device_name)
        xep = mc.get_xep()
        xep.module_reset()
        mc.close()
        sleep(self.__delayed_launch)

    def read(self, device_name: str, new_frame_callback: lambda arg: None, fps=100,  max_frames=100):
        simple_xep_read(device_name=device_name, read_frame_callback=new_frame_callback, fps=fps, max_frames=max_frames)

    def connect(self, device_name: str):
        try:
            self.reset(device_name)
        except RuntimeError as e:
            return e.with_traceback(e.__traceback__)
        mc = pymoduleconnector.ModuleConnector(device_name)

        # Assume an X4M300/X4M200 module and try to enter XEP mode
        app = mc.get_x4m300()
        # Stop running application and set module in manual mode.
        try:
            app.set_sensor_mode(0x13, 0)  # Make sure no profile is running.
        except RuntimeError:
            # Profile not running, OK
            pass

        try:
            app.set_sensor_mode(0x12, 0)  # Manual mode.
        except RuntimeError:
            # Maybe running XEP firmware only?
            pass

        xep = mc.get_xep()

        # Set DAC range
        xep.x4driver_set_dac_min(900)
        xep.x4driver_set_dac_max(1150)

        # Set integration
        xep.x4driver_set_iterations(16)
        xep.x4driver_set_pulses_per_step(26)

        xep.x4driver_set_downconversion(0)
        # Start streaming of data
        xep.x4driver_set_fps(10)

        connection_status = False
        try:
            frame = xep.read_message_data_float()
            connection_status = numpy.array(frame.data).size > 0
        finally:
            # Stop streaming of data
            xep.x4driver_set_fps(0)

        return connection_status
