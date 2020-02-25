<h1>Core concepts</h1>

This section summarizes the main features and modules in CARLA. While this page is just an overview, the rest of the information can be found in their respective pages, including fragments of code and in-depth explanations.  
In order to learn everything about the different classes and methods in the API, take a look at the [Python API reference](python_api.md). There is also another reference named [Code recipes](python_cookbook.md) containing some of the most common fragments of code regarding different functionalities that could be specially useful during these first steps. 

  * [__First steps__](#first-steps)  
	* 1st. World and client
	* 2nd. Actors and blueprints
	* 3rd. Maps and navigation
	* 4th. Sensors and data
  * [__Advanced steps__](#advanced-steps)  

!!! Important
    **This documentation refers to CARLA 0.9.X**. <br>
    The API changed significantly from previous versions (0.8.X). There is another documentation regarding those versions that can be found [here](https://carla.readthedocs.io/en/stable/getting_started/). 

---------------
##First steps

<h4>1st. World and client</h4>

The client is the module the user runs to ask for information or changes in the simulation. It communicates with the server via terminal. A client runs with an IP and a specific port. There can be many clients running at the same time, although multiclient managing needs a full comprehension on CARLA in order to make things work properly.  

The world is an object representing the simulation, an abstract layer containing the main methods to manage it: spawn actors, change the weather, get the current state of the world, etc. There is only one world per simulation, but it will be destroyed and substituted for a new one when the map is changed. 

<h4>2nd. Actors and blueprints</h4>
In CARLA, an actor is anything that plays a role in the simulation. That includes:  

* Vehicles.
* Walkers.
* Sensors.
* The spectator.
* Traffic signs and traffic lights.

A blueprint is needed in order to spawn an actor. __Blueprints__ are a set of already-made actor layouts: models with animations and different attributes. Some of these attributes can be set by the user, others don't. There is a library provided by CARLA containing all the available blueprints and the information regarding them. Visit the [Blueprint library](bp_library.md) to learn more about this. 

<h4>3rd. Maps and navigation</h4>

The map is the object representing the model of the world. There are many maps available, seven by the time this is written, and all of them use OpenDRIVE 1.4 standard to describe the roads.  
Roads, lanes and junctions are managed by the API to be accessed using different classes and methods. These are later used along with the waypoint class to provide vehicles with a navigation path.  
Traffic signs and traffic lights have bounding boxes placed on the road that make vehicles aware of them and their current state in order to set traffic conditions.

<h4>4th. Sensors and data</h4>

Sensors are one of the most important actors in CARLA and their use can be quite complex. A sensor is attached to a parent vehicle and follows it around, gathering information of the surroundings for the sake of learning. Sensors, as any other actor, have blueprints available in the [Blueprint library](bp_library.md) that correspond to the types available. Currently, these are:  

* Cameras (RGB, depth and semantic segmentation).  
* Collision detector.  
* Gnss sensor.  
* IMU sensor.  
* Lidar raycast.  
* Lane invasion detector.  
* Obstacle detector.  
* Radar.  

Sensors wait for some event to happen to gather data and then call for a function defining what they should do. Depending on which, sensors retrieve different types of data in different ways and their usage varies substantially. 

---------------
##Advanced steps  

Some more complex elements and features in CARLA are listed here to make newcomers familiar with their existence. However it is highly encouraged to first take a closer look to the pages regarding the first steps in order to learn the basics. 

  - **Recorder:** CARLA feature that allows for reenacting previous simulations using snapshots of the world.  
  - **Rendering options:** Some advanced configuration options in CARLA that allow for different graphics quality, off-screen rendering and a no-rendering mode. 
  - **Simulation time and synchrony:** Everything regarding the simulation time and how does the server run the simulation depending on clients.  
  - **Traffic manager:** This module is in charge of every vehicle set to autopilot mode. It conducts the traffic in the city for the simulation to look like a real urban environment. 