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

// for convenience
using nlohmann::json;
using std::cout;
using std::endl;
using std::string;
using std::vector;

int lane = 1;          // started in lane 1
double ref_vel = 0; // have a reference velocity to target
const double lane_width		= 4.0;		// width of a lane					(m)
const double safety_margin	= 20.0;		// distance to keep from other cars	(m)
const double max_safe_speed	= 49.5;		// max reference speed in the limit	(mph)


int main()
{
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

  std::ifstream in_map_(map_file_.c_str(), std::ifstream::in); // ifstream: input stream to operate files

  string line;
  while (getline(in_map_, line)) // char buffer[256]; in_map_.getline(buffer, 256);
  {
    std::istringstream iss(line); //
    // cout << "iss" << endl;
    // cout << iss.str() << endl;
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
    // cout << "***************" << endl;
    // cout << x << y << s << d_x << d_y << endl;
    map_waypoints_x.push_back(x);
    map_waypoints_y.push_back(y);
    map_waypoints_s.push_back(s);
    map_waypoints_dx.push_back(d_x);
    map_waypoints_dy.push_back(d_y);
  }

  h.onMessage([&map_waypoints_x, &map_waypoints_y, &map_waypoints_s,
               &map_waypoints_dx, &map_waypoints_dy](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                                                     uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event

    if (length && length > 2 && data[0] == '4' && data[1] == '2')
    {

      auto s = hasData(data);

      if (s != "")
      {
        auto j = json::parse(s);

        string event = j[0].get<string>();

        if (event == "telemetry")
        {
          // j[1] is the data JSON object

          // Main car's localization Data
          double car_x = j[1]["x"];
          cout << "car_x" << car_x << endl;
          double car_y = j[1]["y"];
          cout << "car_y" << car_y << endl;
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
          vector<vector<double> > sensor_fusion = j[1]["sensor_fusion"];

          json msgJson;

          /**
           * TODO: define a path made up of (x,y) points that the car will visit
           *   sequentially every .02 seconds
           */
          // Define the actual (x,y) points we will use for the planner
          // vector<double> next_x_vals;
          // vector<double> next_y_vals;
          
          int prev_size = previous_path_x.size();

          if (prev_size > 0)
          {
            car_s = end_path_s; // car s represent the end point in the last path planning module iteration // car_s represent the future of our host car
          }
          bool too_close = false;
          bool is_too_close			 = false;
			    bool prepare_for_lane_change = false;
			    bool ready_for_lane_change   = false;
			    bool is_left_lane_free		 = true;
			    bool is_right_lane_free		 = true;

          // find ref_v to use
          for (int i = 0; i < sensor_fusion.size(); i++)
          {
            // car is in my lane
            float d = sensor_fusion[i][6];
            if (d < (2 + lane_width * lane + 2) && d > (2 + lane_width * lane - 2))
            {
              cout << "find one car in front of us" << endl;
              double vx = sensor_fusion[i][3];
              double vy = sensor_fusion[i][4];
              double check_speed = sqrt(vx * vx + vy * vy); //the vehicle of front car in our lane
              double check_car_s = sensor_fusion[i][5]; // the target vehicle 

              // using previous points can project s value out in time 
              check_car_s += ((double)prev_size * 0.02 * check_speed); // 前车按照它的速度，在主车当前点与当前路径终点时间间距内行驶过的距离
              //check s values greater than mine and s gap
              bool is_in_front_of_us = check_car_s > car_s;
              bool is_closer_than_safety_margin = (check_car_s - car_s) < safety_margin;
              // if((check_car_s > car_s) &&(check_car_s - car_s) < 30)
              if(is_in_front_of_us && is_closer_than_safety_margin)
              {
                // Do some logic here, lower reference velocity so we don't crash into the car infron of us, could also flag to try to change lanes
                // ref_vel = 29.5; // mph
                too_close = true;
                prepare_for_lane_change = true;
                // if(lane > 0)
                // {
                //   lane = 0;
                // }
              } 
            }
          }

          if(prepare_for_lane_change)
          {
            int num_vehicles_left = 0;
            int num_vehicles_right = 0;
            for (int i = 0; i < sensor_fusion.size(); i++)
            {
              // car is in my lane
              float s = sensor_fusion[i][5];
              float d = sensor_fusion[i][6];
              double vx = sensor_fusion[i][3];
              double vy = sensor_fusion[i][4];
              double check_speed = sqrt(vx * vx + vy * vy); //the vehicle of front car in our lane
              if (d < (2 + 4 * (lane-1) + 2) && d > (2 + 4 * (lane-1) - 2))
              {
                num_vehicles_left++;
                s += ((double)prev_size * 0.02 * check_speed);
                bool too_closed_to_change = (s > (car_s - safety_margin/2)) && (s < (car_s + safety_margin/2));
                if(too_closed_to_change)
                {
                  is_left_lane_free = false;
                }
              }
              else if (d < (2 + 4 * (lane+1) + 2) && d > (2 + 4 * (lane+1) - 2))
              {
                num_vehicles_right++;
                s += ((double)prev_size * 0.02 * check_speed);
                bool too_closed_to_change = (s > (car_s - safety_margin/2)) && (s < (car_s + safety_margin/2));
                if(too_closed_to_change)
                {
                  is_right_lane_free = false;
                }
              }
              if(is_left_lane_free || is_right_lane_free)
              {
                ready_for_lane_change = true;
              }
            }
            cout << "LEFT " << num_vehicles_left << "RIGHT " << num_vehicles_right << endl;
          }

          			// Actually perform lane change
			    if(ready_for_lane_change && is_left_lane_free && lane > 0)
				    lane -= 1;
			    else if (ready_for_lane_change && is_right_lane_free && lane < 2)
				    lane += 1;
          if(too_close)
          {
            ref_vel -= 0.224; // around fine meters per second squared
          }
          else if(ref_vel < 49.5)
          {
            ref_vel += 0.224;
          }
          //////////////////////////////////////////////////////////////////////////////////////////////////
          //////////////////////////////////////////////////////////////////////////////////////////////////
          // create a list of widely spaced (x,y) waypoints, evenly spaced at 30m,
          // later we will interoplate with a spline and fill it in with more points that control speed
          vector<double> ptsx;
          vector<double> ptsy;

          // reference x,y,yaw_state
          // either we will reference the starting point as where the car is or at the previous end point
          double ref_x = car_x;
          double ref_y = car_y;
          double ref_yaw = deg2rad(car_yaw);
          
          cout << "car yaw" << car_yaw << endl;
          // cout << cos(60) << endl;
          // cout << "cos(60*3/1315829/180" << cos(deg2rad(60)) << endl;

          // if previous size is almost empty, use the car as starting reference
          if (prev_size < 2)
          {
            // Use two points that make the path tangent to the car
            // double prev_car_x = car_x - cos(car_yaw);
            // double prev_car_y = car_y - sin(car_yaw);
            double prev_car_x = car_x - cos(ref_yaw);
            double prev_car_y = car_y - sin(ref_yaw);

            ptsx.push_back(prev_car_x);
            ptsx.push_back(car_x);

            ptsy.push_back(prev_car_y);
            ptsy.push_back(car_y);
          }
          // using the previous path's end point as starting reference
          else
          {
            // Redefine reference state as previous path end point
            ref_x = previous_path_x[prev_size - 1];
            ref_y = previous_path_y[prev_size - 1];

            double ref_x_prev = previous_path_x[prev_size - 2];
            double ref_y_prev = previous_path_y[prev_size - 2];

            // Use two points that make the path tangent to the previous path's end point
            ptsx.push_back(ref_x_prev);
            ptsx.push_back(ref_x);

            ptsy.push_back(ref_y_prev);
            ptsy.push_back(ref_y);
          }

          vector<double> next_wp0 = getXY(car_s + 30, (2 + 4 * lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
          vector<double> next_wp1 = getXY(car_s + 60, (2 + 4 * lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
          vector<double> next_wp2 = getXY(car_s + 90, (2 + 4 * lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);

          ptsx.push_back(next_wp0[0]);
          ptsx.push_back(next_wp1[0]);
          ptsx.push_back(next_wp2[0]);

          ptsy.push_back(next_wp0[1]);
          ptsy.push_back(next_wp1[1]);
          ptsy.push_back(next_wp2[1]);

          for (int i = 0; i < ptsx.size(); i++)
          {
            // shift car reference angle to 0 degree
            // doint a transformation to this local car's coordinates
            double shift_x = ptsx[i] - ref_x;
            double shift_y = ptsy[i] - ref_y;

            ptsx[i] = (shift_x * cos(0 - ref_yaw) - shift_y * sin(0 - ref_yaw));
            ptsy[i] = (shift_x * sin(0 - ref_yaw) + shift_y * cos(0 - ref_yaw));
          }

          // create a spline
          tk::spline s;
          // set(x,y) points to the spline
          s.set_points(ptsx, ptsy); // in car's local coordinate system

          // Define the actual (x,y) points we will use for the planner
          vector<double> next_x_vals;
          vector<double> next_y_vals;

          // start with all of the previous path points from last time
          for (int i = 0; i < previous_path_x.size(); i++)
          {
            next_x_vals.push_back(previous_path_x[i]);
            next_y_vals.push_back(previous_path_y[i]);
          }

          // Calculate how to break up apline points so that we travel at our desired reference velocity
          double target_x = 30;
          double target_y = s(target_x);
          double target_dist = sqrt((target_x) * (target_x) + (target_y) * (target_y));

          double x_add_on = 0;

          // Fill up the rest of our path planner after filling it with previous points, here we will always output 50 points
          for (int i = 1; i <= 50 - previous_path_x.size(); i++)
          {
            double N = (target_dist / (0.02 * ref_vel / 2.24)); //miles per hour and meters per hour transimation
            double x_point = x_add_on + (target_x) / N;
            double y_point = s(x_point);

            x_add_on = x_point;

            double x_ref = x_point;
            double y_ref = y_point;
            // rotate back to normal after rotating it eralier
            x_point = (x_ref * cos(ref_yaw) - y_ref * sin(ref_yaw));
            y_point = (x_ref * sin(ref_yaw) + y_ref * cos(ref_yaw));

            x_point += ref_x;
            y_point += ref_y;

            next_x_vals.push_back(x_point);
            next_y_vals.push_back(y_point);
          }

          msgJson["next_x"] = next_x_vals;
          msgJson["next_y"] = next_y_vals;

          auto msg = "42[\"control\"," + msgJson.dump() + "]";

          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        } // end "telemetry" if
      }
      else
      {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    } // end websocket if
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
  if (h.listen(port))
  {
    std::cout << "Listening to port " << port << std::endl;
  }
  else
  {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }

  h.run();
}


          // double dist_inc = 0.5;
          // for (int i = 0; i < 50; i++)
          // {
          //   double next_s = car_s + (i + 1)* dist_inc;
          //   double next_d = 6;

          //   vector<double> xy = getXY(next_s, next_d, map_waypoints_s, map_waypoints_x, map_waypoints_y);

          //   next_x_vals.push_back(xy[0]);
          //   next_y_vals.push_back(xy[1]);
          // }

          // go straight*********************
          // double dist_inc = 0.5;
          // for(int i = 0; i < 50; i++)
          // {
          //   next_x_vals.push_back(car_x + dist_inc * i * cos(deg2rad(car_yaw)));
          //   next_y_vals.push_back(car_y + dist_inc * i * sin(deg2rad(car_yaw)));
          // }