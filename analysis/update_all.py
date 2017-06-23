
#
# This script processes all-day tracking data and generates a complete report on 
# important statistics of the behavior
#

# =======================================================================================
# Imports
# =======================================================================================

import os, multiprocessing
from subprocess import call

from df_reports import generate_reports

from postprocessing import extract_perch_locations
from postprocessing import extract_perching_locations
from postprocessing import extract_perching_orientations
from postprocessing import extract_flysim
from postprocessing import extract_raw_mac_data
from postprocessing import extract_mocap_trigger
from postprocessing import extract_log_info
from postprocessing import extract_headmovements
from postprocessing import create_highspeed_links
from postprocessing import plot_takeoffs
from postprocessing import merge_daily_data

from shared import util

# TODO: Make arena interface display list of reports... =) And generate placeholder "in progress" html files?

# Working directory is data directory
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)),'../data'))

# =======================================================================================
# Go through all post-processing steps
#    This will produce extracted trajectories and other relevant events, in a format 
#    that facilitates easy plotting and analysis. This will also generate HTML reports.
# =======================================================================================

def updateAll_p1(settings):
    print("Extracting raw camera data.")
    p4 = extract_raw_mac_data.run(async=False, settings=settings)

    print("Done extracting raw camera data, now extracting alignment triggers.")
    extract_mocap_trigger.run(settings=settings)

    print("Extracting telemetry")
    pass

def updateAll_p2(settings):
    # Process data with Python scripts
    print("Processing Flysim")
    extract_flysim.run(async=False, settings=settings)

    print("Done processing FlySim, now extracting log info.")
    extract_log_info.run(settings=settings)

    print("Done extracting log info, now computing perch locations.")
    extract_perch_locations.run(async=False, settings=settings)

    print("Done extracting perch locations, now computing takeoffs / perching locations.")

    # After FlySim trajectories have been extracted, we can process perching locations
    extract_perching_locations.run(async=False, settings=settings)

    # extract_perching_orientations.run(async=True)
    # extract_headmovements.run()

    print("Done processing data, now plotting takeoffs.")
    plot_takeoffs.run(async=False, settings=settings)
    create_highspeed_links.run(async=False, settings=settings)

    # Generate the final report
    print("Done plotting takeoffs, now generating reports.")
    generate_reports.run(settings=settings)

    # Print done
    print("All data analysis is up to date.")


def updateAll(settings):
    # Create daily files (by default, we now always compute the data over entire days, rather than segments)
    merge_daily_data.run(settings)

    # Run other scripts (in parallel where possible, hence part 1 (p1) and part 2 (p2)
    p1 = multiprocessing.Process(target=updateAll_p1, args=(settings,))
    p1.start()

    p2 = multiprocessing.Process(target=updateAll_p2, args=(settings,))
    p2.start()

    # Wait for subprocesses to finish
    p1.join()
    p2.join()

# =======================================================================================
# Main entry point: Allows this script to be run directly by user, who will be queried for input
# =======================================================================================

if __name__ == "__main__":

    print("Starting data processing...")

    # Ask for options
    settings = util.askForExtractionSettings()
    
    updateAll(settings)
    
    input("Press any button to exit.") 