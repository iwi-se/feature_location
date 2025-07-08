#include "render.hpp"
#include "configuration.hpp"
#include "set_operations.hpp"
#include "tree.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

enum class CharacterColor
{
  TRANSPARENT,
  GREEN,
  RED
};

struct CharacterWithColor
{
    std::string    character; // Needed for HTML escape sequences
    CharacterColor color;
    size_t         weight { 0 }; // Weight for the character, default is 1
};

enum class RelativePosition
{
  BEFORE,
  INSIDE,
  AFTER
};

CharacterWithColor makeCharacterWithColorFromCharacter(const char &character)
{
  return CharacterWithColor { std::string(1, character),
                              CharacterColor::TRANSPARENT };
}

RelativePosition getRelativePosition(const SourcePosition &sourcePosition,
                                     const std::size_t    &lineIndex,
                                     const std::size_t    &columnIndex)
{
  if (lineIndex < sourcePosition.getStartLine()
      || (lineIndex == sourcePosition.getStartLine()
          && columnIndex < sourcePosition.getStartColumn()))
  {
    return RelativePosition::BEFORE;
  }
  else if (lineIndex > sourcePosition.getEndLine()
           || (lineIndex == sourcePosition.getEndLine()
               && columnIndex >= sourcePosition.getEndColumn()))
  {
    return RelativePosition::AFTER;
  }
  else
  {
    return RelativePosition::INSIDE;
  }
}

void markCharacterColor(
    std::vector<std::vector<CharacterWithColor>>   &characterLines,
    std::vector<std::pair<SourcePosition, size_t>> &sourcePositions,
    const CharacterColor                           &color)
{
  if (sourcePositions.size() == 0)
  {
    return;
  }

  // Sort source positions by start position
  std::sort(sourcePositions.begin(), sourcePositions.end());

  size_t currentSourcePositionIndex = 0;
  for (std::size_t currentLineIndex = 0;
       currentLineIndex < characterLines.size();
       currentLineIndex++)
  {
    std::vector<CharacterWithColor> &currentLine {
      characterLines[currentLineIndex]
    };
    for (std::size_t currentColumnIndex = 0;
         currentColumnIndex < currentLine.size();
         currentColumnIndex++)
    {
      auto *currentSourcePosition {
        &sourcePositions[currentSourcePositionIndex]
      };
      while (getRelativePosition(currentSourcePosition->first,
                                 currentLineIndex,
                                 currentColumnIndex)
                 == RelativePosition::AFTER
             && currentSourcePositionIndex < sourcePositions.size() - 1)
      {
        currentSourcePositionIndex++;
        currentSourcePosition = &sourcePositions[currentSourcePositionIndex];
      }
      if (getRelativePosition(currentSourcePosition->first,
                              currentLineIndex,
                              currentColumnIndex)
              == RelativePosition::INSIDE
          && currentLine[currentColumnIndex].character != " ")
      {
        currentLine[currentColumnIndex].color  = color;
        currentLine[currentColumnIndex].weight = currentSourcePosition->second;
      }
    }
  }
}

