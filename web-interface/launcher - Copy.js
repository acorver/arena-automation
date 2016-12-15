
/* Imports */
var express = require('express')
var serveStatic = require('serve-static')
var serialport = require('serialport');
var async = require('async');

/* Start the application */
var app = express()

/* Declare global variables */
var devicePorts = {};

/* Proces USB requests concurrently */
var queue = async.queue(function(task, callback){ task(callback); }, 1);

/* Serve static web interface content */
app.use(serveStatic('www', {'index': ['index.html']}))

/* Custom handler for turning power relays on/off and getting status */
app.get('/api/power/:relayid/:cmd', function(req, res){

    queue.push(function(callback){
        var relay = req.params.relayid;
        var cmd = req.params.cmd;

        if (!(relay in devicePorts)) {
            res.send("Device ID not recognized.");
            callback();
        } else {
            try{
                var port = new serialport( devicePorts[relay], {
                    baudrate: 9600,
                    parser: serialport.parsers.readline("\n")
                }, openCallback=function(err) {
                    if (err) {
                        res.send("{\"status\": \"error\", \"response\": \"\"}");
                        console.log("Error in port "+devicePorts[relay]+": "+err);
                        port.close();
                        callback();
                    }

                    port.write(cmd, function(){
                        port.drain();
                    });
                } );
                
                /* Register data handler */
                var timer = setTimeout(function(){
                    console.log("<");
                    port.close(function(err){
                        if (err) {
                            res.send("{\"status\": \"timeout\", \"response\": \"\"}");
                            callback();
                        }
                    });
                    console.log(">");
                }, 15000);

                port.on('data', function (data) {
                    clearTimeout(timer);
                    res.send("{\"status\": \"ok\", \"response\": \""+data.trim().replace(/"/g, '\\"')+"\"}");
                    port.close()
                    callback();
                });
            } catch (err) {
                callback();
            }
        }
    });
});

/* Try connecting to a port */
var fInit = function() {
    console.log("Searching COM ports for power relay interfaces.");
    serialport.list(function (err, ports) {
        var tryPort = function(i, callback) {
            /* Have we processed all available ports? */
            if (i >= ports.length) { 
                callback();
                return; 
            }

            console.log("Trying port "+i+" ("+JSON.stringify(ports[i].comName)+")");

            /* Open the connection */
            var port = new serialport( ports[i].comName, {
                baudrate: 9600,
                parser: serialport.parsers.readline("\n")
            }, openCallback = function(err){
                if (err) {
                    console.log("Error connecting to port "+ports[i].comName+": "+err);
                    tryPort(i+1, callback); 
                } else {
                    console.log("Successfully opened serial port.");
                    
                    port.write('h', function(){
                        port.drain(function(){});
                    });
                }
            } );
            
            /* Register data handler */
            port.on('data', function (data) {
                console.log("Received response from port "+i+": "+data);
                if ( data.indexOf('Power Relay Controller ') !== -1 ) {
                    /* Use this port for our communications */
                    idx = data.substring('Power Relay Controller '.length).trim()
                    devicePorts[idx] = ports[i].comName;
                }
                console.log(JSON.stringify(devicePorts));
                port.close()
            });
            
            
            /* Try next port */
            setTimeout(function(){tryPort(i+1, callback)}, 5000);
        };
        tryPort(1, function(){
            return;
            /* After having initialized our list of ports, re-synchronize the clocks */
            /* This makes sure alarms go off at the right time */
            for (portID in devicePorts) {
                var port = new serialport( devicePorts[portID], {
                    baudrate: 9600,
                    parser: serialport.parsers.readline("\n")
                }, openCallback=function(err) {
                    if (err) {
                        console.log("Unable to sync port "+devicePorts[portID]);
                        port.close();
                    } else {
                        var cmdSync = "t" + Math.floor(new Date() / 1000);
                        console.log("Sending sync command to "+devicePorts[portID]+": "+cmdSync);
                        /*
                        port.write(cmdSync, function(){
                            port.drain(function(){
                                port.close();
                            });
                        });
                        */
                    }
                } );
            }
        });
    });
}

/* Initialize */
fInit();

// TODO: Add button/status message indicating whether app is running / recording, e.g. by pinging it every few seconds.
// Add button to launch application (at least if it's not already running, o/w relaunch)

app.listen(80)
