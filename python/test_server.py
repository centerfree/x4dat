#!/usr/bin/env python
"""
Example script to download data from socket and process it
"""
from server import server
import numpy


def clean(arr, carrier_freq=7.29e9, sampling_freq=23.328e9, threshold=1e-5):
    carrier = numpy.exp(
        -1j *
        (carrier_freq / sampling_freq) *
        (2 * numpy.pi) *
        numpy.arange(arr.shape[1]))
    carrier = numpy.repeat(carrier.reshape(1, -1), repeats=arr.shape[0], axis=0)
    multiplexed = arr * carrier
    mag = numpy.abs(multiplexed)
    phase = numpy.angle(multiplexed)
    mag[mag < threshold] = threshold
    stable = mag * numpy.exp(1j * phase)
    log_scale = numpy.log(stable.astype(numpy.complex))
    log_scale = log_scale - numpy.repeat(numpy.nanmean(log_scale, axis=1, keepdims=True),
                                         axis=1, repeats=log_scale.shape[1])
    corrected = numpy.exp(log_scale)
    return corrected


def process_chunk(chunk):
    """
    Example function which processes a chunk of data frames and detects nearest disturbance or movement's distance
    :param chunk: array containing chunk of data frames
    """
    from matplotlib import pyplot
    offset = 150
    cleaned = clean(chunk)[:, offset:]
    cleaned = cleaned - numpy.repeat(numpy.nanmean(cleaned, axis=0, keepdims=True),
                                     axis=0, repeats=cleaned.shape[0])
    tau = 1
    disturbance = numpy.std(cleaned, axis=0)
    indices = numpy.arange(disturbance.size)
    disturbance = numpy.log(disturbance + tau)
    hot_spots = disturbance > numpy.log(2 * tau)
    if hot_spots.sum() > 0:
        min_distance = numpy.min(indices[hot_spots])
    else:
        min_distance = 0
    min_distance = (min_distance + offset) * 9.6 / 1467.
    pyplot.cla()
    pyplot.plot(disturbance)
    pyplot.title("Target Distance: %2.2f meters" % min_distance, fontsize=48)
    pyplot.pause(0.1)


def main():
    down_link = server.DownLink(ip_address="192.168.137.1", port=5005, max_frames=100)
    while True:
        down_link.download()
        prefixes, data = down_link.dump()
        process_chunk(data)


if __name__ == '__main__':
    main()
