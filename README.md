# Abstract
This package is a Motion capture OptiTrack driver for ROS. You can receive rigid body pose from the natnet.

# Setup

## Create a workspace and clone the souces
```
$ mkdir ~/motion_ws
$ cd ~/motion_ws
$ mkdir src
$ cd src
$ catkin_init_workspace
$ git clone https://github.com/kentegrate/mocap_ros
```

## Build the workspace
```
$ cd ~/motion_ws
$ catkin_make
```

## Add the workspace setup.bash to .bashrc
```
$ echo "source ~/motion_ws/devel/setup.bash" >> ~/.bashrc
$ source ~/.bashrc
```


# Configuration

## Register the IP addresses of your machine and the motion capture host machine.
```
$ nano ~/motion_ws/src/mocap_ros/launch/mocap.launch
```
```
<launch>
	<param name="mocap_ros/local_addr" type="str" value="192.168.11.3"/>
        <param name="mocap_ros/server_addr" type="str" value="192.168.11.4"/>
	<node pkg="mocap_ros" type="mocap_ros_node" name="mocap_ros_node"/>
</launch>
```
Replace 192.168.11.3 with the IP address of the machine you are running this code, and replace 192.168.11.4 with the IP address of the natnet host machine.

# Run
```
$ roslaunch mocap_ros mocap.launch
```

# Topics
## Published topics
### /mocap/pose
Type : geometry_msgs::PoseStamped

This is the 6DoF Position of the rigid body broadcasted from the natnet with timestamp.


# Notes
This ROS package currently supports only one rigid body publishing. Make sure that you have only one rigid body broadcasted on the natnet.

