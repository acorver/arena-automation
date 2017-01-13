﻿
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

    if ( ! Detector.webgl ) Detector.addGetWebGLMessage();

    var container, stats;

    var camera, controls, scene, renderer;

    var cross;

    init();
    animate();

    function init() {

        camera = new THREE.PerspectiveCamera( 60, window.innerWidth / window.innerHeight, 1, 1000 );
        camera.position.z = 500;

        controls = new THREE.TrackballControls( camera );

        controls.rotateSpeed = 1.0;
        controls.zoomSpeed = 1.2;
        controls.panSpeed = 0.8;

        controls.noZoom = false;
        controls.noPan = false;

        controls.staticMoving = true;
        controls.dynamicDampingFactor = 0.3;

        controls.keys = [ 65, 83, 68 ];

        controls.addEventListener( 'change', render );

        // world

        scene = new THREE.Scene();
        scene.fog = new THREE.FogExp2( 0xcccccc, 0.002 );

        var geometry = new THREE.CylinderGeometry( 0, 10, 30, 4, 1 );
        var material =  new THREE.MeshPhongMaterial( { color:0xffffff, shading: THREE.FlatShading } );

        for ( var i = 0; i < 500; i ++ ) {

            var mesh = new THREE.Mesh( geometry, material );
            mesh.position.x = ( Math.random() - 0.5 ) * 1000;
            mesh.position.y = ( Math.random() - 0.5 ) * 1000;
            mesh.position.z = ( Math.random() - 0.5 ) * 1000;
            mesh.updateMatrix();
            mesh.matrixAutoUpdate = false;
            scene.add( mesh );

        }


        // lights

        light = new THREE.DirectionalLight( 0xffffff );
        light.position.set( 1, 1, 1 );
        scene.add( light );

        light = new THREE.DirectionalLight( 0x002288 );
        light.position.set( -1, -1, -1 );
        scene.add( light );

        light = new THREE.AmbientLight( 0x222222 );
        scene.add( light );


        // renderer

        renderer = new THREE.WebGLRenderer( { antialias: false } );
        renderer.setClearColor( scene.fog.color );
        renderer.setPixelRatio( window.devicePixelRatio );
        renderer.setSize( window.innerWidth, window.innerHeight );

        container = document.getElementById( 'container' );
        container.appendChild( renderer.domElement );

        stats = new Stats();
        container.appendChild( stats.dom );

        //

        window.addEventListener( 'resize', onWindowResize, false );
        //

        render();

    }

    function onWindowResize() {

        camera.aspect = window.innerWidth / window.innerHeight;
        camera.updateProjectionMatrix();

        renderer.setSize( window.innerWidth, window.innerHeight );

        controls.handleResize();

        render();

    }

    function animate() {

        requestAnimationFrame( animate );
        controls.update();

    }

    function render() {

        renderer.render( scene, camera );
        stats.update();

    }
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

/* ======================================================================================
   Motion Controller
====================================================================================== */

// standard global variables
var container, scene, camera, renderer, controls;
var clock = new THREE.Clock();

// custom global variables
var mesh;

arenaApp.controller('motionController', function($scope, $rootScope) {

    var init = function() {
        // SCENE
        scene = new THREE.Scene();
        // CAMERA
        var SCREEN_WIDTH = window.innerWidth, SCREEN_HEIGHT = window.innerHeight;
        var VIEW_ANGLE = 45, ASPECT = SCREEN_WIDTH / SCREEN_HEIGHT, NEAR = 0.1, FAR = 20000;
        camera = new THREE.PerspectiveCamera( VIEW_ANGLE, ASPECT, NEAR, FAR);
        scene.add(camera);
        camera.position.set(0,150,400);
        camera.lookAt(scene.position);
        // RENDERER
        if ( Detector.webgl )
            renderer = new THREE.WebGLRenderer( {antialias:true} );
        else
            renderer = new THREE.CanvasRenderer();
        renderer.setSize(SCREEN_WIDTH, SCREEN_HEIGHT);
        container = document.getElementById( 'ThreeJS' );
        container.appendChild( renderer.domElement );
        // CONTROLS
        controls = new THREE.OrbitControls( camera, renderer.domElement );
        // LIGHT
        var light = new THREE.PointLight(0xffffff);
        light.position.set(100,250,100);
        scene.add(light);
        /*
        // FLOOR
        var floorTexture = new THREE.ImageUtils.loadTexture( 'images/checkerboard.jpg' );
        floorTexture.wrapS = floorTexture.wrapT = THREE.RepeatWrapping;
        floorTexture.repeat.set( 10, 10 );
        var floorMaterial = new THREE.MeshBasicMaterial( { map: floorTexture, side: THREE.DoubleSide } );
        var floorGeometry = new THREE.PlaneGeometry(1000, 1000, 10, 10);
        var floor = new THREE.Mesh(floorGeometry, floorMaterial);
        floor.position.y = -0.5;
        floor.rotation.x = Math.PI / 2;
        scene.add(floor);
        */
        // SKYBOX
        var skyBoxGeometry = new THREE.CubeGeometry( 10000, 10000, 10000 );
        var skyBoxMaterial = new THREE.MeshBasicMaterial( { color: 0x9999ff, side: THREE.BackSide } );
        var skyBox = new THREE.Mesh( skyBoxGeometry, skyBoxMaterial );
        scene.add(skyBox);

        ////////////
        // CUSTOM //
        ////////////

        var geometry = new THREE.SphereGeometry( 30, 32, 16 );
        var material = new THREE.MeshLambertMaterial( { color: 0x000088 } );
        mesh = new THREE.Mesh( geometry, material );
        mesh.position.set(40,40,40);
        scene.add(mesh);

        var axes = new THREE.AxisHelper(50);
        axes.position = mesh.position;
        scene.add(axes);

        var gridXZ = new THREE.GridHelper(100, 10);
        gridXZ.setColors( new THREE.Color(0x006600), new THREE.Color(0x006600) );
        gridXZ.position.set( 100,0,100 );
        scene.add(gridXZ);

        var gridXY = new THREE.GridHelper(100, 10);
        gridXY.position.set( 100,100,0 );
        gridXY.rotation.x = Math.PI/2;
        gridXY.setColors( new THREE.Color(0x000066), new THREE.Color(0x000066) );
        scene.add(gridXY);

        var gridYZ = new THREE.GridHelper(100, 10);
        gridYZ.position.set( 0,100,100 );
        gridYZ.rotation.z = Math.PI/2;
        gridYZ.setColors( new THREE.Color(0x660000), new THREE.Color(0x660000) );
        scene.add(gridYZ);

        // direction (normalized), origin, length, color(hex)
        var origin = new THREE.Vector3(50,100,50);
        var terminus  = new THREE.Vector3(75,75,75);
        var direction = new THREE.Vector3().subVectors(terminus, origin).normalize();
        var arrow = new THREE.ArrowHelper(direction, origin, 50, 0x884400);
        scene.add(arrow);
    }

    var animate = function() {
        requestAnimationFrame( animate );
        render();
        update();
    }

    var update = function () {
        controls.update();
    }

    var render = function() {
        renderer.render( scene, camera );
    }

    init();
    animate();

});
