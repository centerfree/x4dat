#!/usr/bin/env python
from monitors import Monitor
import numpy


class Buffer(Monitor.Monitor):
    def __init__(self, buffer_len=2**10):
        super(Buffer, self).__init__()
        self.__len = buffer_len
        self.__data = None
        self.__counter = 0

    def step(self, frame: numpy.ndarray):
        if self.full():
            raise UserWarning("Buffer overflow...")
        if self.__data is None:
            print("Creating a buffer of size %dx%d" % (self.__len, frame.size))
            self.__data = numpy.empty((self.__len, frame.size))
        self.__data[self.__counter, :] = frame
        self.__counter += 1

    def reset(self, buffer_len=None):
        self.dump()
        self.__counter = 0
        self.__data = None
        if buffer_len:
            self.__len = buffer_len

    def dump(self):
        if self.__data is None:
            return None
        return self.__data[:self.__counter, :]

    def full(self):
        return self.__counter == self.__len

    def size(self):
        return self.__counter
