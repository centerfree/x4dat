#!/usr/bin/env python
"""
Up-link and Down-link classes for raw UWB data stream
"""
import socket
import numpy
from collections import deque

PREFIX_LENGTH = 8
PREFIX = '%.{}d'.format(PREFIX_LENGTH)
SOCKET_BUFFER_SIZE = 2**15
ENCODING = 'utf-8'
DEFAULT_IP = "127.0.0.1"
DEFAULT_PORT = "5005"


def serialize(counter=0, array=None):
    if array is None:
        message = PREFIX % counter
        message = message.encode(ENCODING)
    else:
        arr = array.reshape(-1, ).astype('complex')
        arr_real = numpy.real(arr)
        arr_imag = numpy.imag(arr)
        arr2 = numpy.hstack((arr_real, arr_imag))
        message = PREFIX % counter
        message = message.encode(ENCODING) + arr2.tobytes()
    return message


def deserialize(message):
    prefix, data = message[:PREFIX_LENGTH], message[PREFIX_LENGTH:]
    prefix = int(prefix.decode(ENCODING))
    if len(data) > 0:
        data = numpy.frombuffer(data, dtype='float64')
        n = int(data.shape[0] / 2)
        data = data[:n] + 1j * data[n:]
        return prefix, data
    else:
        return prefix, None


class UpLink:
    """
    Class to upload data for streaming, assuming source of data to be uploaded is buffered
     e.g. pymoduleconnector.ModuleConnector.get_xep().read_message_data_float() is a high level buffered implementation
    """
    def __init__(self, ip_address=DEFAULT_IP, port=5005, verbose=0):
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

    def _upload(self, message):
        self._sock.sendto(message, (self._UDP_IP, self._UDP_PORT))

    def upload(self, arr: numpy.ndarray):
        """
        Create a data packet from numpy array with frame number prefix
        :param arr: numpy array to be streamed
        :return: None
        """
        message = serialize(self._counter, arr)
        self._counter += 1
        self._upload(message)
        if self._verbose > 0:
            print("Frame %d sent..." % self._counter)

    def terminate(self):
        """
        terminate socket connection
        """
        message = (PREFIX % (10 ** PREFIX_LENGTH - 1)).encode(ENCODING)
        self._upload(message)


class DownLink:
    """
    Class to download streaming data with buffered implementation
    """
    _last_prefix = -1

    def __init__(self, max_frames=1024, ip_address="127.0.0.1", port=5005, verbose=0):
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

    def download(self, num_packets=None):
        """receive and decode data packet and store prefix with array"""
        packet_count = 0
        if num_packets is None:
            num_packets = self.max_frames
        while packet_count < num_packets:
            message, address = self._sock.recvfrom(SOCKET_BUFFER_SIZE)  # buffer size is 1024 bytes
            prefix, data = deserialize(message)
            if prefix == 10 ** PREFIX_LENGTH - 1:
                break
            else:
                if prefix > self._last_prefix:
                    self._last_prefix = prefix
                else:
                    continue

                packet_count += 1

                if self._counter >= self.max_frames:
                    print("Max frames have been read, frames may be dropped, stopping download...")
                    break
                else:
                    if not data:
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
