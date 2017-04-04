
var arenaApp = angular.module('arenaApp', ['ngRoute']);

/* ======================================================================================
   Configure Routes
====================================================================================== */

arenaApp.config(function($routeProvider) {
    $routeProvider

        // route for the home page
        .when('/', {
            redirectTo: '/log'
        })

        .when('/log', {
            templateUrl : 'log.html',
            controller  : 'mainController'
        })

        .when('/flysim', {
            templateUrl : 'flysim.html',
            controller  : 'flysimController'
        })

        .when('/cableflysim', {
            templateUrl : 'cableflysim.html',
            controller  : 'cableFlysimController'
        })

        // route for the about page
        .when('/power', {
            templateUrl : 'power.html',
            controller  : 'powerController'
        })


        // route for the contact page
        .when('/settings', {
            templateUrl : 'settings.html',
            controller  : 'settingsController'
        })

        // route for the about page
        .when('/cameras', {
            templateUrl : 'cameras.html',
            controller  : 'camerasController'
        });
});

/* ======================================================================================
   Main Controller
====================================================================================== */

arenaApp.controller('mainController', function($scope, $rootScope, $http) {

    $rootScope.host = { ip: "http://10.101.30.47:1000" /*ip: 'http://10.101.30.47:7894'*/ };
    $scope.log = [];
    $scope.running = false;
    $scope.connectingToServer = true;
    $scope.startingApplication = false;

    /* Start server functions */
    $scope.startApplicationServer = function() {

        $scope.startingApplication = true;
        $http({
            method: 'GET', 
            url: '/api/application/start'
        }).then(function successCallback(response) {
            $scope.startingApplication = false;
        }, function errorCallback(response) {
            $scope.startingApplication = false;
        });
    };
    
    $scope.stopApplicationServer = function() {

        $http({
            method: 'GET', 
            url: '/api/application/stop'
        }).then(
            function successCallback(response) {}, 
            function errorCallback(response) {}
        );
    };

    /* Start periodic check for new logging info */
    setInterval(function() {
        $http({
            method: 'GET',
            url: $rootScope.host.ip + '/api/log/*/0'
        }).then(function successCallback(response) {

            $scope.startingApplication = false;
            $scope.running = true;
            $scope.connectingToServer = false;
            
            $scope.log = response.data.log;

        }, function errorCallback(response) {
            
            $scope.startingApplication = false;
            $scope.running = false;
            $scope.connectingToServer = false;

            $scope.msg = "";
        });
    }, 1000);
});

/* ======================================================================================
   FlySim Controller
====================================================================================== */

arenaApp.controller('flysimController', function($scope, $rootScope, $http) {

    $scope.flysimCmd = '';

    $scope.sendFlySimCommand = function() {
         $http({
            method: 'GET',
            url: $rootScope.host.ip + '/api/flysim/' + $scope.flysimCmd
        }).then(function successCallback(response) {
        }, function errorCallback(response) {
        });
    };
});

/* ======================================================================================
   CableFlysim Controller
====================================================================================== */

