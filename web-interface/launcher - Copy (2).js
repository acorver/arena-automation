
/* Imports */
var express = require('express')
var serveStatic = require('serve-static')
var serialport = require('serialport');
var queue = require('queue');

/* Start the application */
var app = express()

/* Declare global variables */
var devicePorts = {};

/* Proces USB requests concurrently */
var queue = queue();
queue.concurrency = 1;
queue.timeout = 4000;
queue.on('timeout', function(next, job) {
  console.log('Job timed out');
  next();
});

/* Serve static web interface content */
app.use(serveStatic('www', {'index': ['index.html']}));

/* Process command */
var processPowerCmd = function(relay, cmd, callback, res) {
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
                    if (res) { res.send("{\"status\": \"error\", \"response\": \"\"}"); }
                    console.log("Error in port "+devicePorts[relay]+": "+err);
                    port.close(function(err){
                        clearTimeout(timer); callback();
                    });
                } else {
                    port.write(cmd, function(){
                        port.drain();
                    });
                }
            } );
            
            /* Register data handler */
            var timer = setTimeout(function(){
                port.close(function(err){
                    if (res) { res.send("{\"status\": \"timeout\", \"response\": \"\"}"); }
                    callback();
                });
            }, 3000);

            port.on('data', function (data) {
                if (res) { res.send("{\"status\": \"ok\", \"response\": \""+
                    data.trim().replace(/"/g, '\\"')+"\"}"); }
                port.close(function(err){
                    clearTimeout(timer);callback();
                });
            });
        } catch (err) {
            callback();
        }
    }
}

/* Custom handler for turning power relays on/off and getting status */
app.get('/api/power/:relayid/:cmd', function(req, res){

    queue.push(function(callback){
        var relay = req.params.relayid;
        var cmd = req.params.cmd;
        processPowerCmd(relay, cmd, callback, res);        
    });

    queue.start(function(err) {});
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
            /* After having initialized our list of ports, re-synchronize the clocks */
            /* This makes sure alarms go off at the right time */
            for (portID in devicePorts) {
                var cmdSync = "t" + Math.floor(new Date() / 1000);
                processPowerCmd(portID, cmdSync, function(){}, null);
            }
            /*
            queue.push(function(callback){
                for (portID in devicePorts) {
                    var port = new serialport( devicePorts[portID], {
                        baudrate: 9600,
                        parser: serialport.parsers.readline("\n")
                    }, openCallback=function(err) {
                        if (err) {
                            console.log("Unable to sync port "+devicePorts[portID]);
                            port.close();
                            callback();
                        } else {
                            console.log("Sending sync command to "+devicePorts[portID]+": "+cmdSync);
                            
                            port.write(cmdSync, function(err){
                                if(err){
                                    console.log("Failed to sync: "+err);
                                    callback();
                                } else {
                                    port.drain(function(err2){
                                        callback();
                                        port.close(function(err3){ });
                                    });
                                }
                            });
                        }
                    } );
                }
            });
            */
        });
    });
}

/* Initialize */
fInit();

// TODO: Add button/status message indicating whether app is running / recording, e.g. by pinging it every few seconds.
// Add button to launch application (at least if it's not already running, o/w relaunch)

app.listen(80)
