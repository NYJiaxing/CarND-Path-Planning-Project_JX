#include <uWS/uWS.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "helpers.h"
#include "json.hpp"
#include "spline.h"
#include "math.h"

// for convenience
using nlohmann::json;
using std::string;
using std::vector;
using namespace std;
int main() {
  uWS::Hub h;

  // Load up map values for waypoint's x,y,s and d normalized normal vectors
  vector<double> map_waypoints_x;
  vector<double> map_waypoints_y;
  vector<double> map_waypoints_s;
  vector<double> map_waypoints_dx;
  vector<double> map_waypoints_dy;

  // Waypoint map to read from
  string map_file_ = "../data/highway_map.csv";
  // The max s value before wrapping around the track back to 0
  double max_s = 6945.554;

  std::ifstream in_map_(map_file_.c_str(), std::ifstream::in);

  string line;
  while (getline(in_map_, line)) {
    std::istringstream iss(line);
    double x;
    double y;
    float s;
    float d_x;
    float d_y;
    iss >> x;
    iss >> y;
    iss >> s;
    iss >> d_x;
    iss >> d_y;
    map_waypoints_x.push_back(x);
    map_waypoints_y.push_back(y);
    map_waypoints_s.push_back(s);
    map_waypoints_dx.push_back(d_x);
    map_waypoints_dy.push_back(d_y);
  }
  
  //define lane index and target velocity, in this project 50 MPH 
  // Display them on the terminal
  int lane_index = 1;
  double target_vel = 0;
  
  h.onMessage([&target_vel,&map_waypoints_x,&map_waypoints_y,&map_waypoints_s,
               &map_waypoints_dx,&map_waypoints_dy, &lane_index]
              (uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
               uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    if (length && length > 2 && data[0] == '4' && data[1] == '2') {

      auto s = hasData(data);

      if (s != "") {
        auto j = json::parse(s);
        
        string event = j[0].get<string>();
        
        if (event == "telemetry") {
          // j[1] is the data JSON object
          
          // Main car's localization Data
          double car_x = j[1]["x"];
          double car_y = j[1]["y"];
          double car_s = j[1]["s"];
          double car_d = j[1]["d"];
          double car_yaw = j[1]["yaw"];
          double car_speed = j[1]["speed"];

          // Previous path data given to the Planner
          auto previous_path_x = j[1]["previous_path_x"];
          auto previous_path_y = j[1]["previous_path_y"];
          // Previous path's end s and d values 
          double end_path_s = j[1]["end_path_s"];
          double end_path_d = j[1]["end_path_d"];

          // Sensor Fusion Data, a list of all other cars on the same side 
          //   of the road.
          auto sensor_fusion = j[1]["sensor_fusion"];

          json msgJson;

          /**
           * TODO: define a path made up of (x,y) points that the car will visit
           *   sequentially every .02 seconds
           */
          
          //check if there is previous path generated
          double prev_path =  previous_path_x.size();
         
          if (prev_path > 0)
          {
            car_s = end_path_s;
          }

          bool too_close = false;
          bool left_lane = true;
          bool right_lane = true;
          //find target velocity
          for (int i = 0; i<sensor_fusion.size();i++)
          {
            float d = sensor_fusion[i][6];
            double left_lane_ceriteria = 2 + 4*(lane_index-1);
            double right_lane_ceriteria = 2 + 4*(lane_index+1);
            double car_s_1 = sensor_fusion[i][5];
            double safety_distance1 = car_s_1- car_s;
            
            if (d < (2 + 4* lane_index + 2) && d > (2 + 4 * lane_index -2))
            {
              double vx = sensor_fusion[i][3];
              double vy = sensor_fusion[i][4];
              double car_s_ = sensor_fusion[i][5];
              double car_vel_ = sqrt(vx*vx + vy*vy);
              
              //predict vehicle position
              car_s_ = car_s_ + prev_path*0.02*car_vel_;
              double safety_distance = car_s_ - car_s;
              //check if the safety distance enough, if not, check the lane next to the current lane 
              if ((safety_distance < 30) && (safety_distance > 0))
              {
                too_close = true;
              }
            }
            //check if the left lane available
            if (abs(safety_distance1) < 30 && d < (left_lane_ceriteria + 2) && d > (left_lane_ceriteria - 2))
            {
              left_lane = false;
            }
            
            //check if the right lane available
            if (abs(safety_distance1) < 30 && d < (right_lane_ceriteria + 2) && d > (right_lane_ceriteria - 2))
            {
              right_lane = false;
            }
              
            // check the neighbor lane occupied by vehicles or not

            if (too_close == true)
            {  
              if (left_lane == true && lane_index >= 1)
              {
                lane_index = lane_index - 1;
              }
              else if (right_lane == true && lane_index <= 1)
              {
                lane_index = lane_index + 1;
              }

            }
          }

          if(too_close)
          {
            target_vel -= 0.224; //decrease 5m/s
          }
          else if(target_vel < 49.5)
          {
            target_vel += 0.224; //inclrease 5m/s
          }


          vector<double> ptsx;
          vector<double> ptsy;
          
          double ref_x = car_x;
          double ref_y = car_y;
          double ref_yaw = deg2rad(car_yaw);

          // If previous list is almost empty
          if(prev_path < 2)
          {
            double prev_car_x = car_x - cos(car_yaw);
            double prev_car_y = car_y - sin(car_yaw);
            
            ptsx.push_back(prev_car_x);
            ptsx.push_back(car_x);
            
            ptsy.push_back(prev_car_y);
            ptsy.push_back(car_y);
          }
          // If the previous path exists
          else
          {
            // update the new ref start with the previous end point
            ref_x = previous_path_x[prev_path-1];
            ref_y = previous_path_y[prev_path-1];

            double ref_x_prev = previous_path_x[prev_path-2];
            double ref_y_prev = previous_path_y[prev_path-2];
            ref_yaw = atan2(ref_y-ref_y_prev,ref_x-ref_x_prev);
            
            // Use two points that make the path tangent to the previous path's end point
            ptsx.push_back(ref_x_prev);
            ptsx.push_back(ref_x);
            
            ptsy.push_back(ref_y_prev);
            ptsy.push_back(ref_y);
          }

          //In Frenet add evenly 30m spaced points ahead of the starting reference
          vector<double> next_wp0 = getXY(car_s+30, (2+4*lane_index), map_waypoints_s, map_waypoints_x, map_waypoints_y);
          vector<double> next_wp1 = getXY(car_s+60, (2+4*lane_index), map_waypoints_s, map_waypoints_x, map_waypoints_y);
          vector<double> next_wp2 = getXY(car_s+90, (2+4*lane_index), map_waypoints_s, map_waypoints_x, map_waypoints_y);

          ptsx.push_back(next_wp0[0]);
          ptsx.push_back(next_wp1[0]);
          ptsx.push_back(next_wp2[0]);

          ptsy.push_back(next_wp0[1]);
          ptsy.push_back(next_wp1[1]);
          ptsy.push_back(next_wp2[1]);

          if (ptsx.size() > 2 )
          {
            for (int i = 0; i < ptsx.size(); ++i) {

              double shift_x = ptsx[i]-ref_x;
              double shift_y = ptsy[i]-ref_y;

              ptsx[i] = (shift_x * cos(0-ref_yaw)-shift_y*sin(0-ref_yaw));
              ptsy[i] = (shift_x * sin(0-ref_yaw)+shift_y*cos(0-ref_yaw));

            }
          }
          tk::spline s;

          s.set_points(ptsx, ptsy);

          double target_x = 30.0;
          double target_y = s(target_x);
          double target_dist = sqrt((target_x)*(target_x)+(target_y)*(target_y));

          double x_add_on = 0;

          vector<double> next_x_vals;
          vector<double> next_y_vals;

          for(int i = 0; i < prev_path; i++)
          {
            next_x_vals.push_back(previous_path_x[i]);
            next_y_vals.push_back(previous_path_y[i]);
          }


          for(int i = 0; i < 50 - prev_path; i++) {
            double N = (target_dist/(.02*target_vel/2.24));
            double x_point = x_add_on+(target_x)/N;
            double y_point = s(x_point);
            
            x_add_on = x_point;
            
            double x_ref = x_point;
            double y_ref = y_point;
            
            x_point = (x_ref*cos(ref_yaw)-y_ref*sin(ref_yaw));
            y_point = (x_ref*sin(ref_yaw)+y_ref*cos(ref_yaw));
            
            x_point += ref_x;
            y_point += ref_y;
            
            next_x_vals.push_back(x_point);
            next_y_vals.push_back(y_point);
          }
          //TODO END
          msgJson["next_x"] = next_x_vals;
          msgJson["next_y"] = next_y_vals;

          auto msg = "42[\"control\","+ msgJson.dump()+"]";

          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        }  // end "telemetry" if
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }  // end websocket if
  }); // end h.onMessage

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  
  h.run();
}
