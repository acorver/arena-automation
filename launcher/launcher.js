
/* Imports */
var express = require('express')
var serveStatic = require('serve-static')
var serialport = require('serialport');

/* Start the application */
var app = express()

/* Declare global variables */
var availablePorts = [];
var currentPort    = null;

/* Serve static web interface content */
app.use(serveStatic('.', {'index': ['index.html']}))

/* Custom handler for turning power relays on/off */
app.get('/api/power/:relayid/:state', function(req, res){
    
    var relay = req.params.relayid;
    var state = req.params.state;

    /* Try connection to a port */
    var tryCOMPort = function(idx) {

        /* Have we run out of ports to try? */
        if (idx > availablePorts.length) {
            return null;
        }

        /* Open the connection */
        var port = new serialport( availablePorts[idx], {
            baudrate: 9600,
            parser: serialport.parsers.readline("\n")
        });

        /* Register data handler */
        port.on('data', function (data) {
            if ( data.contains('Power Relay Controller') ) {
                /* Use this port for our communications */
                return port;
            } else {
                /* Close this port, open next one */
                return tryCOMPort( idx + 1);
            }
        });
    };

    /* Open the correct COM port */
    var port = tryCOMPort(0);

    /* Write */
    if (port != null) {
        port.write('h', function(err) {
            // ...
        });
    }
});

// TODO: Add button/status message indicating whether app is running / recording, e.g. by pinging it every few seconds.
// Add button to launch application (at least if it's not already running, o/w relaunch)

app.listen(80)
