
/* Imports */
var os = require('os');
var ifaces = os.networkInterfaces();
var express = require('express')
var serveStatic = require('serve-static')
var serialport = require('serialport');
var queue = require('queue');
var spawn = require('child_process').spawn;
var path = require('path');
var fs   = require('fs');

var DEBUG = true;

// DEBUG WITH LONGER STACK TRACES:
require('longjohn');

/* Start the application */
var app = express()

/* Declare global variables */
var devicePorts = {};

/* Variable for the child process of the arena automation application */
var arenaAutomationProcess = null;

/* Proces USB requests concurrently */
var queue = queue();
queue.concurrency = 1;
queue.timeout = 4000;
queue.on('timeout', function(next, job) {
  log('Job timed out');
  next();
});

/* Serve static web interface content */
app.use(serveStatic('www', {'index': ['index.html']}));

/* ======================================================================================
   Helper functions
====================================================================================== */

var log = function(msg, lvl='debug') {
    if (lvl == 'notification' || DEBUG) { // Change this line to filter out certain messages
        console.log(msg);
    }
}

app.get('/api/devices', function(req, res){
    res.send(devicePorts);
});

/* ============================================================================
 * Start ArenaAutomation executable process
 * ============================================================================ */

app.get('/api/application/start', function(req, res){

    var appDir  = path.join(path.dirname(fs.realpathSync(__filename)), '../deploy/');
    var appFile = path.join(path.dirname(fs.realpathSync(__filename)), '../deploy/ArenaAutomation.exe');
    
    res.send("OK");

    console.log("Received request to start ArenaAutomation.exe");
    if (arenaAutomationProcess == null) {
        console.log("Starting ArenaAutomation.exe");
        arenaAutomationProcess = spawn(appFile, {
            cwd: appDir, stdio: 'inherit'
        }, function(error, stdout, stderr) {
            if (error!=null) {
                arenaAutomationProcess = null;
            }
            console.log("Started ArenaAutomation.exe");
        });
    }
});

app.get('/api/application/stop', function(req, res){

    res.send("OK");

    if (arenaAutomationProcess != null) {
        //console.log("Killing process: "+arenaAutomationProcess.pid);
        //process.kill(arenaAutomationProcess.pid);
        arenaAutomationProcess.kill();
        arenaAutomationProcess = null;
        console.log("Stopped ArenaAutomation.exe");
    }
});

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
                        log("Error in port "+devicePorts[relay]+": "+err);
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

app.get('/api/power/', function(req, res){

    res.send({'power': devicePorts});
});

/* ============================================================================
 * Process CableFlysim command
 * ============================================================================ */

var processCableFlysimCmd = function(cmd, res) {

    queue.push(function(callback){
        try{
            log(devicePorts["CABLEFLYSIM"]);
            var port = new serialport( devicePorts["CABLEFLYSIM"], {
                baudrate: 9600,
                parser: serialport.parsers.readline("\n")
            }, openCallback=function(err) {
                if (err) {
                    if (res) { res.send("{\"status\": \"error\", \"response\": \"\"}"); }
                    log("Error in CableFlysim port "+devicePorts["CABLEFLYSIM"]+": "+err);
                    port.close(function(err){
                        clearTimeout(timer); callback();
                    });
                } else {
                    var cmdp = cmd.replace('%20',' ') + '\n';
                    log("Sending CableFlysim Command: " + cmdp);
                    port.write(cmdp, function(){
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

    log("received cableflysim cmd: "+cmd);

    processCableFlysimCmd(cmd, res);
});

/* ============================================================================
 * Initialize COM/USB ports
 * ============================================================================ */

/* Try connecting to a port */
var fInit = function() {

    /* Report local IP address at which to find this server 
       See: http://stackoverflow.com/questions/3653065/get-local-ip-address-in-node-js */
    log("\n==============================================================", lvl='notification');
    log("Open one of the following links in your browser (not all are correct):", lvl='notification');
    Object.keys(ifaces).forEach(function (ifname) {
        var alias = 0;
        ifaces[ifname].forEach(function (iface) {
            
            if ('IPv4' !== iface.family || iface.internal !== false) {
                // skip over internal (i.e. 127.0.0.1) and non-ipv4 addresses
                return;
            }
            log("  o " + iface.address, lvl='notification');
        });
    });
    log("==============================================================\n", lvl='notification');

    log("Searching COM ports for power relay interfaces.");
    serialport.list(function (err, ports) {

        log("Found "+ports.length+" ports: ");
        for (var a=0;a<ports.length;a++) {
          log("  "+ports[a].comName);
        }

        var tryPort = function(i, callback) {
            /* Have we processed all available ports? */
            if (i >= ports.length) {
                log("Reached port number "+i);
                callback();
                return;
            }

            log("Trying port "+i+" ("+JSON.stringify(ports[i].comName)+")");

            /* Open the connection */
            var port = new serialport( ports[i].comName, {
                baudrate: 9600,
                parser: serialport.parsers.readline("\n")
            }, openCallback = function(err){
                if (err) {
                    log("Error connecting to port "+ports[i].comName+": "+err);
                } else {
                    log("Successfully opened serial port.");

                    port.write('h\n', function(){
                        port.drain(function(){});
                    });
                }
            } );

            /* Register data handler */
            port.on('data', function (data) {
                log("Received response from port "+i+": "+data);
                if ( data.indexOf('Power Relay Controller ') !== -1 ) {
                    /* Save this port */
                    idx = 'POWER_RELAY_' + data.substring('Power Relay Controller '.length).trim()
                    devicePorts[idx] = ports[i].comName;
                } else if ( data.indexOf('CableFlysim Controller') !== -1 ) {
                    devicePorts['CABLEFLYSIM'] = ports[i].comName;
                }
                log(JSON.stringify(devicePorts));
                port.close(function(){});
            });

            /* Try next port */
            setTimeout(function(){
              if (port.isOpen) {
                log("Port timed out. Closing port and moving on...")
              }
              port.close(function(){
                tryPort(i+1, callback);
              });
            }, 1000 )// 750);
        };
        tryPort(0, function(){

            log("Finished port discovery...");

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

log("Listening on port 80...");
app.listen(80)
