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

MatchesPerFile extract_matches_per_file(const MatchList &matches, const int &index)
{
    MatchesPerFile matches_per_file;
    for (const auto &match : matches)
    {
        matches_per_file.push_back(match[index]);
    }
    return matches_per_file;
}

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
              [](const std::pair<Match, double> &a, const std::pair<Match, double> &b)
              {
                  return a.second > b.second;
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
                matches.push_back({node, current_node});
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

DifferenceResult difference(
    const std::vector<std::shared_ptr<Node>> &leftFiles, 
    const std::vector<std::shared_ptr<Node>> &rightFiles, 
    const Configuration &config)
{
    auto left_side_intersection = intersection(leftFiles, config);

    // For now do a cartesian product of the left and right files
    std::vector<FileDifferenceResult> difference_results;
    for (int index = 0; index < leftFiles.size(); index++)
    {
        FileDifferenceResult file_difference_result;
        file_difference_result.intersection = extract_matches_per_file(left_side_intersection, index);

        for (const auto &right_file : rightFiles)
        {
            auto subtraction = intersection({leftFiles[index], right_file}, config);
            auto subtraction_left_side = extract_matches_per_file(subtraction, 0);
            file_difference_result.subtraction.insert(file_difference_result.subtraction.end(), subtraction_left_side.begin(), subtraction_left_side.end());
        }
        difference_results.push_back(file_difference_result);
    }

    return difference_results;
}
