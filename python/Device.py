#!/usr/bin/env python
"""
#Interface for all uwb devices

#Usage: See Artik.py or Xethru.py
"""


class Device:
    __version__ = 1

    def __init__(self):
        pass

    def reset(self, device_name: str):
        pass

    def connect(self, device_name: str):
        pass

    def read(self, device_name: str, new_frame_callback: callable, fps=100,  max_frames=100):
        pass
