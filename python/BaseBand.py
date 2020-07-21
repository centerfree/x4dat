#!/usr/bin/env python
from monitors import Buffer
from algorithms import common


class BaseBand(Buffer):
    def __init__(self, buffer_len: int = 2**10,
                 carrier_frequency: float = common.__fc,
                 sampling_frequency: float = common.__fs):
        super().__init__(buffer_len)
        self.__carrier_frequency = carrier_frequency
        self.__sampling_frequency = sampling_frequency

    def get_base_band_signal(self):
        return common.down_conversion(self.dump(),
                                      self.__carrier_frequency,
                                      self.__sampling_frequency,
                                      range_axis=1)
