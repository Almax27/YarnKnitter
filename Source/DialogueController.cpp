#include "DialogueController.h"

#include "DialogueMacros.h"

#include "DialogueLineParser.h"
#include "DialogueNode.h"
#include "DialogueContent.h"

#include "IDialogueDelegate.h"
#include "IDialogueResolver.h"

#include <algorithm>
#include <numeric>

DialogueController::DialogueController(IDialogueDelegate* _dialogueDelegate,
                                       const IDialogueResolver* _dialogueResolver)
    : m_dialogueDelegate(_dialogueDelegate)
    , m_dialogueResolver(_dialogueResolver)
    , m_isSkipping(false)
    , m_isProgressing(false)
    , m_isPaused(false)
    , m_pendingStop(false)
{

}

DialogueController::~DialogueController()
{
    clearNodes();
}

//-------------------------------------------------------------------------------------------------------------------
//Dialogue Configuration
//-------------------------------------------------------------------------------------------------------------------
bool DialogueController::addNode(const std::string &_name, const std::string &_tags, const std::string &_body, unsigned _seed)
{
    if(m_dialogueResolver)
    {
        DialogueLineParser parser(m_dialogueResolver);
        std::vector<DialogueNode> nodes;
        parser.parse(_name, _tags, _body, _seed, nodes);
        for(const auto& parsedNode : nodes)
        {
            if(addNode(parsedNode) == false)
            {
                return false;
            }
        }
        return true;
    }
    else
    {
        LOGERROR("Failed to add node: Invalid Dialogue Resolver");
        return false;
    }
}

bool DialogueController::addNode(const DialogueNode& _node)
{
    auto it = m_nodes.find(_node.name);
    if (it == m_nodes.end())
    {
        m_nodes.insert(std::make_pair(_node.name, new DialogueNode(_node)));
        return true;
    }
    else
    {
        LOG("Failed to add node: name '%s' already in use", _node.name.c_str());
        return false;
    }
}

void DialogueController::clearNodes()
{
    for (auto& pair : m_nodes)
    {
        delete pair.second;
    }
    m_nodes.clear();
    m_nodeStack.clear();
}

DialogueNode* DialogueController::getNodeByName(const std::string& _name) const
{
    auto it = m_nodes.find(_name);
    if (it != m_nodes.end())
    {
        return it->second;
    }
    return nullptr;
}

void DialogueController::getActors(std::vector<std::string>& out_actorKeys) const
{
    for(const auto& node : m_nodes)
    {
        for(const auto& line : node.second->lines)
        {
            if(std::find(out_actorKeys.begin(), out_actorKeys.end(), line.actorKey) == out_actorKeys.end())
            {
                out_actorKeys.push_back(line.actorKey);
            }
        }
    }
}

//-------------------------------------------------------------------------------------------------------------------
//Dialogue Control
//-------------------------------------------------------------------------------------------------------------------

bool DialogueController::start(const std::string& _startNode, unsigned _lineIndex, bool _forceStart)
{
    if(m_nodeStack.empty() == false)
    {
        if(_forceStart)
        {
            onDialogueEnded();
        }
        else
        {
            LOG("Failed to start Dialogue: dialogue already running");
            return false;
        }
    }
    if(_startNode.empty())
    {
        const static std::string s_defaultStartNode = "Start";
        LOG("Starting Dialogue at default: '%s':%u", s_defaultStartNode.c_str(), _lineIndex);
        return enterNode(s_defaultStartNode, _lineIndex);
    }
    else
    {
        LOG("Starting Dialogue at: '%s':%u", _startNode.c_str(), _lineIndex);
        return enterNode(_startNode, _lineIndex);
    }
}

bool DialogueController::start(const NodeStack &_nodeStack, bool _forceStart)
{
    if(m_nodeStack.empty() == false)
    {
        if(_forceStart)
        {
            onDialogueEnded();
        }
        else
        {
            LOG("Failed to start Dialogue: dialogue already running");
            return false;
        }
    }
    if(_nodeStack.empty() == false)
    {
        m_nodeStack = _nodeStack;
        if(present(*m_nodeStack.back().node, m_nodeStack.back().lineIndex) == false)
        {
            return advanceLine();
        }
        return true;
    }
    else
    {
        LOG("Failed to start Dialogue: empty stack given");
        return false;
    }
}

bool DialogueController::stop()
{
    if(m_isProgressing)
    {
        m_pendingStop = true;
        return true;
    }
    else if(m_nodeStack.empty() == false)
    {
        onDialogueEnded();
        return true;
    }
    else
    {
        LOGERROR("Failed to stop Dialogue: not running");
        return false;
    }
}

