/*
 * SimpleExample.cpp is part of NatNetLinux, and is Copyright 2013-2014,
 * Philip G. Lee <rocketman768@gmail.com>
 *
 * NatNetLinux is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * NatNetLinux is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NatNetLinux.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <mocap_ros/NatNet.h>
#include <mocap_ros/CommandListener.h>
#include <mocap_ros/FrameListener.h>

#include <boost/program_options.hpp>
#include <time.h>

#include <ros/ros.h>
#include <geometry_msgs/PoseStamped.h>

class Globals
{
public:
   
   // Parameters read from the command line
   static uint32_t localAddress;
   static uint32_t serverAddress;
   
   // State of the main() thread.
   static bool run;
};
uint32_t Globals::localAddress = 0;
uint32_t Globals::serverAddress = 0;
bool Globals::run = false;
ros::Publisher motion_pub;
// End the program gracefully.
void terminate(int)
{
   // Tell the main() thread to close.
   Globals::run = false;
}

// Set the global addresses from the command line.
void readOpts( int argc, char* argv[] )
{
   namespace po = boost::program_options;
   
   po::options_description desc("simple-example: demonstrates using NatNetLinux\nOptions");
   desc.add_options()
      ("help", "Display help message")
      ("local-addr,l", po::value<std::string>(), "Local IPv4 address")
      ("server-addr,s", po::value<std::string>(), "Server IPv4 address")
   ;
   
   po::variables_map vm;
   po::store(po::parse_command_line(argc,argv,desc), vm);
   
   if(
      argc < 5 || vm.count("help") ||
      !vm.count("local-addr") ||
      !vm.count("server-addr")
   )
   {
      std::cout << desc << std::endl;
      exit(1);
   }
   
   Globals::localAddress = inet_addr( vm["local-addr"].as<std::string>().c_str() );
   Globals::serverAddress = inet_addr( vm["server-addr"].as<std::string>().c_str() );
}

geometry_msgs::PoseStamped natNet2PoseMsg(Point3f location, Quaternion4f orientation){
  geometry_msgs::PoseStamped msg;
  msg.header.stamp = ros::Time::now();
  msg.header.frame_id = "base_link";
  msg.pose.position.x = location.x;
  msg.pose.position.y = -location.z;
  msg.pose.position.z = location.y;

  msg.pose.orientation.x = orientation.qx;
  msg.pose.orientation.y = -orientation.qz;
  msg.pose.orientation.z = orientation.qy;
  msg.pose.orientation.w = orientation.qw;

  return msg;
}
// This thread loop just prints frames as they arrive.

void printFrames(FrameListener& frameListener)
{
   bool valid;
   MocapFrame frame;
   Globals::run = true;
   while(Globals::run)
   {
      while( true )
      {
         // Try to get a new frame from the listener.
         MocapFrame frame(frameListener.pop(&valid).first);
         // Quit if the listener has no more frames.
         if( !valid )
            break;
	 //         std::cout << frame << std::endl;
	 std::vector<RigidBody> const& rBodies = frame.rigidBodies();
	 for(int i = 0; i < 1; i++){
	   geometry_msgs::PoseStamped msg = natNet2PoseMsg(rBodies[i].location(), rBodies[i].orientation());
	   motion_pub.publish(msg);
	 }
      }
      
      // Sleep for a little while to simulate work :)
      usleep(1000);
   }
}

// This thread loop collects inter-frame arrival statistics and prints a
// histogram at the end. You can plot the data by copying it to a file
// (say time.txt), and running gnuplot with the command:
//     gnuplot> plot 'time.txt' using 1:2 title 'Time Stats' with bars
void timeStats(FrameListener& frameListener, const float diffMin_ms = 0.5, const float diffMax_ms = 7.0, const int bins = 100)
{
   size_t hist[bins];
   float diff_ms;
   int bin;
   struct timespec current;
   struct timespec prev;
   struct timespec tmp;
   
   std::cout << std::endl << "Collecting inter-frame arrival statistics...press ctrl-c to finish." << std::endl;
   
   memset(hist, 0x00, sizeof(hist));
   bool valid;
   Globals::run = true;
   while(Globals::run)
   {
      while( true )
      {
         // Try to get a new frame from the listener.
         prev = current;
         tmp = frameListener.pop(&valid).second;
         // Quit if the listener has no more frames.
         if( !valid )
            break;
         
         current = tmp;
         
         diff_ms =
            std::abs(
               (static_cast<float>(current.tv_sec)-static_cast<float>(prev.tv_sec))*1000.f
                + (static_cast<float>(current.tv_nsec)-static_cast<float>(prev.tv_nsec))/1000000.f
            );
         
         bin = (diff_ms-diffMin_ms)/(diffMax_ms-diffMin_ms) * (bins+1);
         if( bin < 0 )
            bin = 0;
         else if( bin >= bins )
            bin = bins-1;
         
         hist[bin] += 1;
      }
      
      // Sleep for a little while to simulate work :)
      usleep(1000);
   }
   
   // Print the stats
   std::cout << std::endl << std::endl;
   std::cout << "# Time diff (ms), Count" << std::endl;
   for( bin = 0; bin < bins; ++bin )
      std::cout << diffMin_ms+(diffMax_ms-diffMin_ms)*(0.5f+bin)/bins << ", " << hist[bin] << std::endl;
}

int main(int argc, char* argv[])
{

  ros::init(argc, argv, "mocap_ros");
  ros::NodeHandle nh;
  motion_pub = nh.advertise<geometry_msgs::PoseStamped>("mocap/pose", 10);
  
   // Version number of the NatNet protocol, as reported by the server.
   unsigned char natNetMajor;
   unsigned char natNetMinor;
   
   // Sockets
   int sdCommand;
   int sdData;
   
   // Catch ctrl-c and terminate gracefully.
   signal(SIGINT, terminate);
   
   // Set addresses
   //   readOpts( argc, argv );
   std::string local_addr, server_addr;
   nh.getParam("mocap_ros/local_addr", local_addr);
   nh.getParam("mocap_ros/server_addr", server_addr);
   Globals::serverAddress = inet_addr(server_addr.c_str());
   Globals::localAddress = inet_addr(local_addr.c_str());   
   // Use this socket address to send commands to the server.
   struct sockaddr_in serverCommands = NatNet::createAddress(Globals::serverAddress, NatNet::commandPort);
   
   // Create sockets
   sdCommand = NatNet::createCommandSocket( Globals::localAddress );
   sdData = NatNet::createDataSocket( Globals::localAddress );
   
   // Start the CommandListener in a new thread.
   CommandListener commandListener(sdCommand);
   commandListener.start();

   // Send a ping packet to the server so that it sends us the NatNet version
   // in its response to commandListener.
   NatNetPacket ping = NatNetPacket::pingPacket();
   ping.send(sdCommand, serverCommands);
   
   // Wait here for ping response to give us the NatNet version.
   commandListener.getNatNetVersion(natNetMajor, natNetMinor);
   
   // Start up a FrameListener in a new thread.
   FrameListener frameListener(sdData, natNetMajor, natNetMinor);
   frameListener.start();
   
   // This infinite loop simulates a "worker" thread that reads the frame
   // buffer each time through, and exits when ctrl-c is pressed.
   printFrames(frameListener);
   //timeStats(frameListener);
   
   // Wait for threads to finish.
   frameListener.stop();
   commandListener.stop();
   frameListener.join();
   commandListener.join();
   
   // Epilogue
   close(sdData);
   close(sdCommand);
   return 0;
}