arenaApp.controller('cableFlysimController', function($scope, $rootScope, $http) {

    $scope.cableflysimStatus = null;
    $scope.cableFlysimLog = [];
    $scope.strSerialCmd = "";

    // Register handler for serial communication
    $scope.doCableFlysimSerialReq = function(){
        
        $.getJSON("/api/cableflysim/"+$scope.strSerialCmd, function(data){
            if (data.response != undefined) {
                $scope.cableFlysimLog.push(data.response);
            }
        });
    };

    // Periodically request info from flysim
    var checkCableFlysim = function(){
        $.getJSON("/api/cableflysim/status +waypoints", function(data){
            
            // --------------------------------------------------------------------
            // Parse the message received from the microprocessor
            // --------------------------------------------------------------------
            
            try {
                $scope.cableflysimStatus = JSON.parse(data.response);
            } catch(err) {}
            
            /*
            $scope.cableflysimStatus.motorPositions = [
                [0,0,0],
                [500,0,0],
                [-1000,0,0],
                [1000,0,0],
                [0,0,-100],
                [500,0,-100],
                [-1000,0,-100],
                [1000,0,-100],
                [0,0,800],
                [500,0,800],
                [-1000,0,800],
                [1000,0,800]
            ];
            */

            // --------------------------------------------------------------------
            // Update motor locations
            // --------------------------------------------------------------------
            
            if ( $scope.webglMotors == null ) {
                
                $scope.webglMotors = [];
                
                var darkMaterial = new THREE.MeshBasicMaterial( { color: 0xffffcc } );
                var wireframeMaterial = new THREE.MeshBasicMaterial( { color: 0x000000, wireframe: true, transparent: true } ); 
                var multiMaterial = [ darkMaterial, wireframeMaterial ]; 

                for (var i = 0; i < $scope.cableflysimStatus.motorPositionsXYZ.length; i++) {
                    var shape = THREE.SceneUtils.createMultiMaterialObject( 
                        new THREE.OctahedronGeometry( 40, 1 ), 
                        multiMaterial );
                    
                    function rint(min,max)
                        { return Math.floor(Math.random()*(max-min+1)+min); }
                    
                    var p = $scope.cableflysimStatus.motorPositionsXYZ[i];
                    shape.position.set(p[0], p[2], p[1]); // The Y/Z convention is flipped
                    $scope.webglMotors.push( shape );
                    glScene.add( shape );
                }
            }

            for (var i = 0; i < $scope.cableflysimStatus.motorPositionsXYZ.length; i++) {
                var p = $scope.cableflysimStatus.motorPositionsXYZ[i];
                $scope.webglMotors[i].position.set( p[0], p[2], p[1] );
            }

            // --------------------------------------------------------------------
            // Update waypoint locations
            // --------------------------------------------------------------------

            if ($scope.cableflysimStatus.waypoints != null) {
                
                if ( $scope.webglWaypoints == null ) {
                
                $scope.webglWaypoints = [];
                
                var darkMaterial = new THREE.MeshBasicMaterial( { color: 0xaaaaff } );
                var wireframeMaterial = new THREE.MeshBasicMaterial( { color: 0x000000, wireframe: true, transparent: true } ); 
                var multiMaterial = [ darkMaterial, wireframeMaterial ]; 

                for (var i = 0; i < $scope.cableflysimStatus.waypoints.length; i++) {
                    
                    var shape = THREE.SceneUtils.createMultiMaterialObject( 
                        new THREE.OctahedronGeometry( 1, 1 ), 
                        multiMaterial );
                    
                    var p = $scope.cableflysimStatus.waypoints[i];
                    shape.position.set(p[0], p[2], p[1]); // The Y/Z convention is flipped
                    $scope.webglWaypoints.push( shape );
                    glScene.add( shape );
                }
                }

                for (var i = 0; i < $scope.cableflysimStatus.waypoints.length; i++) {
                    var p = $scope.cableflysimStatus.waypoints[i];
                    $scope.webglWaypoints[i].position.set( p[0], p[2], p[1] );
                }
            }

            // --------------------------------------------------------------------
            // Continue to periodically update the state
            // --------------------------------------------------------------------
            
            setTimeout(function(){
                checkCableFlysim();
            }, 1000);
        });
    };
    
    // ----------------------------------------------------------------------------
    // Create 3D visualization
    //
    // Visualization code largely based on code by Sue Lockwood, code at:
    //    <https://github.com/deathbearbrown/learning-three-js-blogpost/>
    // 
    // For more 3D shapes, see: 
    //   o https://stemkoski.github.io/Three.js/Shapes.html
    //   o view-source:https://stemkoski.github.io/Three.js/Shapes.html
    // ----------------------------------------------------------------------------

    if ( ! Detector.webgl ) Detector.addGetWebGLMessage();

    var container, stats;
    var views, glScene, glRenderer, camera, cssrenderer;
    var cssScene, cssRenderer;
    var light;
    var mouseX = 0, mouseY = 0;
    var realData;
    var startPosition;
    
    var windowWidth  = $("#cableFlysimCanvas").innerWidth();
    var windowHeight = $("#cableFlysimCanvas").innerHeight();

    var graphBounds = {
        xmin: -1000,
        xmax:  1000,
        ymin: -1000,
        ymax:  1000,
        zmin:  -100,
        zmax:   800
    }
    var tickstepsize = 100;

    var views = [ {
        left: 0,
        bottom: 0.3,
        width: 1.0,
        height: 0.7,
        background: new THREE.Color().setRGB( 0.95, 0.95, 0.95 ),
        eye: [ 0, 0, 3000 ],
        fov: 30
    }, {
        left: 0,
        bottom: 0,
        width: 0.3333,
        height: 0.3,
        background: new THREE.Color().setRGB( 0.95, 0.95, 0.95 ),
        eye: [ 3000, 0, 0 ],
        fov: 50
    }, {
        left: 0.3333,
        bottom: 0,
        width: 0.3333,
        height: 0.3,
        background: new THREE.Color().setRGB( 0.95, 0.95, 0.95 ),
        eye: [ 0, 3000, 0 ],
        fov: 50
    }, {
        left: 0.6666,
        bottom: 0,
        width: 0.333,
        height: 0.3,
        background: new THREE.Color().setRGB( 0.95, 0.95, 0.95 ),
        eye: [ 0, 0, 3000 ],
        fov: 50
    }];
    
    function labelAxis(direction){

        p = { x: (direction=='x'?graphBounds.xmin:0), 
              y: (direction=='y'?graphBounds.ymin:0), 
              z: (direction=='z'?graphBounds.zmin:0) };
        
        x0 = (direction=='x'?graphBounds.xmin:(direction=='y'?
            graphBounds.ymin:graphBounds.zmin))
        x1 = (direction=='x'?graphBounds.xmax:(direction=='y'?
            graphBounds.ymax:graphBounds.zmax))

        dobj = new THREE.Object3D();
        
        for ( var i = x0; i < x1; i += tickstepsize ) {
            p[direction] = i;
            var label = makeTextSprite(direction + "=" + i);
            label.position.set(p.x,p.z,p.y);
            dobj.add( label );
        }
        return dobj;
    }

    // This was written by Lee Stemkoski
    // https://stemkoski.github.io/Three.js/Sprite-Text-Labels.html
    function makeTextSprite( message, parameters )
    {
        if ( parameters === undefined ) parameters = {};

        var fontface = parameters["fontface"] || "Helvetica";
        var fontsize = parameters["fontsize"] || 70;
        var canvas = document.createElement('canvas');
        var context = canvas.getContext('2d');
        context.font = fontsize + "px " + fontface;

        // get size data (height depends only on font size)
        var metrics = context.measureText( message );
        var textWidth = metrics.width;
        
        // text color
        context.fillStyle = "rgba(0, 0, 0, 1.0)";
        context.fillText( message, 0, fontsize);

        // canvas contents will be used for a texture
        var texture = new THREE.Texture(canvas);
        texture.minFilter = THREE.LinearFilter;
        texture.needsUpdate = true;

        var spriteMaterial = new THREE.SpriteMaterial({ map: texture, useScreenCoordinates: false});
        var sprite = new THREE.Sprite( spriteMaterial );
        sprite.scale.set(100,50,1.0);
        return sprite;
    }

    // ----------------------------------------------------------------------------
    //  createAGrid
    //     opts {
    // 	     height: width, width: depth, linesHeight: b, linesWidth: c, color: 0xcccccc 
    // }
    // ----------------------------------------------------------------------------
    
    function createAGrid(opts){
        var config = opts || {
            color: 0xDD006C
        };

        var material = new THREE.LineBasicMaterial({
            color: config.color,
            opacity: 0.9
        });

        var gridObject = new THREE.Object3D(),
                gridGeo= new THREE.Geometry();

        // lines along x
        for ( var i = config.ymin; i <= config.ymax; i += tickstepsize ) {
            gridGeo.vertices.push( new THREE.Vector3( config.xmin, i, 0 ) );
            gridGeo.vertices.push( new THREE.Vector3( config.xmax, i, 0 ) );
        }

        // lines along y
        for ( var i = config.xmin; i <= config.xmax; i += tickstepsize ) {
            gridGeo.vertices.push( new THREE.Vector3( i, config.ymin, 0 ) );
            gridGeo.vertices.push( new THREE.Vector3( i, config.ymax, 0 ) );
        }

        var line = new THREE.Line( gridGeo, material, THREE.LinePieces );
        gridObject.add(line);

        return gridObject;
    }

    //----------------------------------------------------------
    // Initialize grids
    //----------------------------------------------------------

    function gridInit(){

        var boundingGrid = new THREE.Object3D();
        
        //pink
        var newGridXZ = createAGrid({
            xmin: graphBounds.xmin, 
            xmax: graphBounds.xmax, 
            ymin: graphBounds.zmin, 
            ymax: graphBounds.zmax,
            color: 0xcccccc
        });
        newGridXZ.position.z = graphBounds.ymin;
        boundingGrid.add(newGridXZ);

        //blue
        var newGridYZ = createAGrid({
            xmin: graphBounds.ymin, 
            xmax: graphBounds.ymax, 
            ymin: graphBounds.xmin, 
            ymax: graphBounds.xmax,
            color: 0xcccccc
        });
        newGridYZ.rotation.x = Math.PI/2;
        newGridYZ.position.y = graphBounds.zmin;
        boundingGrid.add(newGridYZ);

        //green
        var newGridXY = createAGrid({
            xmin: graphBounds.ymin, 
            xmax: graphBounds.ymax, 
            ymin: graphBounds.zmin, 
            ymax: graphBounds.zmax,
            color: 0xcccccc
        });

        newGridXY.position.x = graphBounds.ymin;
        newGridXY.rotation.y = Math.PI/2;
        boundingGrid.add(newGridXY);

        glScene.add(boundingGrid);
        
        var labelsW = labelAxis("x");
        labelsW.position.x = 0;
        labelsW.position.y = 0;
        labelsW.position.z = 0; //graphBounds.zmin;
        glScene.add(labelsW);

        var labelsH = labelAxis("y");
        labelsH.position.x = 0;
        labelsH.position.y = 0; //graphBounds.zmax;
        labelsH.position.z = 0;
        glScene.add(labelsH);
        
        var labelsD = labelAxis("z");
        labelsD.position.x = 0;
        labelsD.position.y = 0;
        labelsD.position.z = 0;
        glScene.add(labelsD);
    };

    function init() {

        container = document.getElementById( 'cableFlysimCanvas' );

        // ----------------------------------------------------------------------------
        //   Create scenes for webGL
        // ----------------------------------------------------------------------------
        
        glScene = new THREE.Scene();
        
        // ----------------------------------------------------------------------------
        //    Add a light source & create Canvas
        // ----------------------------------------------------------------------------
        
        light = new THREE.DirectionalLight( 0xffffff );
        light.position.set( 0, 0, 1 );
        glScene.add( light );

        // create canvas
        var canvas = document.createElement( 'canvas' );
        canvas.width = 128;
        canvas.height = 128;

        var context = canvas.getContext( '2d' );

        gridInit();

        // ----------------------------------------------------------------------------
        //    SET UP RENDERERS
        // ----------------------------------------------------------------------------
        
        //set up webGL renderer
        glRenderer = new THREE.WebGLRenderer();
        glRenderer.setPixelRatio( window.devicePixelRatio );
        glRenderer.setClearColor( 0xf0f0f0 );
        glRenderer.setSize( windowWidth, windowHeight);
        container.appendChild( glRenderer.domElement );

        // set up window resize listener
        window.addEventListener( 'resize', onWindowResize, false );
        animate();

        // ----------------------------------------------------------------------------
        //    Init multiple viewpoints
        // ----------------------------------------------------------------------------
        
        for (var ii =  0; ii < views.length; ++ii ) {

            var view = views[ii];
            var camera = null;
            
            if (ii == 0) {
                camera = new THREE.PerspectiveCamera( 
                    view.fov, windowWidth / windowHeight, 1, 10000 );
            } else {
                camera = new THREE.OrthographicCamera( 
                    0, windowWidth, 0, windowHeight, 100, 10000 );
            }
            
            var controls = controls = new THREE.TrackballControls( camera, container );
            controls.damping = 0.2;
            
            if (ii !=0 ) {
                controls.noRotate = true;
            }

            camera.position.x = view.eye[ 0 ];
            camera.position.y = view.eye[ 1 ];
            camera.position.z = view.eye[ 2 ];
            
            controls.addEventListener( 'change', render );

            view.camera = camera;
            view.controls = controls;
        }
        
        
        // ----------------------------------------------------------------------------
        // Start update loop
        // ----------------------------------------------------------------------------
        
        checkCableFlysim();
    }

    // ----------------------------------------------------------------------------
    //	Animate
    // ----------------------------------------------------------------------------
    
    function animate() {
        requestAnimationFrame(animate);
        for ( var ii = 0; ii < views.length; ++ii ) {
            if (views[ii].controls != null) {
                views[ii].controls.update();
            }
        }
    }

    function updateSize() {
        var ww  = $("#cableFlysimCanvas").innerWidth();
        var wh = $("#cableFlysimCanvas").innerHeight();
        
        if ( windowWidth != ww || windowHeight != wh ) {
            windowWidth  = ww;
            windowHeight = wh;
            glRenderer.setSize ( windowWidth, windowHeight );
        }
    }
    
    function render() {

        updateSize();

        for ( var ii = 0; ii < views.length; ++ii ) {
            view = views[ii];
            camera = view.camera;
            // view.updateCamera( camera, glScene );
            var left   = Math.floor( windowWidth  * view.left );
            var bottom = Math.floor( windowHeight * view.bottom );
            var width  = Math.floor( windowWidth  * view.width );
            var height = Math.floor( windowHeight * view.height );
            glRenderer.setViewport( left, bottom, width, height );
            glRenderer.setScissor( left, bottom, width, height );
            glRenderer.setScissorTest( true );
            glRenderer.setClearColor( view.background );
            camera.aspect = width / height;
            camera.updateProjectionMatrix();
            glRenderer.render( glScene, camera );
        }

        /*
        camera.lookAt( glScene.position );
        glRenderer.render( glScene, camera );
        */
    }

    // ----------------------------------------------------------------------------
    // ON RESIZE
    // ----------------------------------------------------------------------------
    
    function onWindowResize() {

        camera.aspect = windowWidth / windowHeight;
        camera.updateProjectionMatrix();

        glRenderer.setSize( windowWidth, windowHeight );
        render();

    }

    // Initialize
    init();
    render();
});

