import tkinter
from tkinter import messagebox
from monitors import Buffer
from utils.utils import select_directory, unique_file_name
from annotation.tools import write_info
from algorithms import io
from device import x4m300 as device
from time import sleep
from pathlib import Path
import os


class Recorder:
    __version__ = 2
    __buffer__ = None
    __connection_status = False
    __dev__ = ""
    __ext__ = "uwb"
    __info_ext__ = "info"
    __primary_light = "#fff7ff"
    __primary_dark = "#a094b7"
    __primary_medium = "#d1c4e9"
    __secondary_light = "#d1d9ff"
    __secondary_dark = "#6f79a8"
    __secondary_medium = "#9fa8da"
    __RED = "#bc477b"
    __GREEN = "#629749"
    __YELLOW = "#ffa040"
    __countdown_steps = 3
    __countdown_time = 2
    __fps = 100
    __buffer_size = -1

    def __init__(self):
        self.__window = tkinter.Tk()
        self.__window.title("UWB Recorder Tool - Version %d" %
                            self.__version__)
        self.__window.configure(background=self.__primary_medium)
        self.__sink_path = None

        common = {'master': self.__window,
                  'font': ('System', 16)}

        pad_high = {
            "padx": (8, 8),
            "pady": (32, 18)
        }

        label_padding = {
            "padx": (8, 8),
            "pady": (16, 0)
        }

        vertical_padding = {
            "pady": (64, 32),
        }

        entry_padding = {
            "padx": (8, 8),
            "pady": (0, 16),
            "ipady": 4,
        }

        self.__event_label = tkinter.Label(
            **common, width=16, bg=self.__primary_dark, text="Record Event\n(or Activity)")
        self.__event_label.grid(row=0, column=0, **label_padding)
        self.__event = tkinter.Entry(
            **common, width=18, bg=self.__primary_light)
        self.__event.grid(row=1, column=0,  **entry_padding)

        self.__duration_label = tkinter.Label(
            **common, width=16, bg=self.__primary_dark, text="Record Length\n(Milliseconds)")
        self.__duration_label.grid(row=0, column=1, **label_padding)
        self.__duration = tkinter.Entry(
            **common, width=18, bg=self.__primary_light)
        self.__duration.grid(row=1, column=1, **entry_padding)

        self.__distance_label = tkinter.Label(
            **common, width=16, bg=self.__primary_dark, text="Entity Distance\n(...)")
        self.__distance_label.grid(row=0, column=2, **label_padding)
        self.__distance = tkinter.Entry(
            **common, width=18, bg=self.__primary_light)
        self.__distance.grid(row=1, column=2, **entry_padding)

        self.__room_label = tkinter.Label(
            **common, width=16, bg=self.__primary_dark, text="Record Room\n(...)")
        self.__room_label.grid(row=0, column=3, **label_padding)
        self.__room = tkinter.Entry(
            **common, width=18, bg=self.__primary_light)
        self.__room.grid(row=1, column=3, **entry_padding)

        self.__entity_label = tkinter.Label(
            **common, width=16, bg=self.__primary_dark, text="Record Entity\n(...)")
        self.__entity_label.grid(row=0, column=4, **label_padding)
        self.__entity = tkinter.Entry(
            **common, width=18, bg=self.__primary_light)
        self.__entity.grid(row=1, column=4, **entry_padding)

        self.__comment_label = tkinter.Label(
            **common, width=16, bg=self.__primary_dark, text="Comment\n(...)")
        self.__comment_label.grid(row=0, column=5, **label_padding)
        self.__comment = tkinter.Entry(
            **common, width=18, bg=self.__primary_light)
        self.__comment.insert(tkinter.INSERT, "NA")
        self.__comment.grid(row=1, column=5, **entry_padding)

        self.__instruction_window = tkinter.Text(
            **common, width=64, height=8, bg=self.__RED)
        self.__instruction_window.insert(
            tkinter.INSERT, "Press start to continue...")
        self.__instruction_window.configure(state="disabled")
        self.__instruction_window.grid(
            row=2, column=1, columnspan=4, **pad_high)

        self.__toggle = tkinter.Button(**common,
                                       text="Start", relief="raised",
                                       height=2, width=64,
                                       command=self.start_button_callback,
                                       bg=self.__secondary_medium)
        self.__toggle.grid(row=3, column=1, columnspan=4, pady=(0, 32))

        self.__device_label = tkinter.Label(**common, width=32, height=2, bg=self.__primary_dark,
                                            text="Device Name\n(e.g. COM12)")
        self.__device_label.grid(
            row=4, column=0, columnspan=2, **label_padding)
        self.__device = tkinter.Entry(
            **common, width=36, bg=self.__primary_light)
        self.__device.grid(row=5, column=0, columnspan=2, **entry_padding)

        self.__device_window = tkinter.Text(
            **common, width=56, height=4, bg=self.__RED)
        self.__device_window.insert(tkinter.INSERT, "No device connected...")
        self.__device_window.configure(state="disabled")
        self.__device_window.grid(row=4, column=2, columnspan=3, rowspan=2)

        self.__connect = tkinter.Button(**common,
                                        text="Connect", relief="raised",
                                        height=4, width=12,
                                        bg=self.__secondary_medium,
                                        command=self.connect_callback)
        self.__connect.grid(row=4, column=5, rowspan=2)

        self.__sink_label = tkinter.Label(
            **common, height=2, width=32, bg=self.__primary_dark, text="Sink Directory")
        self.__sink_label.grid(row=6, column=0, columnspan=2,
                               rowspan=2, **vertical_padding)
        self.__sink = tkinter.Text(**common, width=56, height=2, bg=self.__RED)
        self.__sink.grid(row=6, column=2, columnspan=3,
                         rowspan=2, **vertical_padding)

        self.__browse = tkinter.Button(**common,
                                       text="Browse",
                                       height=2, width=12,
                                       bg=self.__secondary_medium,
                                       command=self.browse_callback)
        self.__browse.grid(row=6, column=5, rowspan=2, **vertical_padding)

        # self.__window.pack()
        self.__window.mainloop()

    def parse_and_verify(self):
        num_errors = 0
        info = dict()
        info['location'] = "NA"

        info['timestamp'] = self.__duration.get()
        try:
            self.__buffer_size = self.calculate_buffer_size(info['timestamp'])
        except Exception as e:
            num_errors += 1
            self.__buffer_size = -1
            print(e)
            messagebox.showerror(
                "Caution!!!", "Cannot understand time duration \"%s\"" % info['timestamp'])

        if self.__buffer_size < 1:
            num_errors += 1
            messagebox.showerror(
                "Caution!!!", "Invalid time duration \"%s\"" % info['timestamp'])

        info['timestamp'] = "%f" % (float(int(info['timestamp'])) / 1000.0)

        info['event_type'] = self.__event.get()
        if len(info['event_type']) < 1:
            num_errors += 0

        info['distance'] = self.__distance.get()
        if len(info['distance']) < 1:
            num_errors += 0

        info['room_id'] = self.__room.get()
        if len(info['room_id']) < 1:
            num_errors += 0

        info['person_id'] = self.__entity.get()
        if len(info['person_id']) < 1:
            num_errors += 0

        info['comments'] = self.__entity.get()
        if len(info['comments']) < 1:
            info['comments'] = "NA"

        self.__sink_path = self.__sink.get("1.0", tkinter.END).strip("\n")
        if not self.__sink_path:
            self.browse_callback()
            if not self.__sink_path:
                num_errors += 1
                messagebox.showerror(
                    "Fatal!!!", "Sink directory can not be empty.")
            if not Path(self.__sink_path).is_dir():
                num_errors += 1
                messagebox.showerror(
                    "Fatal!!!", "Invalid sink directory %s" % self.__sink_path)
        elif not Path(self.__sink_path).is_dir():
            self.browse_callback()
            if not self.__sink_path:
                num_errors += 1
                messagebox.showerror(
                    "Fatal!!!", "Sink directory can not be empty.")
            if not Path(self.__sink_path).is_dir():
                num_errors += 1
                messagebox.showerror(
                    "Fatal!!!", "Invalid sink directory %s" % self.__sink_path)

        if not self.__connection_status:
            self.connect_callback()
            if not self.__connection_status:
                num_errors += 1

        return info, num_errors

    def browse_callback(self):
        self.__sink.configure(bg=self.__YELLOW)
        self.__sink.delete("1.0", tkinter.END)
        self.__sink.insert(tkinter.INSERT, "selecting sink path...")
        self.__browse.configure(relief="sunken")
        self.__browse.configure(state="disabled")
        self.__window.update()
        self.__sink_path = select_directory(
            title="Select a sink directory to store UWB records").strip('\n')
        if self.__sink_path:
            self.__sink.configure(bg=self.__GREEN)
        else:
            self.__sink.configure(bg=self.__RED)
        self.__sink.delete("1.0", tkinter.END)
        self.__sink.insert(tkinter.INSERT, self.__sink_path)
        self.__browse.configure(relief="raised")
        self.__browse.configure(state="normal")
        self.__window.update()

    def connect_callback(self):
        self.__xep__ = None
        self.__dev__ = self.__device.get()

        if not len(self.__dev__):
            messagebox.showwarning("Caution!!!",
                                   "Please enter a valid Serial device name.\n \"COM 12\", \"/dev/ttyACM0/\" etc.")

        self.__device_window.configure(state="normal")
        self.__device_window.configure(bg=self.__YELLOW)
        self.__device_window.delete("1.0", tkinter.END)
        self.__device_window.insert(
            tkinter.INSERT, "connecting to device %s..." % self.__dev__)
        self.__connect.configure(relief="sunken")
        self.__connect.configure(state="disabled")
        self.__window.update()

        self.__connection_status = device.connect(self.__dev__)

        if not self.__connection_status:
            self.__device_window.configure(state="normal")
            self.__device_window.configure(bg=self.__RED)
            self.__device_window.delete("1.0", tkinter.END)
            self.__device_window.insert(
                tkinter.INSERT, "Failed to connect to device \"%s\"" % self.__dev__)
            self.__device_window.configure(state="disabled")
            self.__connect.configure(relief="raised")
            self.__connect.configure(state="normal")
            self.__window.update()
            messagebox.showerror(
                "Connection Error!!!", "Failed to connect to device \"%s\"" % self.__dev__)
            return

        self.__connect.configure(relief="raised")
        self.__connect.configure(state="normal")
        self.__device_window.configure(state="normal")
        self.__device_window.configure(bg=self.__GREEN)
        self.__device_window.delete("1.0", tkinter.END)
        self.__device_window.insert(
            tkinter.INSERT, "Connected to device \"%s\"" % self.__dev__)
        self.__device_window.configure(state="disabled")

    def calculate_buffer_size(self, ms: str):
        return int((float(int(ms)) * float(self.__fps)) / 1000.)

    def record(self, info: dict):
        self.__buffer__ = Buffer.Buffer(self.__buffer_size)
        device.simple_xep_read(self.__dev__,
                               baseband=False,
                               fps=self.__fps,
                               read_frame_callback=lambda arg: self.__buffer__.step(
                                   arg),
                               max_frames=self.__buffer_size)
        self.save(self.__buffer__.dump(), info)

    def save(self, frame, info: dict):
        file_path = unique_file_name()
        raw_data_path = os.path.join(self.__sink_path, file_path + "." + self.__ext__)
        annotation_path = os.path.join(self.__sink_path, file_path + "." + self.__info_ext__)
        io.write_uwb(raw_data_path, frame)
        write_info(annotation_path, info)
        print("Raw data and annotations are saved succesfully\nin %s\nand %s respectively\n" % (raw_data_path, annotation_path))

    def countdown_animation(self):
        for i in range(self.__countdown_steps):
            self.__instruction_window.configure(state="normal")
            self.__instruction_window.configure(bg=self.__RED)
            self.__instruction_window.delete("1.0", tkinter.END)
            self.__instruction_window.insert(
                tkinter.INSERT, "Countdown %d..." % (self.__countdown_steps - i))
            self.__instruction_window.configure(state="disabled")
            self.__window.update()
            sleep(0.5 * float(self.__countdown_time) /
                  float(self.__countdown_steps))

            self.__instruction_window.configure(bg=self.__YELLOW)
            self.__window.update()
            sleep(0.5 * float(self.__countdown_time) /
                  float(self.__countdown_steps))

    def start_button_callback(self):
        if self.__toggle.config('relief')[-1] == 'sunken':
            return
        else:
            self.__toggle.configure(state="normal")
            self.__toggle.configure(relief="sunken")
            self.__toggle.configure(text="Busy...")
            self.__toggle.configure(state="disabled")
            self.__window.update()

            info, num_errors = self.parse_and_verify()
            if num_errors > 0:
                self.__instruction_window.configure(state="normal")
                self.__instruction_window.configure(bg=self.__RED)
                self.__instruction_window.delete("1.0", tkinter.END)
                self.__instruction_window.insert(
                    tkinter.INSERT, "Please fix errors and press Start...")
                self.__instruction_window.configure(state="disabled")
                self.__toggle.configure(relief="raised")
                self.__toggle.configure(text="Start...")
                self.__toggle.config(state="normal")
                self.__window.update()
                return

            self.countdown_animation()

            self.__instruction_window.configure(state="normal")
            self.__instruction_window.configure(bg=self.__GREEN)
            self.__instruction_window.delete("1.0", tkinter.END)
            self.__instruction_window.insert(
                tkinter.INSERT, "Please perform the activity...")
            self.__instruction_window.configure(state="disabled")
            self.__window.update()

            self.record(info)
            # sleep(1)

            self.__instruction_window.configure(state="normal")
            self.__instruction_window.configure(bg=self.__RED)
            self.__instruction_window.delete("1.0", tkinter.END)
            self.__instruction_window.insert(
                tkinter.INSERT, "Press Start to continue...")
            self.__instruction_window.configure(state="disabled")
            self.__toggle.configure(relief="raised")
            self.__toggle.configure(text="Start...")
            self.__toggle.config(state="normal")
            self.__window.update()

    @staticmethod
    def info_callback():
        print("More information here...")


if __name__ == '__main__':
    rec = Recorder()
