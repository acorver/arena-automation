function [frameOfData, bodyDefs] = MatlabCortexSDKsample()

% [frameOfData, bodyDefs] = MatlabCortexSDKsample()
% Sample file to illustrate usage of the Matlab SDK for Cortex
% 
% This function creates a simple MENU with the following options:
% 
% 1: Obtain the current frame of data. This returns a frame of data structure to the
% variable 'frameOfData'
% 2: Obtain the body definitions: This returns a structure containing the
% body definitions to the variable 'bodyDefs'
% 3: Exit: This unloads the libraries and exits
% 
% The Librairies are initialized upon running the sample function

% Set-up the IP initialization values. These values should be edited to
% match appropriate values for a given system 

initializeStruct.TalkToHostNicCardAddress = '127.0.0.1';
initializeStruct.HostNicCardAddress = '127.0.0.1';
initializeStruct.HostMulticastAddress = '225.1.1.1';
initializeStruct.TalkToClientsNicCardAddress = '0';
initializeStruct.ClientsMulticastAddress = '225.1.1.2';

frameofData = [];
bodyDefs = [];

% Load the SDK libraries
returnValue = mCortexInitialize(initializeStruct);

if returnValue == 0
    choice = 1;
    
    while choice ~=3
        % Generate menu
        choice = menu('Matlab Cortex Sample Program','Get Frame of Data','Get Body Definitions','Unload Libraries and Exit');
        
        switch choice
            case 1 % Get Frame of Data
                frameOfData = mGetCurrentFrame()
            case 2 % Get Body Definitions
                bodyDefs = mGetBodyDefs()
            case 3 % Unload library and exit
                exitValue = mCortexExit();
        end
    end
       
else
    errordlg('Unable to initialize ethernet communication','Sample file error');
end

end

