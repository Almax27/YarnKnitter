#pragma once

#include <string>
#include <vector>

struct DialogueNode
{
    struct Action
    {
        std::string name;
        std::vector<std::string> params;
    };
    
    struct Option
    {
        std::string content;
        std::string gotoNode;
        bool isShortcut;
        std::vector<std::string> conditions;
        std::vector<Action> actions;
    };
    
    struct Line
    {
        std::string actorKey;
        std::string content;
		std::vector<std::string> conditions;
        std::vector<Option> options;
        std::vector<Action> actions;
        std::string gotoNode;
    };
    
	std::string name;
	std::string tags;
	std::vector<Line> lines;
};