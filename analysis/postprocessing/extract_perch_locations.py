
import msgpack
import numpy as np
import math, os, multiprocessing

# Set working directory
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)),'../data'))

# Set "overwrite" to True to overwrite existing files
OVERWRITE = False

# Misc. constants
CORTEX_NAN  = 9999999

def processFile(file):
    print("Started file: "+file)
    outfile = file.replace('.msgpack','.perch_objects.csv')
    try:
        if OVERWRITE or not os.path.isfile( outfile ):
            i = 0
            with open(outfile, 'w') as fo:
                with open(file,'rb') as f:
                    filename = file
                    if '/' in filename:
                        filename = filename[filename.rfind('/')+1:]
                    for x in msgpack.Unpacker(f):
                        # Save only a very small subsample. This data is only meant for determining the position (and variation in position / stability) 
                        # of the physical perch objects, which are currently all stationary.
                        if ((i%100000)==0): 
                            if not isinstance(x, int):
                                for b in x[2]:
                                    # Extract marker points
                                    pos = [[z if z!=CORTEX_NAN else float('NaN') for z in y] for y in b[1]]
                                    for mi in range(len(pos)):
                                        fo.write(','.join([filename, str(b[0]), str(x[0]), str(mi)] + [str(y) for y in pos[mi]]) + '\n')
                        i+=1
                        if ((i%1000000)==0): 
                            print("[" + file + "] Processed "+str(i)+" frames")
    except Exception as e:
        print(str(e))
        os.remove(outfile)

def run(async=False):
    
    files = [x for x in os.listdir('./') if x.endswith('.msgpack')]
    with multiprocessing.Pool(processes=12) as pool:
        (pool.map_async if async else pool.map)(processFile, files)
        return pool
    
    return None

if __name__ == '__main__':    
    run()