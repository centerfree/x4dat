- Each .npy file contains 6 seconds windows data.
- Corresponding directory will also have an output.txt file.

- Example of output.txt -->
2020-09-30 20:16:15 (3, 602, 600): [1, 1, 1]
2020-09-30 20:16:21 (603, 1202, 600): [1, 1, 0]
2020-09-30 20:16:27 (1203, 1802, 600): [0, 0, 0]

- Format of output.txt -->
DATE TIME (S, E, E-S+1): [out0, out1, out2]

where
S = starting frame index of 6 sec window data (minimum index)
E = last frame index of 6 sec window data (maximum index)
E-S+1 = represents number of frames between first and last (should be 600 in case of no loss)
out0 = Output of model 0
out1 = Output of model 1
out2 = Output of model 2