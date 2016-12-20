#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

struct DialogueNode;
struct DialogueLine;
class IDialogueResolver;

class DialogueLineParser
{
public:
    typedef std::function<std::string(std::string)> VariableResolverFunc;
public:
	virtual ~DialogueLineParser();
	DialogueLineParser(const IDialogueResolver* _resolver);

    void parse(const std::string& _title,
               const std::string& _tags,
               const std::string& _body,
               unsigned _seed,
               std::vector<DialogueNode>& out_nodes);
    
    bool resolveCondition(const std::string& _string);
    std::string substituteVariables(const std::string& _string);

protected:
    
    const IDialogueResolver* m_resolver;
    
    bool parseLine(const std::string& _string, DialogueNode& out_line);
    
    void parseGroups(std::string& s,
                     const std::string& _start,
                     const std::string _end,
                     std::vector<std::string>& out_contents);
    
	void parseGroupedLists(std::string& s,
                           const std::string& _start,
                           const std::string _end,
                           const std::string _seperator,
                           std::vector<std::vector<std::string>>& out_lists);
    
    size_t parseNodes(const std::string& _title,
                      const std::string& _tags,
                      const std::vector<std::string>& _lines,
                      size_t _lineIndex,
                      int _indentLevel,
                      std::vector<DialogueNode>& _nodeSet);
private:
};