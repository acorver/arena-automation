
# Arena Automation System
This guide documents the use of the suite of
hardware and software tools that collectively
facilitate the automatic, unsupervised recording
and analysis of dragonfly activity during whole-day
recording sessions. This collection of interrelated tools
shall be referred to as the
*Arena Automation System,* or **AAS**.

## Authors:
* Abel Corver, abel.corver@gmail.com
(Last contribution: June 2017)

## Table of Contents
* Practical step-by-step guide
* Technical documentation:
  * High-level overview
  * Overview of the technology stack
  * Device Diagram
  * Installation and repository structure
  * A note on code editors
  * Real-time software
    * ...
  * Real-time hardware:
    * Trigger Signal Generator
    * LED Trigger Signal Generator
    * FlySim Controller
      * FlySim Z Controller
      * Kangaroo Controller
    * Remote-controllable power switches
  * Software post-processing:
    * Overview of the processing pipeline
    * A note on data formats
    * FlySim extraction
    * Takeoff trajectory extraction
    * Telemetry extraction
  * Unfinished work-in-progress
    * Interfacing with Photrons
    * CableFlysim
    * Rotating stage
    * Networked sign
    *
  * Miscellaneous tools
    * Auto-wand tracking
    * Arena Telemetry Assessment
    * Test backpack

## Practical step-by-step guide
**Step 1**: Launch

- Mention TeamViewer

## Technical documentation
### High-level overview
The _Arena Automation System_ consists of roughly two
components. A real-time pipeline, and an offline post-processing
pipeline. The real-time system has three main goals:

#### The realtime pipeline
* **Goal 1: Save all motion capture data**.

![Starting The Web Interface](https://github.com/acorver/arena-automation/blob/dev/documentation/images/arena_interface_launch_icon.png)

The real-time system
receives every single 3D marker point, and saves its XYZ
coordinates. The output file contains all these markers
(*.msgpack files) is the most important reference dataset,
from which all analysis scripts start.

In addition, the real-time system continuously interfaces
with the Cortex software, and triggers the recording of
virtually all raw mocap data. Because these blocks have
to be re-triggered, about .5 seconds of data is lost for
every minute of recording. However, because the XYZ
marker data (as opposed to the raw XY camera centroids)
is streamed independently, no XYZ data is lost.

* **Goal 2: Trigger high-speed cameras and telemetry**

High-speed (Photron) recordings cannot be performed
continuously, and therefore need to be triggered.
Similarly, the telemetry data recording is currently
recorded upon being triggered, and not continuously saved.

The *Arena Automation System* continuously streams
motion capture data from the Cortex software in real-time,
and tracks the identified Yframes. Upon detection of
a takeoff event (currently based on a velocity threshold),
the system sends a USB command to the Trigger Signal Generator,
which phase-locks the trigger with the motion capture and
camera clocks. This trigger signal will save all high-speed
and telemetry data, and furthermore causes the LED Trigger
Signal to blink, creating an identifiable trigger alignment
point in the motion capture data stream.

* **Goal 3: Control real-time systems**

Lastly, the real-time component of the *Arena Automation System*
interfaces with various peripheral devices that require
real-time feedback and are integral to the experiment.

A primary example of this is the Perch Controller. The
*AAS* monitors the orientation of the perches, and sends
serial commands to the Perch Controller which then rotates
the perches.

In addition, the *AAS* monitors whether a dragonfly is
currently on the perch and has reached the desired
orientation. If so, it notifies the FlySim Controller
that the dragonfly is ready. This functionality, if
enabled, can be used to limit trial presentation to only
those circumstances when the dragonfly is correctly
oriented.

Other real-time monitoring functions can be added
easily.

#### The offline pipeline

The goal of the offline pipeline is to extract all
dragonfly activity, especially prey-capture flights,
from the mass of data collected by the real-time system.

The offline pipeline consists of many scripts (written in
Python) that each extract a particular type of information
from the raw recordings. Here is an overview:

| Tables        | Are           | Cool  |
| ------------- |:-------------:| -----:|
| col 3 is      | right-aligned | $1600 |
| col 2 is      | centered      |   $12 |
| zebra stripes | are neat      |    $1 |

Each of these scripts can be run independently,
facilitating easier experimentation with new
analyses, and allowing subsets of the analysis to be
updated without re-running the whole pipeline.

In most cases, however, the pipeline will be run
with a single click. The main script that will
start the processing will then run each of the processing
sub-steps. All analysis output files will be put
in the data directory for that day/recording, with the
exception of the summary reports, which are in their
own folder.

The offline pipeline takes as its starting point the
XYZ marker data files produced by the real-time system.
These files have .msgpack extensions.

Unless this step is disabled, the first step in the
postprocessing pipeline is to merge all data files
recorded on the same day, under the assumption that
a given day will only contain a single type of
experiment.

The next step is to assign to each frame a unique ID.
Because Cortex can be stopped and restarted,
the frame counter can be reset to 0. This step
makes sure that in all later analyses, frameIDs
are unique and consistent across different type of
analysis output files.

At the end of the entire pipeline, a few files are of
particular interest:

* *.mocap.csv* : ...

...

### Hardware Subsystems

### Software processing
This section details the various scripts that
extract content from

#### Overview of the processing pipeline

Cortex frame received, streamed to file

Cortex frame re-indexed to prevent index conflicts
