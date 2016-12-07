
#
# This script extracts experimental log info
#

import os, sqlite3, re
from datetime import datetime

# Change working directory
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)),'data'))

# Helper function
def getTimeStr(time): return datetime.fromtimestamp(time//1000).strftime('%Y-%m-%d %H:%M:%S')

# =======================================================================================
# Process log
# =======================================================================================

def processFile(file):
    
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
            if '[FLYSIM] ' in r[2]:
                buf += r[2].replace('[FLYSIM] ','')
                # Buffer read whole line?
                if '\r\n' in buf:
                    logFlysim.append( (r[1], buf[0:buf.find('\r\n')]) )
                    buf = buf[buf.find('\r\n')+2:]
    
        # Process flysim log
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

def run():
    # Get all raw data files in data directory
    files = [x for x in os.listdir('./') if x.endswith('.log')]
    
    # Process newest files first
    files.sort(key=lambda x: -os.path.getmtime(x))
    
    # Process all log files:
    for file in files:
        processFile(file)    

if __name__ == "__main__":
    run()