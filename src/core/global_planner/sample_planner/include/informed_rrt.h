/**
 * *********************************************************
 *
 * @file: informed_rrt.h
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
#ifndef INFORMED_RRT_H
#define INFORMED_RRT_H

#include "rrt_star.h"

namespace global_planner
{
/**
 * @brief Class for objects that plan using the informed RRT* algorithm
 */
class InformedRRT : public RRTStar
{
public:
  /**
   * @brief Construct a informed new RRTStar object
   * @param costmap    the environment for path planning
   * @param sample_num andom sample points
   * @param max_dist   max distance between sample points
   * @param r          optimization radius
   */
  InformedRRT(costmap_2d::Costmap2D* costmap, int sample_num, double max_dist, double r);

  /**
   * @brief Informed RRT star implementation
   * @param start  start node
   * @param goal   goal node
   * @param expand containing the node been search during the process
   * @return true if path found, else false
   */
  bool plan(const Node& start, const Node& goal, std::vector<Node>& path, std::vector<Node>& expand);

protected:
  /**
   * @brief Sample in ellipse
   * @param x random sampling x
   * @param y random sampling y
   * @return ellipse node
   */
  Node _transform(double x, double y);

  /**
   * @brief Generates a random node
   * @return generated node
   */
  Node _generateRandomNode();

protected:
  double c_best_;  // best planning cost
  double c_min_;   // distance between start and goal
};
}  // namespace global_planner
#endif  // INFORMED_RRT_H
