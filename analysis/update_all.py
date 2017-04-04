
#
# This script processes all-day tracking data and generates a complete report on 
# important statistics of the behavior
#

# =======================================================================================
# Imports
# =======================================================================================

import os
from subprocess import call

from df_reports import generate_reports

from postprocessing import extract_perch_locations
from postprocessing import extract_perching_locations
from postprocessing import extract_perching_orientations
from postprocessing import extract_flysim
from postprocessing import extract_raw_mac_data
from postprocessing import extract_mac_trigger
from postprocessing import extract_log_info
from postprocessing import extract_headmovements
from postprocessing import create_highspeed_links
from postprocessing import plot_takeoffs

from shared import util

# TODO: Make arena interface display list of reports... =) And generate placeholder "in progress" html files?

# Working directory is data directory
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)),'../data'))

# =======================================================================================
# Go through all post-processing steps
#    This will produce extracted trajectories and other relevant events, in a format 
#    that facilitates easy plotting and analysis. This will also generate HTML reports.
# =======================================================================================

def updateAll(settings):
    # Process data with Python scripts
    p1 = extract_flysim.run(async = False, settings = settings)
    print("Done processing FlySim, now extracting log info.")

    extract_log_info.run(settings = settings)
    print("Done extracting log info, now computing perch locations.")

    p2 = extract_perch_locations.run(async=False, settings = settings)
    
    # Make sure the previous processes have finished before starting the next ones
    #p1.join()
    #p2.join()
    
    print("Done extracting perch locations, now computing takeoffs / perching locations.")
    
    # After FlySim trajectories have been extracted, we can process perching locations
    p3 = extract_perching_locations.run(async=False, settings = settings)
    
    print("Done computing takeoffs, now extracting raw camera data.")
    p4 = extract_raw_mac_data.run(async = False, settings = settings)

    print("Done extracting raw camera data, now extracting alignment triggers.")
    extract_mac_trigger.run(settings = settings)
    
    #extract_perching_orientations.run(async=True)
    #extract_headmovements.run()
    
    # Make sure the previous processes have finished before starting the next ones
    #p3.join()
    
    print("Done processing data, now plotting takeoffs.")
    
    p5 = plot_takeoffs.run(async=False, settings = settings)
    create_highspeed_links.run(async=False, settings = settings)
    
    # Make sure the previous processes have finished before generating the final report
    #p5.join()
    
    print("Done plotting takeoffs, now generating reports.")

    # Generate the final report
    generate_reports.run(settings = settings)
    
    # Print done
    print("All data analysis is up to date.")
    
# =======================================================================================
# Main entry point: Allows this script to be run directly by user, who will be queried for input
# =======================================================================================

if __name__ == "__main__":
    
    # Ask for options
    settings = util.askForExtractionSettings()
    
    updateAll(settings)
    
    input("Press any button to exit.") 