#include "set_operations.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <utility>
#include <memory>
#include <stack>
#include "node_types.hpp"
#include <ranges>

// Assuming Node and Tree classes are defined elsewhere with necessary methods

// Placeholder for calculate_depth_proximity function
// double calculate_depth_proximity(const std::shared_ptr<Node> &node1, const std::shared_ptr<Node> &node2)
// {
//     // Implement depth proximity calculation
//     return 0.0;
// }

// // Placeholder for calculate_environment_similarity function
// double calculate_environment_similarity(const std::shared_ptr<Node> &node1, const std::shared_ptr<Node> &node2)
// {
//     // Implement environment similarity calculation
//     return 0.0;
// }


void sort_by_decision_ratio(MatchList &matches, const Configuration &config)
{
    std::vector<std::pair<Match, double>> matches_with_ratio;

    for (const Match &match : matches)
    {
        // double depth_proximity = calculate_depth_proximity(pair.first, pair.second);
        // double environment_similarity = calculate_environment_similarity(pair.first, pair.second);
        int size = match[0]->get_connected_leaf_weight();

        // double decision_ratio = 0.2 * depth_proximity + 0.3 * environment_similarity + 0.5 * std::min((size / 100.0), 1.0);

        double decision_ratio = size;
        matches_with_ratio.push_back(std::make_pair(match, decision_ratio));
    }

    std::sort(matches_with_ratio.begin(), matches_with_ratio.end(),
              [](const std::tuple<std::pair<std::shared_ptr<Node>, std::shared_ptr<Node>>, double> &a, const std::tuple<std::pair<std::shared_ptr<Node>, std::shared_ptr<Node>>, double> &b)
              {
                  return std::get<1>(a) > std::get<1>(b);
              });

    matches.clear();
    for (const auto &pair : matches_with_ratio)
    {
        if (std::get<1>(pair) > config.options.minimum_trace_weight)
        {
            matches.push_back(std::get<0>(pair));
        }
    }
}

bool non_overlapping(const Match &match,
                     const MatchList &result)
{
    for (const auto &existing_match : result)
    {
        std::vector<Node::RelativePosition> all_relative_positions;
        for (size_t i = 0; i < match.size(); i++)
        {
            Node::RelativePosition pos = match[i]->get_relative_position(existing_match[i]);
            all_relative_positions.push_back(pos);
        }
        if (std::ranges::adjacent_find(all_relative_positions, std::ranges::not_equal_to()) != all_relative_positions.end() || all_relative_positions[0] == Node::RelativePosition::overlapping)
        {
            return false;
        }
    }
    return true;
}

void remove_overlapping_pairs(MatchList &matches)
{
    MatchList result;
    for (const auto &match : matches)
    {
        if (non_overlapping(match, result))
        {
            result.push_back(match);
        }
    }
    matches = result;
}

MatchList match_node_in_trees(const std::shared_ptr<Node> &node, std::vector<std::shared_ptr<Node>> trees, const Configuration &config)
{
    MatchList matches;
    std::stack<std::shared_ptr<Node>> stack{{trees[0]}};
    trees.erase(trees.begin());
    while (!stack.empty())
    {
        auto current_node = stack.top();
        stack.pop();
        if (current_node->get_subtree_hash() == node->get_subtree_hash())
        {
            if (trees.empty())
            {
                matches.push_back({current_node});
            }
            else
            {
                MatchList matches_in_trees = match_node_in_trees(current_node, trees, config);
                for (auto &match : matches_in_trees)
                {
                    // insert current_node at the beginning of each match
                    match.insert(match.begin(), current_node);
                }
                matches.insert(matches.end(), matches_in_trees.begin(), matches_in_trees.end());
            }
        }
        else if (current_node->get_connected_leaf_weight() >= node->get_connected_leaf_weight())
        {
            for (const auto &child : current_node->get_children())
            {
                stack.push(child);
            }
        }
    }

    return matches;
}

MatchList match_trees(std::vector<std::shared_ptr<Node>> trees, const Configuration &config)
{
    std::vector<std::vector<std::shared_ptr<Node>>> matches;
    std::stack<std::shared_ptr<Node>> stack{{trees[0]}};
    while (!stack.empty())
    {
        auto current_node = stack.top();
        stack.pop();
        MatchList node_matches;

        if (is_included_node_type(current_node, config))
        {
            std::vector<std::shared_ptr<Node>> remaining_trees{trees.begin() + 1, trees.end()};
            node_matches = match_node_in_trees(current_node, remaining_trees, config);
        }

        if (node_matches.empty() || node_matches[0].empty())
        {
            for (const auto &child : current_node->get_children())
            {
                stack.push(child);
            }
        }
        else
        {
            for (const auto &match : node_matches)
            {
                matches.push_back(match);
            }
        }
    }
    return matches;
}

MatchList intersection(
    const std::vector<std::shared_ptr<Node>> &nodes,
    const Configuration &config)
{
    MatchList matches = match_trees(nodes, config);
    sort_by_decision_ratio(matches, config);
    remove_overlapping_pairs(matches);

    return matches;
}

DifferenceResult difference(const std::shared_ptr<Node> &leftFile1,
                            const std::shared_ptr<Node> &leftFile2,
                            const std::shared_ptr<Node> &rightFile1,
                            const std::shared_ptr<Node> &rightFile2,
                            const Configuration &config)
{
    auto left_side_intersection = intersection({leftFile1, leftFile2}, config);

    auto lf1_rf1_intersection = intersection({leftFile1, rightFile1}, config);
    auto lf1_rf2_intersection = intersection({leftFile1, rightFile2}, config);

    auto lf2_rf1_intersection = intersection({leftFile2, rightFile1}, config);
    auto lf2_rf2_intersection = intersection({leftFile2, rightFile2}, config);

    std::vector<std::shared_ptr<Node>> file1_positions_to_remove;
    file1_positions_to_remove.insert(file1_positions_to_remove.end(), lf1_rf1_intersection.first.begin(), lf1_rf1_intersection.first.end());
    file1_positions_to_remove.insert(file1_positions_to_remove.end(), lf1_rf2_intersection.first.begin(), lf1_rf2_intersection.first.end());
    std::vector<std::shared_ptr<Node>> file2_positions_to_remove;
    file2_positions_to_remove.insert(file2_positions_to_remove.end(), lf2_rf1_intersection.first.begin(), lf2_rf1_intersection.first.end());
    file2_positions_to_remove.insert(file2_positions_to_remove.end(), lf2_rf2_intersection.first.begin(), lf2_rf2_intersection.first.end());

    return DifferenceResult{left_side_intersection.first, file1_positions_to_remove, left_side_intersection.second, file2_positions_to_remove};
}
