#include "render.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include "tree.hpp"
#include "configuration.hpp"
#include "set_operations.hpp"

enum class CharacterColor
{
    TRANSPARENT,
    GREEN,
    RED
};

struct CharacterWithColor
{
    std::string character; // Needed for HTML escape sequences
    CharacterColor color;
};

enum class RelativePosition
{
    BEFORE,
    INSIDE,
    AFTER
};

CharacterWithColor make_character_with_color_from_character(const char &character)
{
    return CharacterWithColor{std::string(1, character), CharacterColor::TRANSPARENT};
}

RelativePosition get_relative_position(const SourcePosition &source_position, const std::size_t &line_index, const std::size_t &column_index)
{
    if (line_index < source_position.get_start_line() || (line_index == source_position.get_start_line() && column_index < source_position.get_start_column()))
    {
        return RelativePosition::BEFORE;
    }
    else if (line_index > source_position.get_end_line() || (line_index == source_position.get_end_line() && column_index >= source_position.get_end_column()))
    {
        return RelativePosition::AFTER;
    }
    else
    {
        return RelativePosition::INSIDE;
    }
}

void mark_character_color(std::vector<std::vector<CharacterWithColor>> &character_lines, std::vector<SourcePosition> &source_positions, const CharacterColor &color)
{
    // Sort source positions by start position
    std::sort(source_positions.begin(), source_positions.end());

    int current_source_position_index = 0;
    for (std::size_t currentLineIndex = 0; currentLineIndex < character_lines.size(); currentLineIndex++)
    {
        std::vector<CharacterWithColor> &currentLine{character_lines[currentLineIndex]};
        for (std::size_t currentColumnIndex = 0; currentColumnIndex < currentLine.size(); currentColumnIndex++)
        {
            std::cout << currentLine[currentColumnIndex].character << " " << currentLineIndex << ":" << currentColumnIndex;
            SourcePosition current_source_position{source_positions[current_source_position_index]};
            if (get_relative_position(current_source_position, currentLineIndex, currentColumnIndex) == RelativePosition::AFTER)
            {
                std::cout << "end highlight";
                if (current_source_position_index < source_positions.size() - 1)
                {
                    current_source_position_index++;
                    current_source_position = source_positions[current_source_position_index];
                    std::cout << "\ncurrent_source_position: " << current_source_position.render() << std::endl;
                }
            }
            if (get_relative_position(current_source_position, currentLineIndex, currentColumnIndex) == RelativePosition::INSIDE)
            {
                std::cout << "highlight";
                currentLine[currentColumnIndex].color = color;
            }
            std::cout << std::endl;
        }
    }
}

void escape_html(std::vector<std::vector<CharacterWithColor>> &character_lines)
{
    for (auto &line : character_lines)
    {
        for (auto &character : line)
        {
            if (character.character == "<")
            {
                character.character = "&lt;";
            }
            else if (character.character == ">")
            {
                character.character = "&gt;";
            }
        }
    }
}

std::string render_character_lines(std::vector<std::vector<CharacterWithColor>> &character_lines)
{
    std::string result{};
    for (auto &line : character_lines)
    {
        for (auto &character : line)
        {
            if (character.color == CharacterColor::GREEN)
            {
                result += "<span style=\"background-color:rgb(121, 233, 155);\">";
            }
            else if (character.color == CharacterColor::RED)
            {
                result += "<span style=\"background-color:rgb(233, 121, 155);\">";
            }
            result += character.character;
            if (character.color != CharacterColor::TRANSPARENT)
            {
                result += "</span>";
            }
        }
        result += "<br>";
    }
    return result;
}

std::string render_difference(DifferenceResult difference, Configuration config)
{
    std::string result{"<html><body>"};
    result += render_file(difference.file1_intersection[0].get_file(), config, difference.file1_intersection, difference.file1_subtraction);
    result += render_file(difference.file2_intersection[0].get_file(), config, difference.file2_intersection, difference.file2_subtraction);
    result += "</body></html>";
    return result;
}

std::string render_file(std::filesystem::path file, Configuration config, std::vector<SourcePosition> green_positions, std::vector<SourcePosition> red_positions)
{
    std::cout << "Rendering file: " << file << std::endl;
    std::ifstream input_file(file);
    if (!input_file.is_open())
    {
        std::cerr << "Failed to open file: " << file << std::endl;
        exit(1);
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input_file, line))
    {
        std::cout << "Line: " << line << std::endl;
        lines.push_back(line);
    }
    input_file.close();

    std::string result{};

    result += "<h1>" + file.filename().string() + "</h1>";
    result += "<pre>";

    std::vector<std::vector<CharacterWithColor>> character_lines;
    for (auto &line : lines)
    {
        std::vector<CharacterWithColor> characters;
        for (auto &character : line)
        {
            characters.push_back(make_character_with_color_from_character(character));
        }
        character_lines.push_back(characters);
    }
    mark_character_color(character_lines, green_positions, CharacterColor::GREEN);
    mark_character_color(character_lines, red_positions, CharacterColor::RED);
    escape_html(character_lines);
    result += render_character_lines(character_lines);
    result += "</pre>";

    return result;
}
