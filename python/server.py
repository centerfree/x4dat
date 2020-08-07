#!/usr/bin/env python
"""
Up-link and Down-link classes for raw UWB data stream
"""
import socket
import numpy
from collections import deque

SOCKET_BUFFER_SIZE = 2**15
ENCODING = 'utf-8'
DEFAULT_IP = "127.0.0.1"
DEFAULT_PORT = 80


def serialize(counter=0, array=None, fc=-1.0, fs=-1.0):
    message = numpy.array([float(counter), fc, fs]).astype('float32')
    message = message.tobytes()
    if array is not None:
        arr = array.reshape(-1, ).astype('float32')
        message = message + arr.tobytes()
    return message


def deserialize(message):
    data = numpy.frombuffer(message, dtype='float32')
    prefix = data[2]
    data = data[3:]
    if len(data) > 0:
        return prefix, data
    else:
        return prefix, None


class UpLink:
    """
    Class to upload data for streaming, assuming source of data to be uploaded is buffered
     e.g. pymoduleconnector.ModuleConnector.get_xep().read_message_data_float() is a high level buffered implementation
    """
    __fc = None
    __fs = None

    def __init__(self, ip_address=DEFAULT_IP, port=DEFAULT_PORT, verbose=0, fc=7.29e9, fs=23.328e9):
        """
        :param ip_address:
        :param port:
        :param verbose:
        """
        self._UDP_IP = ip_address
        self._UDP_PORT = port

        self._sock = socket.socket(socket.AF_INET,  # Internet
                                   socket.SOCK_DGRAM)  # UDP
        self._counter = 0
        self._verbose = verbose
        self.__fc = fc
        self.__fs = fs

    def _upload(self, message):
        self._sock.sendto(message, (self._UDP_IP, self._UDP_PORT))

    def upload(self, arr: numpy.ndarray):
        """
        Create a data packet from numpy array with frame number prefix
        :param arr: numpy array to be streamed
        :return: None
        """
        message = serialize(self._counter, arr, self.__fc, self.__fs)
        self._counter += 1
        self._upload(message)
        if self._verbose > 0:
            print("Frame %d sent..." % self._counter)

    def terminate(self):
        """
        terminate socket connection
        """
        message = serialize(-1, None)
        self._upload(message)


class DownLink:
    """
    Class to download streaming data with buffered implementation
    """
    _last_prefix = -1

    def __init__(self, max_frames=1024, ip_address=DEFAULT_IP, port=DEFAULT_PORT, verbose=0):
        """

        :param max_frames:
        :param ip_address:
        :param port:
        :param verbose:
        """
        self._UDP_IP = ip_address
        self._UDP_PORT = port

        self._sock = socket.socket(socket.AF_INET,  # Internet
                                   socket.SOCK_DGRAM)  # UDP
        self._sock.bind((self._UDP_IP, self._UDP_PORT))

        self._counter = 0
        self.raw_data = None
        self.prefixes = None
        self.max_frames = max_frames

    def download(self, num_packets=None, new_frame_callback: callable = None):
        """receive and decode data packet and store prefix with array"""
        packet_count = 0
        if num_packets is None:
            num_packets = self.max_frames
        while packet_count < num_packets:
            message, address = self._sock.recvfrom(SOCKET_BUFFER_SIZE)  # buffer size is 1024 bytes
            # print(len(message), message)
            prefix, data = deserialize(message)
            packet_count += 1

            if new_frame_callback is not None:
                new_frame_callback(data)

            if self._counter >= self.max_frames:
                print("Max frames have been read, frames may be dropped, stopping download...")
                break
            else:
                if data is None:
                    continue

                if self._counter == 0:
                    self.raw_data = deque(maxlen=self.max_frames)
                    self.prefixes = deque(maxlen=self.max_frames)

                self.raw_data.append(data)
                self.prefixes.append(prefix)
                self._counter += 1

    def dump(self):
        """
        get prefixes and raw data frames and reset buffer
        :return: a tuple containing array of prefixes and data frames
        """
        if self._counter > 0:
            self._counter = 0
            return numpy.array(self.prefixes), numpy.array(self.raw_data)
        else:
            return None, None
