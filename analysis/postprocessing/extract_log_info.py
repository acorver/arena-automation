
#
# This script extracts experimental log info
#

# =======================================================================================
# Change working directory so this script can be run independently as well as as a module
# =======================================================================================

import os, sys
if __name__ == "__main__": 
    p = os.path.dirname(os.path.abspath(__file__))
    os.chdir(os.path.join(p,'../../data'))
    sys.path.insert(0, os.path.join(p,'../'))

# =======================================================================================
# Imports for this script
# =======================================================================================

import os, sqlite3, re
from datetime import datetime

# Helper function
def getTimeStr(time): return datetime.fromtimestamp(time//1000).strftime('%Y-%m-%d %H:%M:%S')

# =======================================================================================
# Process log
# =======================================================================================

def processFile(dataFile):
    
    file = dataFile.replace(".msgpack",".log")
    fnameLogOut = file.replace('.log','.log.txt')
    fnameOut = file.replace('.log','.trials.csv')
    
    with open(fnameOut, 'w') as fo:
        # Write header
        fo.write('timestamp, time, speed, height, wait\n')

        # Open log database
        conn = sqlite3.connect(file)
        c = conn.cursor()
    
        # Query all
        rows = []
        try:
            rows = [r for r in c.execute("select * from plog")]
        except:
            print("ERROR: Can't process file: "+file+" (potentially outdated log format)")
            return
    
        # Get flysim log
        logFlysim = []
        buf = ''
        for r in rows:
            if r[2] != None and '[FLYSIM] ' in r[2]:
                buf += r[2].replace('[FLYSIM] ','')
                # Buffer read whole line?
                if '\r\n' in buf:
                    logFlysim.append( (r[1], buf[0:buf.find('\r\n')]) )
                    buf = buf[buf.find('\r\n')+2:]
        
        # Write log to file
        with open(fnameLogOut, 'w') as foLog:
            for line in logFlysim:
                foLog.write(','.join([str(x) for x in list(line)]) + '\n')
        
        # Process flysim log (TODO: Parse new JSON format!!)
        for line in logFlysim:
            m = re.match('Started new trial: speed=([0-9]*), height=([0-9]*), wait=([0-9]*)', line[1])
            if m != None:
                g = m.groups()
                actualTrialStart = line[0] + int(g[2])*1000
                fo.write( ','.join([str(x) for x in [actualTrialStart, getTimeStr(actualTrialStart), ] + list(g)]) + '\n' )
    
    print("Processed file: "+file)

# =======================================================================================
# Entry point
# =======================================================================================

def run(async=False, settings = None):    
    
    if settings == None:
        settings = util.askForExtractionSettings()

    # Process all log files:
    for file in settings.files:
        try:
            processFile(file)
        except Exception as e:
            print(e)

if __name__ == "__main__":
    run()