
#
# This script processes all-day tracking data and generates a complete report on 
# important statistics of the behavior
#

from subprocess import call

from df_reports import generate_reports

from postprocessing import extract_perch_locations
from postprocessing import extract_perching_locations
from postprocessing import extract_perching_orientations
from postprocessing import extract_flysim
from postprocessing import extract_log_info
from postprocessing import extract_headmovements
from postprocessing import create_highspeed_links
from postprocessing import plot_takeoffs

if __name__ == "__main__":
    # Process data with Python scripts
    extract_flysim.run()
    extract_perch_locations.run()
    extract_perching_locations.run()
    extract_perching_orientations.run()
    extract_log_info.run()
    extract_headmovements.run()
    plot_takeoffs.run()
    create_highspeed_links.run()
    generate_reports.run()
