#!/usr/bin/env python
"""
Example script to download data from socket and process it
"""
from server import server
import numpy


def process_chunk(chunk):
    """
    Example function which processes a chunk of data frames and detects nearest disturbance or movement's distance
    :param chunk: array containing chunk of data frames
    """
    from matplotlib import pyplot
    tau = 1e-3
    disturbance = numpy.std(chunk, axis=0)
    indices = numpy.arange(disturbance.size)
    disturbance = disturbance * (indices > 100)
    hot_spots = disturbance > tau
    if hot_spots.sum() > 0:
        min_distance = numpy.min(indices[hot_spots])
    else:
        min_distance = 0
    min_distance = min_distance * 10. / 1467.
    pyplot.cla()
    pyplot.plot(disturbance)
    pyplot.title("Target Distance: %2.2f meters" % min_distance, fontsize=48)
    pyplot.pause(0.1)


def main():
    down_link = server.DownLink(max_frames=50)
    while True:
        down_link.download()
        prefixes, data = down_link.dump()
        process_chunk(data)


if __name__ == '__main__':
    main()
