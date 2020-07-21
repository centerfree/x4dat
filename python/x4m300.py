#!/usr/bin/env python
"""
#Target module: X4M300

#Introduction: XeThru modules support both RF and baseband data output. This is an example of radar raw data processing.
               Developer can use Module Connector API to read chip-set and stream raw data.

#Usage: See stream.py
"""

from time import sleep
import numpy
import pymoduleconnector
import sys


__version__ = 1
__delayed_launch = 3


def reset(device_name: str):
    """
    Reset device with a delayed launch
    :param device_name: Device name such as "com12"
    :return: None
    """
    mc = pymoduleconnector.ModuleConnector(device_name)
    xep = mc.get_xep()
    xep.module_reset()
    mc.close()
    sleep(__delayed_launch)


def clear_buffer(mc: pymoduleconnector.ModuleConnector):
    """
    Clears the frame buffer
    :param mc: pymoduleconnector.ModuleConnector instance
    :return: None
    """
    xep = mc.get_xep()
    while xep.peek_message_data_float():
        xep.read_message_data_float()


def simple_xep_read(device_name: str, baseband=False, fps=100, read_frame_callback=None, verbose=0, max_frames=128):
    """
    Read raw data from chip-set and perform subsequent tasks with a callback
    :param device_name: Device name such as "com12"
    :param baseband: Convert received signal to baseband
    :param fps: Frames per second to read from the sensor
    :param read_frame_callback: this function is called every time a data frame is read from sensor. This can be used
    for further processing or streaming.
    :param verbose: Print log messages
    :return: None
    """
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

    xep.x4driver_set_downconversion(int(baseband))
    # Start streaming of data
    xep.x4driver_set_fps(fps)

    def read_frame():
        """Gets frame data from module"""
        d = xep.read_message_data_float()
        fr = numpy.array(d.data)
        # Convert the resulting frame to a complex array if downconversion is enabled
        if baseband:
            n = int(len(fr) / 2)
            fr = fr[:n] + 1j * fr[n:]
        return fr

    counter = 0
    try:
        while counter < max_frames:
            frame = read_frame()
            clear_buffer(mc)
            counter += 1
            if read_frame_callback is not None:
                read_frame_callback(frame)
    finally:
        # Stop streaming of data
        xep.x4driver_set_fps(0)


def connect(device_name: str):
    try:
        reset(device_name)
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