void DialogueController::setIsPaused(bool _isPaused)
{
    if(_isPaused != m_isPaused)
    {
        m_isPaused = _isPaused;
        if(m_isPaused)
        {
            if(m_dialogueDelegate)
            {
                m_dialogueDelegate->onPaused();
            }
        }
        else
        {
            progressDialogue();
        }
    }
}

bool DialogueController::getIsPaused() const
{
    return m_isPaused;
}

bool DialogueController::selectOption(size_t _index)
{
    if(m_isPaused) return false;

    if(m_presentedOptions.empty() == false)
    {
        if(_index < m_presentedOptions.size())
        {
            const auto option = m_presentedOptions[_index];
            m_presentedOptions.clear();

            //resolve actions for option
            if(option.resolveActions) { option.resolveActions(); }

            //progress to next node if any
            if(!m_isPaused && !m_pendingStop && option.nextNode.empty() == false)
            {
                return enterNode(option.nextNode);
            }
            else //progress dialogue as normal
            {
                return run();
            }
        }
        else
        {
            LOGERROR("Failed to select option: invalid index (%zu)", _index);
            return false;
        }
    }
    else
    {
        LOGERROR("Failed to select option: no options avaialble");
        return false;
    }
}

bool DialogueController::progressDialogue()
{
    if(m_isPaused) return false;
    return run();
}

void DialogueController::skipDialogue()
{
    if(m_isPaused) return;

    if (m_nodeStack.empty() == false)
    {
        m_isSkipping = true;
        while (m_isSkipping)
        {
            m_isSkipping = progressDialogue();
        }
    }
    else
    {
        LOGERROR("Failed to skip dialogue: Empty node stack");
    }
}

//-------------------------------------------------------------------------------------------------------------------
//Accessors
//-------------------------------------------------------------------------------------------------------------------

void DialogueController::setDialogueResolver(const IDialogueResolver *_resolver)
{
    m_dialogueResolver = _resolver;
}

const IDialogueResolver* DialogueController::getDialogueResolver() const
{
    return m_dialogueResolver;
}

void DialogueController::setDialogueDelegate(IDialogueDelegate* _delegate)
{
    m_dialogueDelegate = _delegate;
}

IDialogueDelegate* DialogueController::getDialogueDelegate() const
{
    return m_dialogueDelegate;
}

const DialogueController::NodeState* DialogueController::getCurrentNodeState() const
{
    if(m_nodeStack.empty())
    {
        return nullptr;
    }
    else
    {
        return &m_nodeStack.back();
    }
}

const DialogueController::NodeStack* DialogueController::getNodeStack() const
{
    return &m_nodeStack;
}

//-------------------------------------------------------------------------------------------------------------------
//Internal Helpers
//-------------------------------------------------------------------------------------------------------------------

bool DialogueController::run()
{
    bool didProgress = false;

    //cannot progress if
    if(m_presentedOptions.empty())
    {
        //increment the current line
        if(!m_pendingStop)
        {
            didProgress = advanceLine();
        }

        if(m_nodeStack.empty())
        {
            m_pendingStop = true;
        }

        //handle request to stop
        if(m_pendingStop)
        {
            onDialogueEnded();
        }
    }

    return didProgress;
}

bool DialogueController::advanceLine()
{
    DialogueLineParser parser(m_dialogueResolver);

    bool wasProgressing = m_isProgressing;
    m_isProgressing = true;

    bool didAdvance = false;
    while(m_isPaused == false && m_pendingStop == false && didAdvance == false && m_nodeStack.empty() == false)
    {
        auto& activeNode = m_nodeStack.back().node;
        auto& lineIndex = m_nodeStack.back().lineIndex;

        //process exiting the current line
        if (lineIndex < activeNode->lines.size())
        {
            auto& currentLine = activeNode->lines[lineIndex];

            //resolve line conditions
            bool conditionFailed = false;
            for(const auto& condition : currentLine.conditions)
            {
                if(parser.resolveCondition(condition) == false)
                {
                    conditionFailed = true;
                    break;
                }
            }
            if(conditionFailed)
            {
                lineIndex++;
                continue;
            }

            //resolve line actions
            for(const auto& action : currentLine.actions)
            {
                resolveAction(action.name, action.params);
                if(m_isPaused || m_pendingStop)
                {
                    continue;
                }
            }

            //resolve goto
            if (currentLine.gotoNode.empty() == false)
            {
                didAdvance = enterNode(currentLine.gotoNode);
            }
        }
        else
        {
            didAdvance = exitNode();
        }

        //no need to process next line if we've advanced
        if(didAdvance)
        {
            continue;
        }

        //process entering the new line
        if (lineIndex + 1 < activeNode->lines.size())
        {
            lineIndex++;
            didAdvance = present(*activeNode, lineIndex);
        }
        else
        {
            didAdvance = exitNode();
        }
    }

    m_isProgressing = wasProgressing;

    return didAdvance;
}