void escapeHtml(std::vector<std::vector<CharacterWithColor>> &characterLines)
{
  for (auto &line : characterLines)
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

std::string renderCharacterLines(
    std::vector<std::vector<CharacterWithColor>> &characterLines)
{
  std::stringstream result {};
  CharacterColor    currentColor { CharacterColor::TRANSPARENT };
  size_t            lineCount { 1 };
  std::stringstream lineNumberDiv {};
  lineNumberDiv << "<div style=\"margin-right: 3px; background-color: "
                   "lightgray; padding: 0 5px; text-align: right;\">";
  std::stringstream codeDiv {};
  codeDiv << "<div>";
  for (auto &line : characterLines)
  {
    lineNumberDiv << "<span>" << lineCount << "</span><br>";
    for (auto &character : line)
    {
      if (character.color != currentColor)
      {
        if (currentColor != CharacterColor::TRANSPARENT)
        {
          codeDiv << "</span>";
        }
        currentColor = character.color;
        if (currentColor == CharacterColor::GREEN)
        {
          codeDiv
              << "<span style=\"background-color:rgb(121, 233, 155);\" title=\""
              << character.weight << "\">";
        }
        else if (currentColor == CharacterColor::RED)
        {
          codeDiv << "<span style=\"background-color:rgb(233, 121, 155);\">";
        }
      }
      codeDiv << character.character;
    }
    codeDiv << "<br>";
    lineCount++;
  }
  lineNumberDiv << "</div>";
  codeDiv << "</span></div>";
  result << "<div style=\"display: flex;\">";
  result << lineNumberDiv.str() << codeDiv.str() << "</div>";
  return result.str();
}

std::string renderDifference(DifferenceResult     difference,
                             const Configuration &config)
{
  std::string result { "<html><body>" };

  for (auto &fileDifferenceResult : difference.result)
  {
    if (fileDifferenceResult.intersection.size() > 0)
    {
      result += renderFile(
          fileDifferenceResult.intersection[0]->getSourcePosition().getFile(),
          config,
          fileDifferenceResult.intersection,
          fileDifferenceResult.subtraction);
    }
  }

  result += "</body></html>";
  return result;
}

void debugPrintMarkedCharacters(
    std::vector<std::vector<CharacterWithColor>> &characterLines)
{
  for (auto &line : characterLines)
  {
    for (auto &character : line)
    {
      if (character.color == CharacterColor::GREEN)
      {
        std::cout << character.character << "[green]";
      }
      else if (character.color == CharacterColor::RED)
      {
        std::cout << character.character << "[red]";
      }
      else
      {
        std::cout << character.character;
      }
    }
    std::cout << std::endl;
  }
}

std::string renderFile(std::filesystem::path file,
                       const Configuration  &config,
                       std::vector<Node *>   greenNodes,
                       std::vector<Node *>   redNodes)
{
  std::vector<std::pair<SourcePosition, size_t>> greenPositions;
  std::vector<std::pair<SourcePosition, size_t>> redPositions;

  for (auto &node : greenNodes)
  {
    greenPositions.push_back(
        std::make_pair(node->getSourcePosition(), node->structuralSimilarity));
  }

  for (auto &node : redNodes)
  {
    redPositions.push_back(std::make_pair(node->getSourcePosition(), 0));
  }

  if (config.options.debug)
  {
    std::cout << "Green positions: " << std::endl;
    for (auto &position : greenPositions)
    {
      std::cout << position.first.getStartLine() << ":"
                << position.first.getStartColumn() << " - "
                << position.first.getEndLine() << ":"
                << position.first.getEndColumn() << std::endl;
    }
    std::cout << "Red positions: " << std::endl;
    for (auto &position : redPositions)
    {
      std::cout << position.first.getStartLine() << ":"
                << position.first.getStartColumn() << " - "
                << position.first.getEndLine() << ":"
                << position.first.getEndColumn() << std::endl;
    }
  }

  std::ifstream inputFile(file);
  if (!inputFile.is_open())
  {
    std::cerr << "Failed to open file: " << file << std::endl;
    exit(1);
  }

  std::vector<std::string> lines;
  std::string              line;
  while (std::getline(inputFile, line))
  {
    lines.push_back(line);
  }
  inputFile.close();

  std::string result {};

  result += "<h1>" + file.filename().string() + "</h1>";
  result += "<pre>";

  std::vector<std::vector<CharacterWithColor>> characterLines;
  characterLines.reserve(lines.size());
  for (auto &line : lines)
  {
    std::vector<CharacterWithColor> characters;
    characters.reserve(line.size());
    for (auto &character : line)
    {
      characters.push_back(makeCharacterWithColorFromCharacter(character));
    }
    characterLines.push_back(characters);
  }

  markCharacterColor(characterLines, greenPositions, CharacterColor::GREEN);
  markCharacterColor(characterLines, redPositions, CharacterColor::RED);

  if (config.options.debug)
  {
    debugPrintMarkedCharacters(characterLines);
  }

  escapeHtml(characterLines);
  result += renderCharacterLines(characterLines);
  result += "</pre>";

  return result;
}
