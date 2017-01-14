
var arenaApp = angular.module('arenaApp', ['ngRoute']);

/* ======================================================================================
   Configure Routes
====================================================================================== */

arenaApp.config(function($routeProvider) {
    $routeProvider

        // route for the home page
        .when('/', {
            templateUrl : 'log.html',
            controller  : 'mainController'
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
        })

        // route for the contact page
        .when('/motion', {
            templateUrl : 'motion.html',
            controller  : 'motionController'
        });
});

/* ======================================================================================
   Main Controller
====================================================================================== */

arenaApp.controller('mainController', function($scope, $rootScope, $http) {

    $rootScope.host = { ip: "http://10.101.30.47:1000" /*ip: 'http://10.101.30.47:7894'*/ };
    $scope.log = [];

    /* Start periodic check for new logging info */
    setInterval(function() {
        $http({
            method: 'GET',
            url: $rootScope.host.ip + '/api/log/*/0'
        }).then(function successCallback(response) {

            $scope.log = response.data.log;

        }, function errorCallback(response) {
            
            $scope.msg = "Lost communication with server.";
        });
    }, 1000);
});

/* ======================================================================================
   FlySim Controller
====================================================================================== */

arenaApp.controller('flysimController', function($scope, $rootScope, $http) {

    
});

/* ======================================================================================
   CableFlysim Controller
====================================================================================== */

arenaApp.controller('cableFlysimController', function($scope, $rootScope, $http) {

    $scope.cableflysimStatus = null;

    // Periodically request info from flysim
    var checkCableFlysim = function(){
        $.getJSON("/api/cableflysim/status", function(data){
            
            // --------------------------------------------------------------------
            // Parse the message received from the microprocessor
            // --------------------------------------------------------------------
            
            try {
                $scope.cableflysimStatus = JSON.parse(data.response);
            } catch(err) {}
            
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

            // --------------------------------------------------------------------
            // Update motor locations
            // --------------------------------------------------------------------
            
            if ( $scope.webglMotors == null ) {
                
                $scope.webglMotors = [];
                
                var darkMaterial = new THREE.MeshBasicMaterial( { color: 0xffffcc } );
                var wireframeMaterial = new THREE.MeshBasicMaterial( { color: 0x000000, wireframe: true, transparent: true } ); 
                var multiMaterial = [ darkMaterial, wireframeMaterial ]; 

                for (var i = 0; i < $scope.cableflysimStatus.motorPositions.length; i++) {
                    var shape = THREE.SceneUtils.createMultiMaterialObject( 
                        new THREE.OctahedronGeometry( 40, 1 ), 
                        multiMaterial );

                    function rint(min,max)
                        { return Math.floor(Math.random()*(max-min+1)+min); }
                    
                    var p = $scope.cableflysimStatus.motorPositions[i];
                    shape.position.set(p[0], p[2], p[1]); // The Y/Z convention is flipped
                    $scope.webglMotors.push( shape );
                    glScene.add( shape );
                }
            }

            for (var i = 0; i < $scope.cableflysimStatus.motorPositions.length; i++) {
                //$scope.webglMotors[i].position.set( $scope.cableflysimStatus.motorPositions[i] );
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
        //   Set up camera
        // ----------------------------------------------------------------------------
        vFOVRadians = 2 * Math.atan( windowHeight / ( 2 * 1500 ) ),
        //fov = vFOVRadians * 180 / Math.PI;
        fov = 40; 
        startPosition = new THREE.Vector3( 0, 0, 3000 );
        camera = new THREE.PerspectiveCamera( fov, windowWidth / windowHeight, 1, 30000 );
        camera.position.set( startPosition.x, startPosition.y, startPosition.z );


        controls = new THREE.TrackballControls( camera, container );
        controls.damping = 0.2;
        controls.addEventListener( 'change', render );

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

        // ----------------------------------------------------------------------------
        //    data
        // ----------------------------------------------------------------------------
        
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
        // Start update loop
        // ----------------------------------------------------------------------------
        
        checkCableFlysim();
    }

    // ----------------------------------------------------------------------------
    //	Animate
    // ----------------------------------------------------------------------------
    
    function animate() {
        requestAnimationFrame(animate);
        controls.update();
    }

    function render() {
        camera.lookAt( glScene.position );
        glRenderer.render( glScene, camera );
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
});

/* ======================================================================================
   Power Controller
====================================================================================== */

arenaApp.controller('powerController', function($scope, $rootScope, $http) {

    $scope.powerState = '';
    $scope.allPowerOn = 0;

    $scope.allOn = function(allOn) {
        var x = '';
        if (allOn) { x = '+'; } else { x = '-'; }
        $http.get('/api/power/1/'+x).then(function(response1){});
        $http.get('/api/power/2/'+x).then(function(response1){});
    };

    setInterval(function(){
        
        $http.get('/api/power/1/s').then(function(response1){
           $http.get('/api/power/2/s').then(function(response2) {
               var s = {};
               var d1 = JSON.parse(response1.data.response.replace(",}","}"));
               var d2 = JSON.parse(response2.data.response.replace(",}","}"));
               for (var k in d1) { s['1.'+k] = d1[k]; }
               for (var k in d2) { s['2.'+k] = d2[k]; }
               $scope.powerState = JSON.stringify(s);
           });
        });
    }, 5000);
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

    /*
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
    */

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
