from device.Device import Device
from server import server


class Artik(Device):
    __version__ = 1
    __link = None

    def __init__(self):
        Device.__init__(self)

    def read(self, device_name: str, new_frame_callback: callable, fps=100, max_frames=100):
        split_device_name = device_name.split(':')
        if len(split_device_name) != 2:
            raise ValueError("Invalid device name for Artik device.\n"
                             "Expected format: <ip_address:port>\n"
                             "Example: 192.168.0.1:5005")
        address, port = split_device_name
        self.__link = server.DownLink(ip_address=address, port=int(port), max_frames=max_frames)
        self.__link.download(new_frame_callback=new_frame_callback)

    def connect(self, device_name: str):
        split_device_name = device_name.split(':')
        if len(split_device_name) != 2:
            raise ValueError("Invalid device name for Artik device.\n"
                             "Expected format: <ip_address:port>\n"
                             "Example: 192.168.0.1:5005")
        address, port = split_device_name
        self.__link = server.DownLink(ip_address=address, port=int(port), max_frames=3)
        self.__link.download()
        prefixes, data = self.__link.dump()
        if data is None:
            return False
        else:
            return data.shape[0] == 3

    def reset(self, device_name: str):
        pass
