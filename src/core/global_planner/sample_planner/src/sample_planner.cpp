/**
 * *********************************************************
 *
 * @file: sample_planner.cpp
 * @brief: Contains the sample planner ROS wrapper class
 * @author: Yang Haodong
 * @date: 2022-10-26
 * @version: 1.0
 *
 * Copyright (c) 2024, Yang Haodong.
 * All rights reserved.
 *
 * --------------------------------------------------------
 *
 * ********************************************************
 */
#include <pluginlib/class_list_macros.h>

#include "sample_planner.h"
#include "rrt.h"
#include "rrt_star.h"
#include "rrt_connect.h"
#include "informed_rrt.h"
#include "quick_informed_rrt.h"

PLUGINLIB_EXPORT_CLASS(sample_planner::SamplePlanner, nav_core::BaseGlobalPlanner)

namespace sample_planner
{
/**
 * @brief Constructor a new Sample Planner object
 */
SamplePlanner::SamplePlanner() : initialized_(false), g_planner_(nullptr)
{
}

/**
 * @brief Construct a new Sample Planner object
 * @param name        planner name
 * @param costmap_ros the cost map to use for assigning costs to trajectories
 */
SamplePlanner::SamplePlanner(std::string name, costmap_2d::Costmap2DROS* costmap_ros) : SamplePlanner()
{
  initialize(name, costmap_ros);
}

/**
 * @brief Planner initialization
 * @param name       planner name
 * @param costmapRos costmap ROS wrapper
 */
void SamplePlanner::initialize(std::string name, costmap_2d::Costmap2DROS* costmapRos)
{
  if (!initialized_)
  {
    costmap_ros_ = costmapRos;
    frame_id_ = costmap_ros_->getGlobalFrameID();

    // initialize ROS node
    ros::NodeHandle private_nh("~/" + name);
    private_nh.param("obstacle_factor", factor_, 0.5);       // obstacle factor
    private_nh.param("default_tolerance", tolerance_, 0.0);  // error tolerance
    private_nh.param("outline_map", is_outline_, false);     // whether outline the map or not
    private_nh.param("expand_zone", is_expand_, false);      // whether publish expand zone or not

    // planner parameters
    int sample_points;
    double sample_max_d, optimization_r;
    private_nh.param("sample_points", sample_points, 500);     // random sample points
    private_nh.param("sample_max_d", sample_max_d, 5.0);       // max distance between sample points
    private_nh.param("optimization_r", optimization_r, 10.0);  // optimization radius

    // planner name
    private_nh.param("planner_name", planner_name_, std::string("rrt"));

    auto costmap = costmap_ros_->getCostmap();

    if (planner_name_ == "rrt")
      g_planner_ = std::make_shared<global_planner::RRT>(costmap, sample_points, sample_max_d);
    else if (planner_name_ == "rrt_star")
      g_planner_ = std::make_shared<global_planner::RRTStar>(costmap, sample_points, sample_max_d, optimization_r);
    else if (planner_name_ == "rrt_connect")
      g_planner_ = std::make_shared<global_planner::RRTConnect>(costmap, sample_points, sample_max_d);
    else if (planner_name_ == "informed_rrt")
      g_planner_ = std::make_shared<global_planner::InformedRRT>(costmap, sample_points, sample_max_d, optimization_r);
    else if (planner_name_ == "quick_informed_rrt")
    {
      int rewire_threads_n;
      double prior_set_r, step_ext_d, t_freedom;
      private_nh.param("prior_sample_set_r", prior_set_r, 10.0);    // radius of priority circles set
      private_nh.param("rewire_threads_num", rewire_threads_n, 2);  // threads number of rewire process
      private_nh.param("step_extend_d", step_ext_d, 5.0);           // threads number of rewire process
      private_nh.param("t_distr_freedom", t_freedom, 1.0);          // freedom of t distribution
      g_planner_ = std::make_shared<global_planner::QuickInformedRRT>(
          costmap, sample_points, sample_max_d, optimization_r, prior_set_r, rewire_threads_n, step_ext_d, t_freedom);
    }
    else
      ROS_ERROR("Unknown planner name: %s", planner_name_.c_str());

    g_planner_->setFactor(factor_);

    ROS_INFO("Using global sample planner: %s", planner_name_.c_str());

    // register planning publisher
    plan_pub_ = private_nh.advertise<nav_msgs::Path>("plan", 1);

    // register explorer visualization publisher
    expand_pub_ = private_nh.advertise<visualization_msgs::Marker>("tree", 1);

    // register planning service
    make_plan_srv_ = private_nh.advertiseService("make_plan", &SamplePlanner::makePlanService, this);

    initialized_ = true;
  }
  else
    ROS_WARN("This planner has already been initialized, you can't call it twice, doing nothing");
}

/**
 * @brief Plan a path given start and goal in world map
 * @param start start in world map
 * @param goal  goal in world map
 * @param plan  plan
 * @return true if find a path successfully, else false
 */
bool SamplePlanner::makePlan(const geometry_msgs::PoseStamped& start, const geometry_msgs::PoseStamped& goal,
                             std::vector<geometry_msgs::PoseStamped>& plan)
{
  // start thread mutex
  std::unique_lock<costmap_2d::Costmap2D::mutex_t> lock(*g_planner_->getCostMap()->getMutex());

  if (!initialized_)
  {
    ROS_ERROR("This planner has not been initialized yet, but it is being used, please call initialize() before use");
    return false;
  }

  // clear existing plan
  plan.clear();

  // judege whether start and goal node in costmap frame or not
  if (start.header.frame_id != frame_id_ || goal.header.frame_id != frame_id_)
  {
    ROS_ERROR("The start or goal pose passed to this planner must be in %s frame. It is instead in %s and %s frame.",
              frame_id_.c_str(), start.header.frame_id.c_str(), goal.header.frame_id.c_str());
    return false;
  }

  // get goal and start node coordinate tranform from world to costmap
  unsigned int g_start_x, g_start_y, g_goal_x, g_goal_y;
  double wx, wy;

  wx = start.pose.position.x, wy = start.pose.position.y;
  if (!g_planner_->world2Map(wx, wy, g_start_x, g_start_y))
  {
    ROS_WARN(
        "The robot's start position is off the global costmap. Planning will always fail, are you sure the robot has "
        "been properly localized?");
    return false;
  }

  wx = goal.pose.position.x, wy = goal.pose.position.y;
  if (!g_planner_->world2Map(wx, wy, g_goal_x, g_goal_y))
  {
    ROS_WARN_THROTTLE(1.0,
                      "The goal sent to the global planner is off the global costmap. Planning will always fail to "
                      "this goal.");
    return false;
  }

  // outline the map
  if (is_outline_)
    g_planner_->outlineMap();

  // calculate path
  std::vector<Node> path;
  std::vector<Node> expand;
  bool path_found = false;

  Node start_node(g_start_x, g_start_y, 0, 0, g_planner_->grid2Index(g_start_x, g_start_y), -1);
  Node goal_node(g_goal_x, g_goal_y, 0, 0, g_planner_->grid2Index(g_goal_x, g_goal_y), -1);

  // planning
  path_found = g_planner_->plan(start_node, goal_node, path, expand);

  // convert path to ros plan
  if (path_found)
  {
    if (_getPlanFromPath(path, plan))
    {
      geometry_msgs::PoseStamped goalCopy = goal;
      goalCopy.header.stamp = ros::Time::now();
      plan.push_back(goalCopy);
      history_plan_ = plan;
    }
    else
      ROS_ERROR("Failed to get a plan from path when a legal path was found. This shouldn't happen.");
  }
  else if (history_plan_.size() > 0)
  {
    plan = history_plan_;
    ROS_WARN("Using history path.");
  }
  else
    ROS_ERROR("Failed to get a path.");

  // publish expand zone
  if (is_expand_)
    _publishExpand(expand);

  // publish visulization plan
  publishPlan(plan);

  return !plan.empty();
}

/**
 * @brief Publish planning path
 * @param path planning path
 */
void SamplePlanner::publishPlan(const std::vector<geometry_msgs::PoseStamped>& plan)
{
  if (!initialized_)
  {
    ROS_ERROR("This planner has not been initialized yet, but it is being used, please call initialize() before use");
    return;
  }

  // creat visulized path plan
  nav_msgs::Path gui_plan;
  gui_plan.poses.resize(plan.size());
  gui_plan.header.frame_id = frame_id_;
  gui_plan.header.stamp = ros::Time::now();
  for (unsigned int i = 0; i < plan.size(); i++)
    gui_plan.poses[i] = plan[i];

  // publish plan to rviz
  plan_pub_.publish(gui_plan);
}

/**
 * @brief Regeister planning service
 * @param req  request from client
 * @param resp response from server
 */
bool SamplePlanner::makePlanService(nav_msgs::GetPlan::Request& req, nav_msgs::GetPlan::Response& resp)
{
  makePlan(req.start, req.goal, resp.plan.poses);
  resp.plan.header.stamp = ros::Time::now();
  resp.plan.header.frame_id = frame_id_;
  return true;
}

/**
 * @brief publish expand zone
 * @param expand set of expand nodes
 */
void SamplePlanner::_publishExpand(std::vector<Node>& expand)
{
  ROS_DEBUG("Expand Zone Size:%ld", expand.size());

  // Initializes a Marker msg for a LINE_LIST
  visualization_msgs::Marker tree_msg;
  tree_msg.header.frame_id = "map";
  tree_msg.id = 0;
  tree_msg.ns = "tree";
  tree_msg.type = visualization_msgs::Marker::LINE_LIST;
  tree_msg.action = visualization_msgs::Marker::ADD;
  tree_msg.pose.orientation.w = 1.0;
  tree_msg.scale.x = 0.05;

  // Publish all edges
  for (auto node : expand)
    if (node.pid() != -1)
      _pubLine(&tree_msg, &expand_pub_, node.id(), node.pid());
}

/**
 * @brief Calculate plan from planning path
 * @param path path generated by global planner
 * @param plan plan transfromed from path, i.e. [start, ..., goal]
 * @return bool true if successful, else false
 */
bool SamplePlanner::_getPlanFromPath(std::vector<Node> path, std::vector<geometry_msgs::PoseStamped>& plan)
{
  if (!initialized_)
  {
    ROS_ERROR("This planner has not been initialized yet, but it is being used, please call initialize() before use");
    return false;
  }

  ros::Time planTime = ros::Time::now();
  plan.clear();

  for (int i = path.size() - 1; i >= 0; i--)
  {
    double wx, wy;
    g_planner_->map2World((double)path[i].x(), (double)path[i].y(), wx, wy);

    // coding as message type
    geometry_msgs::PoseStamped pose;
    pose.header.stamp = ros::Time::now();
    pose.header.frame_id = frame_id_;
    pose.pose.position.x = wx;
    pose.pose.position.y = wy;
    pose.pose.position.z = 0.0;
    pose.pose.orientation.x = 0.0;
    pose.pose.orientation.y = 0.0;
    pose.pose.orientation.z = 0.0;
    pose.pose.orientation.w = 1.0;
    plan.push_back(pose);
  }

  return !plan.empty();
}

/**
 *  @brief Publishes a Marker msg with two points in Rviz
 *  @param line_msg Pointer to existing marker object.
 *  @param line_pub Pointer to existing marker Publisher.
 *  @param id       first marker id
 *  @param pid      second marker id
 */
void SamplePlanner::_pubLine(visualization_msgs::Marker* line_msg, ros::Publisher* line_pub, int id, int pid)
{
  // Update line_msg header
  line_msg->header.stamp = ros::Time::now();

  // Build msg
  geometry_msgs::Point p1, p2;
  std_msgs::ColorRGBA c1, c2;
  int p1x, p1y, p2x, p2y;

  g_planner_->index2Grid(id, p1x, p1y);
  g_planner_->map2World(p1x, p1y, p1.x, p1.y);
  p1.z = 1.0;

  g_planner_->index2Grid(pid, p2x, p2y);
  g_planner_->map2World(p2x, p2y, p2.x, p2.y);
  p2.z = 1.0;

  c1.r = 0.43;
  c1.g = 0.54;
  c1.b = 0.24;
  c1.a = 0.5;

  c2.r = 0.43;
  c2.g = 0.54;
  c2.b = 0.24;
  c2.a = 0.5;

  line_msg->points.push_back(p1);
  line_msg->points.push_back(p2);
  line_msg->colors.push_back(c1);
  line_msg->colors.push_back(c2);

  // Publish line_msg
  line_pub->publish(*line_msg);
}
}  // namespace sample_planner