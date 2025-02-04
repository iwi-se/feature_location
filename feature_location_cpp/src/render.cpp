#include "render.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include "tree.hpp"
#include "configuration.hpp"

void render_file(std::filesystem::path file, std::vector<SourcePosition> source_positions, Configuration config)
{
    std::cout << "Rendering file: " << config.base_path / file << std::endl;
    std::ifstream input_file(config.base_path / file);
    if (!input_file.is_open())
    {
        std::cerr << "Failed to open file: " << file << std::endl;
        return;
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

    result += "<html><head></head><body><pre>";

    // Sort source positions by start position
    std::sort(source_positions.begin(), source_positions.end());
    

    int current_source_position_index = 0;
    for (std::size_t currentLineIndex = 0; currentLineIndex < lines.size(); currentLineIndex++)
    {
        std::string currentLine{lines[currentLineIndex]};
        for (std::size_t currentColumnIndex = 0; currentColumnIndex < currentLine.size(); currentColumnIndex++)
        {
            std::cout << currentLine[currentColumnIndex] << " " << currentLineIndex << ":" << currentColumnIndex;
            SourcePosition current_source_position{source_positions[current_source_position_index]};
            if (current_source_position.get_start_position().first == currentLineIndex && current_source_position.get_start_position().second == currentColumnIndex)
            {
                std::cout << "highlight" << std::endl;
                result += "<span class=\"highlight\">";
            }
            if (currentLine[currentColumnIndex] == '<')
            {
                result += "&lt;";
            }
            else if (currentLine[currentColumnIndex] == '>')
            {
                result += "&gt;";
            }
            else
            {
                result += currentLine[currentColumnIndex];
            }
            if (current_source_position.get_end_position().first == currentLineIndex && current_source_position.get_end_position().second == currentColumnIndex)
            {
                std::cout << "end highlight" << std::endl;
                result += "</span>";
                if (current_source_position_index < source_positions.size())
                {
                    current_source_position_index++;
                }
            }
            std::cout << std::endl;
        }
    }
    result += "</pre></body></html>";

    std::ofstream output_file(file.replace_extension(".html").filename());
    output_file << result;
    output_file.close();
}
