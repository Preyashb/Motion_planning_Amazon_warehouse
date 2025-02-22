/**
 * *********************************************************
 *
 * @file: rrt_star.h
 * @brief: Contains the Rapidly-Exploring Random Tree Star(RRT*) planner class
 * @author: Yang Haodong
 * @date: 2022-10-29
 * @version: 1.0
 *
 * Copyright (c) 2024, Yang Haodong.
 * All rights reserved.
 *
 * --------------------------------------------------------
 *
 * ********************************************************
 */
#ifndef RRT_STAR_H
#define RRT_STAR_H

#include "rrt.h"

namespace global_planner
{
/**
 * @brief Class for objects that plan using the RRT* algorithm
 */
class RRTStar : public RRT
{
public:
  /**
   * @brief Construct a new RRTStar object
   * @param costmap    the environment for path planning
   * @param sample_num andom sample points
   * @param max_dist   max distance between sample points
   * @param r          optimization radius
   */
  RRTStar(costmap_2d::Costmap2D* costmap, int sample_num, double max_dist, double r);

  /**
   * @brief RRT star implementation
   * @param start  start node
   * @param goal   goal node
   * @param expand containing the node been search during the process
   * @return  true if path found, else false
   */
  bool plan(const Node& start, const Node& goal, std::vector<Node>& path, std::vector<Node>& expand);

protected:
  /**
   * @brief Regular the new node by the nearest node in the sample list
   * @param list sample list
   * @param node sample node
   * @return nearest node
   */
  Node _findNearestPoint(std::unordered_map<int, Node>& list, Node& node);

protected:
  double r_;  // optimization radius
};
}  // namespace global_planner
#endif  // RRT_STAR_H
