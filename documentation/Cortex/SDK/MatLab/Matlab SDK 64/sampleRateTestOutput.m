% Script to test frame rate
%Initialize Cortex
CreateInitStruct;
retval = mCortexInitialize(initializeStruct)

% loop
tic;
telapsed = 0;
tstart = tic;
count = 1;
dataOut = [];
while telapsed < 5
    framed = mGetCurrentFrame();
    %M = framed.BodyData(1).Markers;
    M = framed.UnidentifiedMarkers;
    M1 = M(2:4,1)';
    telapsed = toc(tstart);
   % Create data output matrix
    dataOut(count,:) = [telapsed M1];
     
    count = count + 1;
end

% Create Time change array when data changes
count2 = 1;
tcount=1;
tchange=[];
    for i = 1:count-2
        
        if dataOut(count2,2) ~= dataOut(count2+1,2)
            tRate = dataOut(count2+1,1);
            tChange(tcount,:) = [tRate];
            tcount=tcount+1;            
        end

        count2 = count2+1;
    end
    
% Measure sample rate
sampleRate=[];
rcount=2;
for i = 1:tcount-2
    sRate = 1/(tChange(rcount)-tChange(rcount-1));
    sampleRate(rcount-1,:) = [sRate];
    
    rcount = rcount+1;
end

% Find the mean sample rate
sampleRateMean = mean(sampleRate);    

retval = mCortexExit()

