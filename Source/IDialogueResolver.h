#pragma once

#include <string>
#include <vector>

class DialogueController;

class IDialogueResolver
{
public:
    virtual ~IDialogueResolver() {};

    virtual bool resolveVariable(const std::string& _varName, std::string& out_value) const = 0;
    virtual bool resolveAction(const std::string& _name, const std::vector<std::string>& _params) const = 0;
};
