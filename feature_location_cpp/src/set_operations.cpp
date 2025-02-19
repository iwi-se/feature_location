#include "set_operations.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <utility>
#include <memory>
#include <stack>
#include "node_types.hpp"

// Assuming Node and Tree classes are defined elsewhere with necessary methods

// Placeholder for calculate_depth_proximity function
double calculate_depth_proximity(const std::shared_ptr<Node> &node1, const std::shared_ptr<Node> &node2)
{
    // Implement depth proximity calculation
    return 0.0;
}

// Placeholder for calculate_environment_similarity function
double calculate_environment_similarity(const std::shared_ptr<Node> &node1, const std::shared_ptr<Node> &node2)
{
    // Implement environment similarity calculation
    return 0.0;
}

void sort_by_decision_ratio(std::vector<std::pair<std::shared_ptr<Node>, std::shared_ptr<Node>>> &pairs, const Configuration &config)
{
    std::vector<std::tuple<std::pair<std::shared_ptr<Node>, std::shared_ptr<Node>>, double>> pairs_with_ratio;

    for (const auto &pair : pairs)
    {
        // double depth_proximity = calculate_depth_proximity(pair.first, pair.second);
        // double environment_similarity = calculate_environment_similarity(pair.first, pair.second);
        int size = pair.first->get_connected_leaf_weight(); // Assuming get_subtree_size() is a method in Node

        // double decision_ratio = 0.2 * depth_proximity + 0.3 * environment_similarity + 0.5 * std::min((size / 100.0), 1.0);

        double decision_ratio = size;
        pairs_with_ratio.push_back(std::make_tuple(pair, decision_ratio));
    }

    std::sort(pairs_with_ratio.begin(), pairs_with_ratio.end(),
              [](const std::tuple<std::pair<std::shared_ptr<Node>, std::shared_ptr<Node>>, double> &a, const std::tuple<std::pair<std::shared_ptr<Node>, std::shared_ptr<Node>>, double> &b)
              {
                  return std::get<1>(a) > std::get<1>(b);
              });

    pairs.clear();
    for (const auto &tuple : pairs_with_ratio)
    {
        if (std::get<1>(tuple) > config.options.minimum_trace_weight)
        {
            pairs.push_back(std::get<0>(tuple));
        }
    }
}

bool non_overlapping(const std::pair<std::shared_ptr<Node>, std::shared_ptr<Node>> &pair,
                     const std::vector<std::pair<std::shared_ptr<Node>, std::shared_ptr<Node>>> &result)
{
    for (const auto &existing_pair : result)
    {
        Node::RelativePosition rel_pos_1 = pair.first->get_relative_position(existing_pair.first);
        Node::RelativePosition rel_pos_2 = pair.second->get_relative_position(existing_pair.second);
        if (rel_pos_1 == Node::RelativePosition::overlapping || rel_pos_1 != rel_pos_2)
        {
            return false;
        }
    }
    return true;
}

void remove_overlapping_pairs(std::vector<std::pair<std::shared_ptr<Node>, std::shared_ptr<Node>>> &pairs)
{
    std::vector<std::pair<std::shared_ptr<Node>, std::shared_ptr<Node>>> result;
    for (const auto &pair : pairs)
    {
        if (non_overlapping(pair, result))
        {
            result.push_back(pair);
        }
    }
    pairs = result;
}

std::pair<std::vector<SourcePosition>, std::vector<SourcePosition>> intersection(
    const std::shared_ptr<Node> &file1,
    const std::shared_ptr<Node> &file2,
    const Configuration &config)
{
    std::vector<std::pair<std::shared_ptr<Node>, std::shared_ptr<Node>>> pairs;

    std::stack<std::shared_ptr<Node>> stack1{{file1}};
    while (!stack1.empty())
    {
        auto node1 = stack1.top();
        stack1.pop();

        std::stack<std::shared_ptr<Node>> stack2{{file2}};
        bool match_found = false;
        while (!stack2.empty())
        {
            auto node2 = stack2.top();
            stack2.pop();

            if (node1->get_subtree_hash() == node2->get_subtree_hash() && 
                is_included_node_type(node2, config))
            {
                pairs.push_back(std::make_pair(node1, node2));
                match_found = true;
            }
            else if (node1->get_connected_leaf_weight() < node2->get_connected_leaf_weight())
            {
                for (const auto &child : node2->get_children())
                {
                    if (child->get_connected_leaf_weight() >= config.options.minimum_trace_weight)
                    {
                        stack2.push(child);
                    }
                }
            }
        }
        if (!match_found)
        {
            for (const auto &child : node1->get_children())
            {
                stack1.push(child);
            }
        }
    }

    sort_by_decision_ratio(pairs, config);
    remove_overlapping_pairs(pairs);

    std::pair<std::vector<SourcePosition>, std::vector<SourcePosition>> result;

    for (const auto &pair : pairs)
    {
        result.first.push_back(pair.first->get_source_position());
        result.second.push_back(pair.second->get_source_position());
    }

    return result;
}

DifferenceResult difference(const std::shared_ptr<Node> &leftFile1,
                            const std::shared_ptr<Node> &leftFile2,
                            const std::shared_ptr<Node> &rightFile1,
                            const std::shared_ptr<Node> &rightFile2,
                            const Configuration &config)
{
    auto left_side_intersection = intersection(leftFile1, leftFile2, config);

    auto lf1_rf1_intersection = intersection(leftFile1, rightFile1, config);
    auto lf1_rf2_intersection = intersection(leftFile1, rightFile2, config);

    auto lf2_rf1_intersection = intersection(leftFile2, rightFile1, config);
    auto lf2_rf2_intersection = intersection(leftFile2, rightFile2, config);

    std::vector<SourcePosition> file1_positions_to_remove;
    file1_positions_to_remove.insert(file1_positions_to_remove.end(), lf1_rf1_intersection.first.begin(), lf1_rf1_intersection.first.end());
    file1_positions_to_remove.insert(file1_positions_to_remove.end(), lf1_rf2_intersection.first.begin(), lf1_rf2_intersection.first.end());
    std::vector<SourcePosition> file2_positions_to_remove;
    file2_positions_to_remove.insert(file2_positions_to_remove.end(), lf2_rf1_intersection.first.begin(), lf2_rf1_intersection.first.end());
    file2_positions_to_remove.insert(file2_positions_to_remove.end(), lf2_rf2_intersection.first.begin(), lf2_rf2_intersection.first.end());

    return DifferenceResult{left_side_intersection.first, file1_positions_to_remove, left_side_intersection.second, file2_positions_to_remove};
}
