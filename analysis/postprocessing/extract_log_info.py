
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

import os, sqlite3, re, json
from datetime import datetime
from shared import util

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
        fo.write('timestamp, time, speed, speedchange, height, wait\n')

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

        with open(fnameLogOut, 'w') as foLog:
            for row in rows:
                foLog.write('\t'.join([str(x) for x in row]) + '\n')

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
        
        # Process flysim log (TODO: Parse new JSON format!!)
        lookForNewTrial = False
        for line in logFlysim:
            # Parse trial announcements
            if 'Started new trial' in line[1]:
                lookForNewTrial = True
            if lookForNewTrial:
                s = None
                try:
                    s = json.loads(line[1])
                except Exception as e:
                    pass
                if s is not None:
                    istart = 0
                    if abs(s['PosX']-s['MaxPosX']) < abs(s['PosX']-s['MinPosX']):
                        istart = len(s['TargetVelX_Segments'])-1
                    g = [abs(s['TargetVelX_Segments'][istart]),
                         s['TargetVelX_Segments'][(len(s['TargetVelX_Segments'])-1)-istart] -
                            s['TargetVelX_Segments'][istart],
                         s['TargetPosZ1'],
                         int(s['TimeUntilTrial'])/1000]
                    actualTrialStart = line[0] + g[2]
                    fo.write(','.join([str(x) for x in
                         [actualTrialStart, getTimeStr(actualTrialStart), ] + list(g)]) + '\n')
                    lookForNewTrial = False
            # Deprecated trial format (kept here to parse older log files)
            m = re.match('Started new trial: speed=([0-9]*), height=([0-9]*), wait=([0-9]*)', line[1])
            if m != None:
                g = m.groups()
                actualTrialStart = line[0] + int(g[2])*1000
                g = list(g)
                g = [g[0], g[0], g[1], g[2]]
                fo.write( ','.join([str(x) for x in [actualTrialStart, getTimeStr(actualTrialStart), ] + g]) + '\n' )
    
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