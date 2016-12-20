#include "DialogueLineParser.h"

#include "DialogueMacros.h"
#include "DialogueNode.h"
#include "IDialogueResolver.h"

#include <sstream>
#include <regex>
#include <algorithm>

using namespace std;

const string k_lineCommentBegin = "//";
const string k_separator = "|";
const string k_actorContentSeparator = ":";
const string k_ifBegin = "<<if", k_ifEnd = ">>";
const string k_actionBegin = "<<", k_actionEnd = ">>";
const string k_gotoBegin = "[[", k_gotoEnd = "]]";
const string k_variableBegin = "$(", k_variableEnd = ")";
const string k_optionShortcut = "->";
const string k_potentialLine = "%";

//------------------------------------
//HELPER FUNCTIONS
void split(const string &s, const string delim, vector<string> &elems)
{
    auto start = 0U;
    auto end = s.find(delim);
    while (end != std::string::npos)
    {
        elems.push_back(s.substr(start, end - start));
        start = end + delim.length();
        end = s.find(delim, start);
    }
    elems.push_back(s.substr(start, end));
}

std::string& trim(std::string& s)
{
    const std::string& delimiters = " \f\n\r\t\v";
    s.erase(0, s.find_first_not_of(delimiters)); //trim left
    s.erase(s.find_last_not_of(delimiters) + 1); //trim right
    return s;
}

bool startsWith(const string& _string, const string& _startsWith)
{
    if(_string.empty()) return false;
    return _string.substr(0, _startsWith.length()) == _startsWith;
}

int calulateIndentLevel(const string& _string)
{
    const unsigned spacesPerLevel = 4;
    unsigned level = 0;
    unsigned spaces = 0;
    for(const auto& c : _string)
    {
        if(c == ' ')
        {
            spaces++;
        }
        else if(c == '\t')
        {
            level++;
            spaces = 0;
        }
        else
        {
            break;
        }
    }
    level += spaces / spacesPerLevel;
    return level;
}

template <typename T>
T fromString(const std::string &str)
{
	std::istringstream is(str);
	T t;
	is >> t;
	if (is.fail()) throw std::invalid_argument("");
	return t;
}

//------------------------------------
//DialogueLineParser implementation
DialogueLineParser::DialogueLineParser(const IDialogueResolver* _resolver)
: m_resolver(_resolver)
{

}

DialogueLineParser::~DialogueLineParser()
{

}

void DialogueLineParser::parse(const string& _name,
                               const string& _tags,
                               const string& _body,
                               unsigned _seed,
                               std::vector<DialogueNode>& out_nodes)
{
    srand(_seed);

    istringstream stream(_body);
    string lineString;
    vector<string> lines;
    while (getline(stream, lineString))
    {
        //strip comments
        auto lineCommentStartIndex = lineString.find(k_lineCommentBegin);
        if(lineCommentStartIndex != string::npos)
        {
            lineString = lineString.substr(0, lineCommentStartIndex);
        }
		//replace endline tokens
		{
			size_t pos = 0;
			std::string token("\\n");
			while ((pos = lineString.find(token, pos)) != std::string::npos) {
				lineString.replace(pos, token.length(), "\n");
				pos += 1;
			}
		}
        //add line
        lines.push_back(lineString);
    }

    parseNodes(_name, _tags, lines, 0, 0, out_nodes);
}

