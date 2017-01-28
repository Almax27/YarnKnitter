#pragma once

#include <string>
#include <vector>

struct DialogueOption
{
    bool isConditionMet;
    std::string content;
};

struct DialogueContent
{
    std::string actorKey;
    std::string speech;
    std::vector<DialogueOption> options;
};
