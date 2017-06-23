
# --------------------------------------------------------
# Library for telemetry reconstruction
#
# Author: Abel Corver
#         abel.corver@gmail.com
#         (Anthony Leonardo Lab, July. 2016)
# --------------------------------------------------------

# =================================================================================================
# Import libraries
# =================================================================================================

import os, struct, numpy as np, pandas as pd
from configparser import ConfigParser

# =================================================================================================
# Load metadata file
# =================================================================================================

def loadMeta(fname):
    meta = {}

    try:
        class obj(object):
            def __init__(self, d):
                for a, b in d.items():
                    if isinstance(b, (list, tuple)):
                       setattr(self, a, [obj(x) if isinstance(x, dict) else x for x in b])
                    else:
                       setattr(self, a, obj(b) if isinstance(b, dict) else b)

        with open(fname, 'r') as f:
            config_string = '[dummy_section]\n' + f.read()
            parser = ConfigParser()
            parser.read_string(config_string)
            metaTmp = dict(parser._sections['dummy_section'])

            for key, value in metaTmp.items():
                try:
                    meta[key] = int(value)
                except:
                    try:
                        meta[key] = float(value)
                    except:
                        if value.lower() in ['true','false']:
                            meta[key] = True if value.lower()=='true' else False
                        elif ',' in value:
                            meta[key] = value.split(',')
                        else:
                            meta[key] = value

            # Objects are easier to address (i.e. a dot (.) instead of braces (["..."]))
            meta = obj(meta)

        # Do some special processing on customRanges information
        for i in range(len(meta.customranges)):
            meta.customranges[i] = [float(x) for x in meta.customranges[i].split(':')]
    except Exception as e:
        print(e)
        return None

    return meta

# =================================================================================================
# Load raw ephys data from binary file
# =================================================================================================

def loadEphys(meta):
    colNames = meta.chandisplaynames
    raw = np.fromfile(meta.outputfile, dtype=np.int16).reshape( (-1, meta.nchans) )
    return pd.DataFrame(raw, columns=colNames, index=list(range(raw.shape[0])))

# =================================================================================================
# Reconstruct ephys
# =================================================================================================