size_t DialogueLineParser::parseNodes(const string& _name,
                                      const string& _tags,
                                      const vector<string>& _lines,
                                      size_t _lineIndex,
                                      int _indentLevel,
                                      std::vector<DialogueNode>& _nodeSet)
{
    DialogueNode node;
    node.name = _name;
    node.tags = _tags;

	DialogueNode potentialNode; //used to store potential nodes
    const auto flushPotentialLines = [&]()
    {
        if(potentialNode.lines.empty() == false)
        {
            const auto& resolvedLine = potentialNode.lines[rand() % potentialNode.lines.size()];
			node.lines.push_back(resolvedLine);
			potentialNode.lines.clear();
        }
    };

    size_t i = _lineIndex;
    for(; i < _lines.size();)
    {
        string lineString = _lines[i];
        int lineIndent = calulateIndentLevel(lineString);
		trim(lineString);

		//if this line is an option and we're processing potentials then parse this into the potential node
		if (startsWith(lineString, k_optionShortcut) && potentialNode.lines.empty() == false)
		{
			parseLine(lineString, potentialNode);
			i++;
			continue;
		}

        //not for us, parse next depth
        if(lineIndent > _indentLevel)
        {
            stringstream nameStream;
            nameStream << _name << ":" << i;
            i = parseNodes(nameStream.str(), _tags, _lines, i, lineIndent, _nodeSet);

            //TODO: probably a better way of doing this, also it looks disgusting :D
			//link option on previous line to most recently parsed node
			DialogueNode::Line* previousLine = nullptr;
			if (potentialNode.lines.empty() == false)
			{
				previousLine = &potentialNode.lines.back();
			}
			else
			{
				if (node.lines.empty() == false)
				{
					previousLine = &node.lines.back();
				}
			}
            if(previousLine != nullptr)
            {
                if(previousLine->options.empty() == false)
                {
                    auto& option = previousLine->options.back();
                    if(option.isShortcut)
                    {
                        if(option.gotoNode.empty() == false)
                        {
                            LOG("Overriding Node(%s) option(%s) goto(%s) with implicit indentation", _name.c_str(), option.content.c_str(), option.gotoNode.c_str());
                        }
                        option.gotoNode = _nodeSet.back().name;
                    }
                }
                else
                {
					previousLine->gotoNode = _nodeSet.back().name;
                }
            }
            continue;
        }

        //break out of scope, we're done
        if(lineIndent < _indentLevel)
        {
            break;
        }

        i++;

        //if this line is is a potential remove the potential symbol and parse into the potential node
        if(startsWith(lineString, k_potentialLine))
        {
			parseLine(lineString.substr(k_potentialLine.size()), potentialNode);
        }
        //else this line is not potential, so flush and parse as usual
        else
        {
            flushPotentialLines();
            parseLine(lineString, node);
        }
    }

    flushPotentialLines();

    _nodeSet.push_back(node);

    return i;
}

bool DialogueLineParser::parseLine(const std::string &_lineString, DialogueNode &_node)
{
    string lineString = _lineString;

    if(lineString.empty()) return false;

	const auto parseOutCommon = [this, &lineString](vector<string>* out_conditions,
													vector<DialogueNode::Action>* out_actions,
													string* out_goto)
	{
		//parse out conditions
		if (out_conditions && lineString.empty() == false)
		{
			vector<string> groupContents;
			parseGroups(lineString, k_ifBegin, k_ifEnd, groupContents);
			for (const auto& content : groupContents)
			{
				out_conditions->push_back(content);
			}
		}

		//parse out actions
		if (out_actions && lineString.empty() == false)
		{
			vector<vector<string>> actionGroups;
            this->parseGroupedLists(lineString, k_actionBegin, k_actionEnd, k_separator, actionGroups);
			for (auto& action : actionGroups)
			{
                if(action.empty())
                {
                    CCLOGERROR("Failed to parse empty action");
                }
                else
                {
                    string name = action[0];
                    vector<string> params(action.begin() + 1, action.end());
                    DialogueNode::Action action = { name, params };
                    out_actions->push_back(action);
                }
			}
		}

		//parse out gotos
		if (out_goto && lineString.empty() == false)
		{
			vector<string> groupContents;
			parseGroups(lineString, k_gotoBegin, k_gotoEnd, groupContents);
			if (groupContents.size() > 0)
			{ //use first, ignore the rest
				*out_goto = groupContents[0];
			}
		}
	};

    if(startsWith(lineString, k_optionShortcut))
    { //parse shortcut options
        lineString = lineString.substr(2);

        DialogueNode::Option option;
        option.isShortcut = true;

		parseOutCommon(&option.conditions, &option.actions, &option.gotoNode);

        option.content = lineString;
        trim(option.content);

        //add option to node
        if(_node.lines.empty())
        {
            _node.lines.push_back(DialogueNode::Line());
        }
        _node.lines.back().options.push_back(option);

        return false; //todo: handle other things to parse, conditions?
    }

    DialogueNode::Line newLine;

	//parse out gotos
    if (lineString.empty() == false)
    {
        vector<vector<string>> gotoParamGroups;
        parseGroupedLists(lineString, k_gotoBegin, k_gotoEnd, k_separator, gotoParamGroups);
        for (auto& params : gotoParamGroups)
        {
            if (params.size() == 1)
            {
                newLine.gotoNode = params[0];
				break;
            }
            else if(params.size() > 1)
            {
                DialogueNode::Option option = { params[0], params[1], false };
                newLine.options.push_back(option);
            }
        }
    }

	parseOutCommon(&newLine.conditions, &newLine.actions, nullptr);

    if(lineString.empty() == false)
    { //parse out actor key
        auto len = lineString.find(k_actorContentSeparator);
        if (len != string::npos)
        {
            newLine.actorKey = lineString.substr(0, len);
            trim(newLine.actorKey);
            lineString = lineString.substr(len + 1);
        }
    }

    //parse the remainder as content
    newLine.content = lineString;
    trim(newLine.content);

    _node.lines.push_back(newLine);

    return true;
}

