
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

/* ============================================================================
 * Process power command
 * ============================================================================ */

var processPowerCmd = function(relay, cmd, res) {

    queue.push(function(callback){
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
                        port.write(cmd + '\n', function(){
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
    });

    queue.start(function(err) {});
}

/* Custom handler for turning power relays on/off and getting status */
app.get('/api/power/:relayid/:cmd', function(req, res){

    var relay = req.params.relayid;
    var cmd = req.params.cmd;
    processPowerCmd(relay, cmd, res);
});

/* ============================================================================
 * Process CableFlysim command
 * ============================================================================ */

var processCableFlysimCmd = function(cmd, res) {

    queue.push(function(callback){
        try{
            console.log(devicePorts["CABLEFLYSIM"]);
            var port = new serialport( devicePorts["CABLEFLYSIM"], {
                baudrate: 9600,
                parser: serialport.parsers.readline("\n")
            }, openCallback=function(err) {
                if (err) {
                    if (res) { res.send("{\"status\": \"error\", \"response\": \"\"}"); }
                    console.log("Error in CableFlysim port "+devicePorts["CABLEFLYSIM"]+": "+err);
                    port.close(function(err){
                        clearTimeout(timer); callback();
                    });
                } else {
                    port.write(cmd + '\n', function(){
                        port.drain();
                    });
                }
            } );

            port.on('data', function (data) {
                if (res) { res.send("{\"status\": \"ok\", \"response\": \""+
                    data.trim().replace(/"/g, '\\"')+"\"}"); }
                port.close(function(err){
                    clearTimeout(timer);callback();
                });
            });

            /* Register data handler */
            var timer = setTimeout(function(){
                port.close(function(err){
                    if (res) { res.send("{\"status\": \"timeout\", \"response\": \"\"}"); }
                    callback();
                });
            }, 3000);
        } catch (err) {
            callback();
        }
    });

    queue.start(function(err) {});
};

/* Custom handler for communicating with CableFlysim */
app.get('/api/cableflysim/:cmd', function(req, res){

    var cmd = req.params.cmd;

    console.log("received cableflysim cmd: "+cmd);

    processCableFlysimCmd(cmd, res);
});

/* ============================================================================
 * Initialize COM/USB ports
 * ============================================================================ */

/* Try connecting to a port */
var fInit = function() {
    console.log("Searching COM ports for power relay interfaces.");
    serialport.list(function (err, ports) {

        console.log("Found "+ports.length+" ports: ");
        for (var a=0;a<ports.length;a++) {
          console.log("  "+ports[a].comName);
        }

        var tryPort = function(i, callback) {
            /* Have we processed all available ports? */
            if (i >= ports.length) {
                console.log("Reached port number "+i);
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
                } else {
                    console.log("Successfully opened serial port.");

                    port.write('h\n', function(){
                        port.drain(function(){});
                    });
                }
            } );

            /* Register data handler */
            port.on('data', function (data) {
                console.log("Received response from port "+i+": "+data);
                if ( data.indexOf('Power Relay Controller ') !== -1 ) {
                    /* Save this port */
                    idx = 'POWER_RELAY_' + data.substring('Power Relay Controller '.length).trim()
                    devicePorts[idx] = ports[i].comName;
                } else if ( data.indexOf('CableFlysim Controller') !== -1 ) {
                    devicePorts['CABLEFLYSIM'] = ports[i].comName;
                }
                console.log(JSON.stringify(devicePorts));
                port.close(function(){});
            });

            /* Try next port */
            setTimeout(function(){
              if (port.isOpen) {
                console.log("Port timed out. Closing port and moving on...")
              }
              port.close(function(){
                tryPort(i+1, callback);
              });
            }, 750);
        };
        tryPort(0, function(){

            console.log("Finished port discovery...");

            /* After having initialized our list of ports, re-synchronize the clocks */
            /* This makes sure alarms go off at the right time */
            for (portID in devicePorts) {
                var cmdSync = "t" + Math.floor(new Date() / 1000);
                processPowerCmd(portID, cmdSync, null);
            }
        });
    });
}

/* Initialize */
fInit();

// TODO: Add button/status message indicating whether app is running / recording, e.g. by pinging it every few seconds.
// Add button to launch application (at least if it's not already running, o/w relaunch)

console.log("Listening on port 80...");
app.listen(80)
