#!/usr/bin/env python
"""
#Target module: X4M300

#Introduction: XeThru modules support both RF and baseband data output. This is an example of radar raw data processing.
               Developer can use Module Connector API to read chip-set and stream raw data.

#Command to run: "python stream.py -d com12
                 change "com12" with your device name, using "--help" to see other options.
"""

import argparse
from server import server
from device.Xethru import Xethru as Dev


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--verbose", help="Print log messages", action="store_true", default=False)
    parser.add_argument("--device", help="Device name (e.g. com12)", required=True, default='com12')
    parser.add_argument("--fps", type=int, help="Frames per second read from the sensor")

    args = parser.parse_args()

    verbose = False
    if args.verbose:
        verbose = True

    fps = 100
    if args.fps:
        fps = args.fps

    socket = server.UpLink(verbose=verbose)

    def fun(f):
        socket.upload(f)

    if verbose:
        print("Initializing data streaming from %s" % args.device)

    dev = Dev()
    connected = dev.connect(device_name=args.device)

    if not connected:
        raise RuntimeError("Count not connect to " + args.device)

    try:
        dev.read(device_name=args.device, fps=fps, new_frame_callback=fun, max_frames=2**16 - 1)
    finally:
        socket.terminate()


if __name__ == "__main__":
    main()