/* ======================================================================================
   Power Controller
====================================================================================== */

arenaApp.controller('powerController', function($scope, $rootScope, $http) {

    $scope.powerState = '';
    $scope.allPowerOn = 0;
    $scope.powerDevices = {};
    $scope.powerState = {};

    /* Periodically check for power interfaces */
    setInterval(function() {
        $http({
            method: 'GET',
            url: '/api/power'
        }).then(function successCallback(response) {

            $scope.powerDevices = response.data.power;

        }, function errorCallback(response) {});

        angular.forEach($scope.powerDevices, function(value, key) {
            $http.get('/api/power/'+key+'/s').then(function(response1){
                var s = JSON.parse(response1.data.response.replace(",}","}"));
                s.relaysOn = 'none';
                if (s.state.every(function(el, index, array){ return el == 1; })) {
                    s.relaysOn = 'all';
                } else if (s.state.some(function(el, index, array){ return el == 1; })) {
                    s.relaysOn = 'some';
                }
                s.id = key;
                $scope.powerState[key] = s;
            });
        });
    }, 1000);

    $scope.switchRelay = function(device, newstate) {
        $http({
            method: 'GET',
            url: '/api/power/' + device.id + '/' + (newstate==1?'+':'-')
        }).then(function successCallback(response) {
        }, function errorCallback(response) {});
    };
});

