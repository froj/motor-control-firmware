#!/usr/bin/env python3

import argparse
from pprint import pprint
import time
import os
import sys
import yaml
from watchdog.observers import Observer
from watchdog.events import PatternMatchingEventHandler
# from datadiff import diff
import msgpack
from serial_datagram import *
import serial

param_file_contents = {}
param_file_namespace = {}
conn_fd = None

def get_file_params(f):
    if os.path.exists(f):
        try:
            p = yaml.load(open(f, 'r'))
            return p
        except:
            return {}
    else:
        return {}

def parameter_update(p):
    global conn_fd
    packer = msgpack.Packer(encoding='ascii', use_single_float=True)
    packet = SerialDatagram.encode(packer.pack(p))
    conn_fd.write(packet)
    print("update {} bytes: {}".format(len(packet), p))
    print(packer.pack(p))


"""
@author: Jonathan Mickle
@summary: Traverses a through multi level dictionaries to diff them and return the changes -- Thanks to @tobywolf <https://github.com/tobywolf> for correcting logic
@param original: takes in the original unmodified JSON
@param modified: Takes in the modified json file for comparison
@return: a json dictionary of changes key-> value
"""
def diffDict(original, modified):
    if isinstance(original, dict) and isinstance(modified, dict):
        changes = {}
        for key, value in modified.items():
            if isinstance(value, dict):
                innerDict = diffDict(original[key], modified[key])
                if innerDict != {}:
                    changes[key] = {}
                    changes[key].update(innerDict)
            else:
                if key in original:
                    if value != original[key]:
                        changes[key] = value
                else:
                    changes[key] = value

        return changes
    else:
        raise Exception('parameters must be a dictionary')


class FileChangeHandler(PatternMatchingEventHandler):
    def on_any_event(self, event):
        if event.event_type in ('modified', 'created') and not event.is_directory:
            print("---- event handler ----")
            # print(event.event_type)
            print(event.src_path)
            new_params = get_file_params(event.src_path)
            if new_params == {}:
                return
            # print(new_params)
            param_diff = diffDict(param_file_contents[event.src_path], new_params)
            if param_diff != {}:
                parameter_update({param_file_namespace[event.src_path]: param_diff})
            # update parameter copy:
            param_file_contents[event.src_path] = new_params


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("dir", help="parameter root directory to be watched")
    parser.add_argument("files", nargs='+', help="json parameter files to be watched")
    parser.add_argument("--dev", dest="dev", help="serial port device")
    parser.add_argument("--baud", dest="baudrate", default=921600, help="serial port baudrate")
    args = parser.parse_args()

    global conn_fd
    print("opening serial port {} with baud rate {}".format(args.dev, args.baudrate))
    conn_fd = serial.Serial(args.dev, args.baudrate)


    files = [os.path.abspath(f) for f in args.files]
    watch_dir = os.path.abspath(args.dir)
    # watch_dir = os.path.dirname(os.path.commonprefix(files))
    print("watching parameter files: {} in {}".format(
        [os.path.basename(f) for f in files], watch_dir))

    for f in files:
        param_file_namespace[f] = os.path.splitext(os.path.relpath(f, watch_dir))[0]
        param_file_contents[f] = get_file_params(f)

    for f in files:
        parameter_update({param_file_namespace[f]: param_file_contents[f]})
    # pprint(param_file_contents)

    event_handler = FileChangeHandler(files)
    observer = Observer()
    observer.schedule(event_handler, watch_dir, recursive=True)
    observer.start()

    try:
        while True:
            # print(conn_fd.read())
            sys.stdout.write(conn_fd.read().decode('ascii', 'ignore'))
            # time.sleep(1)
    except KeyboardInterrupt:
        observer.stop()
    observer.join()


if __name__ == "__main__":
    main()
