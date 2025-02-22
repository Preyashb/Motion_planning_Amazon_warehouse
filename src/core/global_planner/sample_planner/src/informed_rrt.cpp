/**
 * *********************************************************
 *
 * @file: informed_rrt.cpp
 * @brief: Contains the informed RRT* planner class
 * @author: Yang Haodong
 * @date: 2023-01-19
 * @version: 1.0
 *
 * Copyright (c) 2024, Yang Haodong.
 * All rights reserved.
 *
 * --------------------------------------------------------
 *
 * ********************************************************
 */
#include <random>

#include "informed_rrt.h"

namespace global_planner
{
/**
 * @brief Construct a informed new RRTStar object
 * @param costmap    the environment for path planning
 * @param sample_num andom sample points
 * @param max_dist   max distance between sample points
 * @param r          optimization radius
 */
InformedRRT::InformedRRT(costmap_2d::Costmap2D* costmap, int sample_num, double max_dist, double r)
  : RRTStar(costmap, sample_num, max_dist, r)
{
}

/**
 * @brief Informed RRT star implementation
 * @param start  start node
 * @param goal   goal node
 * @param expand containing the node been search during the process
 * @return true if path found, else false
 */
bool InformedRRT::plan(const Node& start, const Node& goal, std::vector<Node>& path, std::vector<Node>& expand)
{
  // clear vector
  path.clear();
  expand.clear();
  sample_list_.clear();

  // initialization
  c_best_ = std::numeric_limits<double>::max();
  c_min_ = helper::dist(start, goal);
  int best_parent = -1;

  // copy
  start_ = start, goal_ = goal;
  sample_list_.insert(std::make_pair(start.id(), start));
  expand.push_back(start);

  // main loop
  int iteration = 0;
  while (iteration < sample_num_)
  {
    // generate a random node in the map
    Node sample_node = _generateRandomNode();

    // regular the sample node
    Node new_node = _findNearestPoint(sample_list_, sample_node);
    if (new_node.id() == -1)
      continue;
    else
    {
      sample_list_.insert(std::make_pair(new_node.id(), new_node));
      expand.push_back(new_node);
    }

    // goal found
    auto dist_ = helper::dist(new_node, goal_);
    if (dist_ <= max_dist_ && !_isAnyObstacleInPath(new_node, goal_))
    {
      double cost = dist_ + new_node.g();
      if (cost < c_best_)
      {
        best_parent = new_node.id();
        c_best_ = cost;
      }
    }

    iteration++;
  }

  if (best_parent != -1)
  {
    Node goal_star(goal_.x(), goal_.y(), c_best_, 0, grid2Index(goal_.x(), goal_.y()), best_parent);
    sample_list_.insert(std::make_pair(goal_star.id(), goal_star));

    path = _convertClosedListToPath(sample_list_, start, goal);
    return true;
  }

  return false;
}

/**
 * @brief Generates a random node
 * @return generated node
 */
Node InformedRRT::_generateRandomNode()
{
  // ellipse sample
  if (c_best_ < std::numeric_limits<double>::max())
  {
    while (true)
    {
      // unit ball sample
      double x, y;
      std::random_device rd;
      std::mt19937 eng(rd());
      std::uniform_real_distribution<float> p(-1, 1);
      while (true)
      {
        x = p(eng);
        y = p(eng);
        if (x * x + y * y < 1)
          break;
      }
      // transform to ellipse
      Node temp = _transform(x, y);
      if (temp.id() < map_size_ - 1)
        return temp;
    }
  }
  else
    return RRTStar::_generateRandomNode();
}

/**
 * @brief Sample in ellipse
 * @param x random sampling x
 * @param y random sampling y
 * @return ellipse node
 */
Node InformedRRT::_transform(double x, double y)
{
  // center
  double center_x = (start_.x() + goal_.x()) / 2;
  double center_y = (start_.y() + goal_.y()) / 2;

  // rotation
  double theta = -helper::angle(start_, goal_);

  // ellipse
  double a = c_best_ / 2.0;
  double c = c_min_ / 2.0;
  double b = std::sqrt(a * a - c * c);

  // transform
  int tx = static_cast<int>(a * cos(theta) * x + b * sin(theta) * y + center_x);
  int ty = static_cast<int>(-a * sin(theta) * x + b * cos(theta) * y + center_y);
  int id = grid2Index(tx, ty);
  return Node(tx, ty, 0, 0, id, -1);
}
}  // namespace global_planner