def reconstructEphys(fname, saveToFile=False, ignoreSavedFile=False):

    # Did we already process this file and save the results to disk?
    fnameOutCsv = fname.replace('.meta', '') + '.csv'
    if os.path.exists(fnameOutCsv) and not ignoreSavedFile:
        # Don't re-process data, just return the already computed table
        return pd.read_csv(fnameOutCsv)

    # Read .meta file
    meta = loadMeta(fname)

    # Read .bug3 file
    bug3FileName = meta.outputfile.replace('.bin', '') + '.bug3'
    bug3File = ConfigParser(strict=False)
    bug3File.read(bug3FileName)

    # Variables
    errorLog = []
    vBlock = []
    vChipFrames = []
    vBoardFrames = []
    vScanCount = []
    vSystemClock = []

    # Handle exceptions (can either 'raise' the exception, interrupting the program
    # (unless the exception is handled downstream), or simply add it to the log and continue operation)
    def exc(exception):
        errorLog.append(exception)

    # Get variable from bug3 file
    def getVariableFromBlock(blockID, key):
        return [int(x) for x in bug3File._sections[blockID][key].split(',')]

    # Loop through the raw data
    for blockID in bug3File.sections():
        # Parse metadata
        block = int(''.join([x for x in blockID if x.isdigit()]))
        scanCount = getVariableFromBlock(blockID, 'spikegl_datafile_scancount')[0]
        chipFrames = getVariableFromBlock(blockID, 'chipframecounter')
        boardFrames = getVariableFromBlock(blockID, 'boardframecounter')
        systemclock = getVariableFromBlock(blockID, 'systemclock')

        # Check data integrity
        if (len(boardFrames) != len(chipFrames)):
            exc(Exception("Number of chip and board frame indices should match, but didn't..."))

        # Check if this block is valid (i.e. not all zeroes)
        isValid = np.logical_not(np.all(np.array(chipFrames) == 0))

        if not isValid:
            block = chipFrames = boardFrames = scanCount = (np.array(chipFrames) * np.nan).tolist()
        else:
            scanCount = [scanCount + (i - (len(chipFrames)-1)) * 16 for i in range(len(chipFrames))]
            block       = [block       for i in range(len(chipFrames))]
            systemclock = [meta.createdon[0:meta.createdon.find(' ')] + ' ' +
                           systemclock for i in range(len(chipFrames))]

        vChipFrames += chipFrames
        vBoardFrames += boardFrames
        vScanCount += scanCount
        vBlock += block
        vSystemClock += systemclock

    # Correct scanCount indices:
    # This bug is introduced by the fact that the whole pre-buffer gets written at once, which
    # means that the spikeGL_DataFile_SampleCount is wrong for the entire pre-buffer
    # To fix this, we start at the last block, and work our way backwards
    vScanCountOld = [x for x in vScanCount]
    for i in range(len(vScanCount)):
        vScanCount[i] = vScanCountOld[-1] - ((len(vScanCountOld)-1) - i) * 16

    # Neural samples per frame constant
    NEURAL_SAMPLES_PER_FRAME = 16

    # create data frame
    data = pd.DataFrame(
        {'block'        : np.repeat(vBlock       , NEURAL_SAMPLES_PER_FRAME),
         'systemClock'  : np.repeat(vSystemClock , NEURAL_SAMPLES_PER_FRAME),
         'chipFrame'    : np.repeat(vChipFrames  , NEURAL_SAMPLES_PER_FRAME),
         'boardFrame'   : np.repeat(vBoardFrames , NEURAL_SAMPLES_PER_FRAME),
         'scanIndex'    : np.repeat(vScanCount   , NEURAL_SAMPLES_PER_FRAME),
         'sampleIndex'  : list(range(len(vBlock) * NEURAL_SAMPLES_PER_FRAME)),
         'scanIndexOld' : np.repeat(vScanCountOld, NEURAL_SAMPLES_PER_FRAME) })

    # Align ephys data
    #   - Load data
    ephys = loadEphys(meta)
    #   - Scale all channels to mV units
    for icol in range(ephys.shape[1]):
        col = ephys.columns[icol]
        ephys[col] /= (2**15-1) # The number are signed in16's
        center = 0.5 * (meta.customranges[icol][1] + meta.customranges[icol][0])
        extent = meta.customranges[icol][1] - center
        ephys[col] = center + (ephys[col] * extent)
        ephys[col] *= 1000 # customRanges is in V, so scale to mV
    #   - Align data from last timepoint backward, and remove any block metadata for which there is no neural data
    data = data[data.index > max(data.index) - len(ephys)].reset_index(drop =True)
    #   - Start "sample" index at 0 (aesthetic reasons only...)
    data.sampleIndex -= min(data.sampleIndex)
    #   - Concatenate neural data
    data = pd.concat([data, ephys], axis=1)

    # Compute a time axis
    #    NOTE: This entire function relies on sequential and ordered processing of rows, in ascending order of
    #          both block index and neural sample index
    #  NOTE 2: Currently, the code makes no attempt to recover the time axis after a completely invalid block.
    #          Several improvements can be made: We already have a (200 Hz) clock signal, which can perhaps we
    #          used to recover the timing (e.g. using the system clock data to verify that the delay wasn't longer
    #          than 5 ms. To overcome longer gaps, we could use an additional sync signal that encodes temporal
    #          offset information, allowing us to recover longer gaps. This is not yet implemented.
    def computeTime(d, direction = 1):
        vBoardFrames = d.boardFrame.as_matrix()[::direction]
        vTime = np.zeros( vBoardFrames.shape[0] )
        t = 0
        for i in range(1, len(vBoardFrames)):
            if np.isnan(vBoardFrames[i]) or np.isnan(t):
                # If a board frame becomes NaN, this means a large number of frames were
                # missing, and the current set of syncing signals do not allow recovery of the
                # absolute time
                t = np.nan
            else:
                deltaSample = abs(vBoardFrames[i] - vBoardFrames[i-1]) * NEURAL_SAMPLES_PER_FRAME
                if deltaSample == 0:
                    # If there was no change in frame, we just had an increase in neural sample
                    deltaSample += 1
                else:
                    # If there was an increase, due to assumed ordered processing we must've gone from sample
                    # 15 to sample 0, so subtract 15... Note that deltaSample will still be larger than 1 if
                    # frames were skipped!
                    deltaSample -= 15
                # Update time (in units of samples)
                t += deltaSample * direction
            # Save the computed time!
            vTime[i] = float(t) / meta.sratehz

        # Add the new column to the dataframe
        d['time'] = vTime[::direction]

        # Done computing time in this direction
        return d

    # Find trigger
    #    Note: Rapid triggering and long time windows can cause the recorded segment to include
    #          multiple trigger points. We look for the one closest to the intended trigger point
    #         (currently 8 seconds into a 13 second segment, i.e. 8 second pre-buffer, 5 second post-buffer)
    columnName = [x for x in data if '(TRG)' in x][0]
    trigSignal = data[columnName]
    trigIdx    = trigSignal[trigSignal.diff() > 1000]
    relativeTriggerTiming = (meta.filetimesecs - meta.pdstoptime) / meta.filetimesecs
    bestTrig   = [abs(x - (relativeTriggerTiming * len(trigSignal))) for x in trigIdx.index.tolist()]
    bestTrig   = bestTrig.index(min(bestTrig))
    trigIdx    = trigIdx.index[bestTrig]

    # Compute time leftward and rightward w.r.t. the trigger point
    x1 = computeTime(data[data.index <= trigIdx], direction = -1)
    x2 = computeTime(data[data.index >= trigIdx], direction =  1)
    # Note that we included the trigger sample (t=0) in the leftwards/backward-looking computeTime(.) call
    # as well. This was to correctly compute the time offset of the first backward sample. We now remove
    # this first sample to prevent a duplicate t=0 sample.
    data = pd.concat([x1.iloc[0:len(x1)-1], x2], axis=0)

    # Set sampleIndex as Pandas DataFrame index
    data.set_index('sampleIndex', inplace=True)

    # Save to file?
    if saveToFile:
        data.to_csv(fnameOutCsv)

    # Done!
    return data

# =================================================================================================
# Entry point
# =================================================================================================

if __name__ == "__main__":
    fname = "Z:/people/Abel/SpikeGL/testdata/telemtest_3.meta"
    reconstructEphys(fname)