bool DialogueController::present(const DialogueNode& _node, size_t _index)
{
    if(_index >= _node.lines.size())
    {
        LOGERROR("Failed to present %s:%zu: Invalid line index", _node.name.c_str(), _index);
        return false;
    }

    const auto& line = _node.lines[_index];

    DialogueLineParser parser(m_dialogueResolver);

    //resolve line conditions
    for(const auto& condition : line.conditions)
    {
        if(parser.resolveCondition(condition) == false)
        {
            return false; //return false if a condition fails
        }
    }

    //handle nothing to present
    if(line.actorKey.empty() && line.content.empty())
    {
        //goto another node if necessary
        if(line.gotoNode.empty() == false)
        {
            return enterNode(line.gotoNode);
        }
        else //else return false - nothing to present
        {
            return false;
        }
    }

    //resolve content
    DialogueContent dialogueContent;
    dialogueContent.actorKey = line.actorKey;
    dialogueContent.speech = parser.substituteVariables(line.content);

    //add options
    m_presentedOptions.clear();
    for(const auto& option : line.options)
    {
        bool conditionsMet = true;
        std::string optionContent(option.content);

        //resolve conditions
        for(const auto& condition : option.conditions)
        {
            if(parser.resolveCondition(condition) == false)
            {
                conditionsMet = false;
                break;
            }
        }

        //parse out variables
        optionContent = parser.substituteVariables(optionContent);

        dialogueContent.options.push_back({conditionsMet, optionContent});
        m_presentedOptions.push_back({option.gotoNode, [this, option]
            {
                for(const auto& action : option.actions)
                {
                    resolveAction(action.name, action.params);
                }
            }});
    }

    //notify delegate
    if(m_dialogueDelegate)
    {
        m_dialogueDelegate->onProgress(dialogueContent);
    }

    return true;
}

void DialogueController::resolveAction(const std::string& _actionName, const std::vector<std::string>& _params)
{
    auto nameLower(_actionName);
    std::transform(_actionName.begin(), _actionName.end(), nameLower.begin(), ::tolower);
    if(nameLower == "stop" || nameLower == "end" || nameLower == "fin" || nameLower == "exit")
    {
        stop();
        return;
    }

    DialogueLineParser parser(m_dialogueResolver);
    auto parsedParams = _params;
    for(auto& param : parsedParams)
    {
        param = parser.substituteVariables(param);
    }
#define ACC_VEC(v) (v.empty() ? "" : std::accumulate(v.begin()+1, v.end(), std::string(v.front()), [](std::string& a, std::string& b) {return a + ',' + b;}).c_str())

    if(m_dialogueResolver)
    {
        if(m_dialogueResolver->resolveAction(_actionName, parsedParams) == false)
        {
            LOGERROR("Failed to resolve action '%s(%s): unhandled", _actionName.c_str(), ACC_VEC(parsedParams));
        }
    }
    else
    {
        LOGERROR("Failed to resolve action '%s(%s)': invalid DialogueResolver", _actionName.c_str(), ACC_VEC(parsedParams));
    }
}

bool DialogueController::enterNode(const std::string& _nodeName, unsigned _lineIndex)
{
    auto node = getNodeByName(_nodeName);
    if (node)
    {
        //increment previous node so we start at the next line after we exit the new node
        if(m_nodeStack.empty() == false)
        {
            m_nodeStack.back().lineIndex++;
        }

        if(node->lines.empty() == false)
        {
            m_nodeStack.push_back( { node, _lineIndex} );

            //attempt to present the line
            if(present(*node, _lineIndex) == false)
            {
                //if presentiation fails then advance the line
                return advanceLine();
            }
            return true;
        }
        else
        {
            LOGERROR("Failed to push back node: No lines");
        }
    }
    else
    {
        LOGERROR("Failed to push back node: Invalid name '%s'", _nodeName.c_str());
    }
    return false;
}

bool DialogueController::exitNode()
{
    if (m_nodeStack.empty() == false)
    {
        m_nodeStack.pop_back();
        if(m_nodeStack.empty() == false)
        {
            auto& activeNode = m_nodeStack.back().node;
            auto& lineIndex = m_nodeStack.back().lineIndex;
            return present(*activeNode, lineIndex);
        }
        return true;
    }
    else
    {
        LOGERROR("Failed to pop node: stack is empty");
    }
    return false;
}

void DialogueController::onDialogueEnded()
{
    LOG("Dialogue ended");

    //ensure everything is cleaned up
    m_nodeStack.clear();
    m_presentedOptions.clear();
    m_isProgressing = false;
    m_isSkipping = false;
    m_isPaused = false;
    m_pendingStop = false;

    //notify delegate
    if(m_dialogueDelegate)
    {
        m_dialogueDelegate->onEnd();
    }
}