bool DialogueLineParser::resolveCondition(const std::string &_string)
{
    LOG("Resolving condition if(%s)", _string.c_str());

    std::string conditionString = _string;
    trim(conditionString);

    conditionString = substituteVariables(conditionString);

    //parse operator
    enum class OperatorType
    {
        Equals,
        NotEquals,
        GreaterThan,
        LessThan,
        GreaterThanOrEqualTo,
        LessThanOrEqualTo,
        None
    };
    std::vector<std::pair<std::string, OperatorType>> ops =
    {
        {"==", OperatorType::Equals},
        {"!=", OperatorType::NotEquals},
        {">=", OperatorType::GreaterThanOrEqualTo},
        {"<=", OperatorType::LessThanOrEqualTo},
        {">", OperatorType::GreaterThan},
        {"<", OperatorType::LessThan}
    };

    //find the operator
    std::string lvalue(conditionString), rvalue;
    OperatorType op = OperatorType::None;
    for(auto& opPair : ops)
    {
        auto index = conditionString.find(opPair.first);
        if(index != string::npos)
        {
            lvalue = conditionString.substr(0, index);
            rvalue = conditionString.substr(index + opPair.first.size(), string::npos);
            op = opPair.second;
            break;
        }
    }

    //trim
    trim(lvalue);
    trim(rvalue);

    //represent empty strings as false
    if(lvalue.empty()) lvalue = "false";
    if(rvalue.empty()) rvalue = "false";

    try {
        switch(op)
        {
            case OperatorType::Equals:
                return lvalue == rvalue;
            case OperatorType::NotEquals:
                return lvalue != rvalue;
            case OperatorType::GreaterThanOrEqualTo:
                return fromString<float>(lvalue) >= fromString<float>(rvalue);
            case OperatorType::LessThanOrEqualTo:
                return fromString<float>(lvalue) <= fromString<float>(rvalue);
            case OperatorType::GreaterThan:
                return fromString<float>(lvalue) > fromString<float>(rvalue);
            case OperatorType::LessThan:
                return fromString<float>(lvalue) < fromString<float>(rvalue);
            case OperatorType::None:
            {
                string lvalueLower = lvalue;
                transform(lvalueLower.begin(), lvalueLower.end(), lvalueLower.begin(), ::tolower);
                if(lvalueLower == "true") return true;
                if(lvalueLower == "false") return false;

                return fromString<float>(lvalue) != 0;
            }
            default:
                LOGERROR("Unknown operation type");
                break;
        }
    } catch (std::invalid_argument e)
    {
        LOGERROR("Failed to resolve: '%s'", _string.c_str());
        LOGERROR("%s", e.what());
        return false;
    }

    ASSERT(false);
    return false;

}

std::string DialogueLineParser::substituteVariables(const std::string& _string)
{
    if(m_resolver == nullptr)
    {
        LOGERROR("Failed to substitute variables: Invalid resolver");
        return _string;
    }

    string s(_string);
    auto startPos = _string.find(k_variableBegin) + k_variableBegin.size();
    auto endPos = _string.find(k_variableEnd);
    while (startPos != string::npos && endPos != string::npos)
    {
        string varName = s.substr(startPos, endPos - startPos);
        string value;
        m_resolver->resolveVariable(varName, value);
        s.replace(startPos - k_variableBegin.size(), k_variableBegin.size() + varName.size() + k_variableEnd.size(), value);

        auto nextSearchIndex = startPos + value.size();
        startPos = s.find(k_variableBegin, nextSearchIndex) + k_variableBegin.size();
        endPos = s.find(k_variableEnd, nextSearchIndex);
    }
    return s;
}

void DialogueLineParser::parseGroups(std::string& s,
                                     const std::string& _start,
                                     const std::string _end,
                                     std::vector<std::string>& out_contents)
{
    auto startPos = s.find(_start);
    auto endPos = s.find(_end);
    string prefix = s.substr(0, startPos);
    while (startPos != string::npos && endPos != string::npos)
    {
        startPos += _start.size(); //don't include start
        string content = s.substr(startPos, endPos - startPos);
        trim(content);

        out_contents.push_back(content);

        s = prefix + s.substr(endPos + _end.size());
        startPos = s.find(_start);
        endPos = s.find(_end);
    }
}

void DialogueLineParser::parseGroupedLists(string &s, const string &_start, const string _end, const string _seperator, vector<vector<string>> &out_lists)
{
    vector<string> groups;
    parseGroups(s, _start, _end, groups);
    for(auto& groupContent : groups)
    {
        vector<string> list;
        split(groupContent, _seperator, list);
        for(auto& item : list)
        {
            trim(item);
        }
        out_lists.push_back(list);
    }
}