/* ======================================================================================
   Settings Controller
====================================================================================== */

arenaApp.controller('settingsController', function($scope, $rootScope, $http) {

   if ($("#treema").treema != null) {
        $http.get('settings.schema.json')
            .then(function(res){
                var schema = res.data.data;
                $http.get( $rootScope.host.ip + '/api/settings/get')
                    .then(function(res) {
                        $rootScope.settings = res.data;
                        var options = { schema: schema, data: $rootScope.settings };
                        $scope.treema = $("#treema").treema(options); 
                        $scope.treema.build();
                });     
            });   
    }

    // Sync settings occasionally
    setInterval(function() {
        if (!_.isEqual($scope.uploadedSettings, $rootScope.settings)) {
            $http.post(
                $rootScope.host.ip + '/api/settings/set', 
                $rootScope.settings, null).
                    then(function successCallback(response) {
                
                $scope.uploadedSettings =  _.cloneDeep($rootScope.settings);

            }, function errorCallback(response) {
                
                $scope.msg = "Lost communication with server.";
            });
        }
    }, 1000);
});

/* ======================================================================================
   Camera Controller
====================================================================================== */

arenaApp.controller('camerasController', function($scope, $rootScope, $http) {

    $scope.cameras = {
        'photron': [{},{}],
        'usb': [{},]
    }

    $scope.recordSample = function() {
        $http.get($rootScope.host.ip+'/api/camera/save').then(function(response){}, function(response){});
        return false;
    };
    $scope.triggerTTL = function() {
        $http.get($rootScope.host.ip+'/api/hardware/trigger').then(function(response){}, function(response){});
        return false;
    };
    
    setInterval(function(){
        for (var i = 0; i < $scope.cameras.length; i++) {
            image = new Image();
            if (angular.element(".img-live-camera-"+i)[0] != null) {
                image.onload = (function(_image, _i){ return function () {
                    var canvas = angular.element(".img-live-camera-"+_i)[0];
                    canvas.width = 1024;
                    canvas.height = 1024;
                    canvas.getContext('2d').drawImage(this, 0, 0, 1024, 1024);
                }})(image, i);
                image.src = $rootScope.host.ip + '/api/camera/photron/'+i;
            }
        }
    }, 1000);

    for (var type in $scope.cameras) {
        for (var cameraID in $scope.cameras[type]) {
            var timerId = setInterval((function(_cameraID, _type){ return function () {
                if (angular.element(".img-"+_type+"-camera-"+_cameraID)[0] != null) {
                    var image = new Image();
                    image.onload = function () {
                        var canvas = angular.element(".img-"+_type+"-camera-"+_cameraID)[0];
                        canvas.width = image.naturalWidth;
                        canvas.height = image.naturalHeight;
                        canvas.getContext('2d').drawImage(this, 0, 0);
                    };
                    image.src = $rootScope.host.ip + '/api/camera/'+_type+'/'+_cameraID+'/'+ Math.floor(Math.random()*10000);
                }
            }})(cameraID, type), cameraID * 600 + Object.keys($scope.cameras).length * 450);
        }
    }
});
