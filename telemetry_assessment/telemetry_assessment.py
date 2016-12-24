
#
# This script streams the telemetry output from the FlySim arduino and saves it to file.
#

import serial
from tkinter import *
from tkinter import filedialog
from time import sleep
import datetime

if __name__ == "__main__":

    Tk().withdraw() # we don't want a full GUI, so keep the root window from appearing
    with filedialog.asksaveasfile(filetypes = (("Arduino Position Log", \
        "*.arduino.position.log"), ("all files","*.*"))) as f:

        print("Saving to file: "+str(f.name))

        ser = None
        while True:
            try:
                ser = serial.Serial('COM6', 9600)
            except:
                print("Could not connect to FlySim Arduino on port COM 6")
                sleep(1)
            # Connected successfully
            break

        # Wait for Kangaroo motors to intiailize
        sleep(10);

        # Send "space exploration" command
        ser.write('h\n'.encode('ascii'))
        ser.write('explore\n'.encode('ascii'))

        iline = 0
        while True:
            try:
                iline += 1
                line = ser.readline().decode('ascii')
                t = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")
                f.write(t + ',' + line + '\n')
                if (iline%100)==0:
                    f.flush()
                    print(line)

            except Exception as e:
                print("Can't read from Arduino, retrying...'" + str(e))
                sleep(1.0)
