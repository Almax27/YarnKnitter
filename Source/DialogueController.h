#pragma once

#include <string>
#include <map>
#include <functional>

struct DialogueTreeConfig;
struct DialogueNode;
class IDialogueResolver;
class IDialogueDelegate;

class DialogueController
{
public:
    struct NodeState
    {
        DialogueNode* node;
        size_t lineIndex;
    };
    typedef std::vector<NodeState> NodeStack;
public:
  	virtual ~DialogueController();
    /*! ctor
     @param _dialogueResolver resolver used to handle actions, variables, etc
     @param _seed seed used for random content */
	  DialogueController(IDialogueDelegate* _dialogueDelegate = nullptr,
                       const IDialogueResolver* _dialogueResolver = nullptr);

    //-------------------------------------------
    //Dialogue Configuration

    /*! Add a node with given title, tags and body. Body is parsed into lines.
     @return true if the body was successfully parsed and name is unique */
    bool addNode(const std::string& _name, const std::string& _tags, const std::string& _body, unsigned _seed);

    /*! Add Dialogue Node
     @return true if name is unique */
	  bool addNode(const DialogueNode& _node);

    /*! Remove node by name
     @return true if the node was successfully removed*/
    bool removeNode(const std::string& name);

    /*! Remove all added nodes. Calling this during active dialogue may cause the dialogue to stop */
	  void clearNodes();

    /*! Retrieve node by name (title)
     @return a pointer to the node or nullptr if not found*/
    DialogueNode* getNodeByName(const std::string& _name) const;

    /*! Retrieve list of unique actors reference by all added nodes */
    void getActors(std::vector<std::string>& out_actorKeys) const;

    //-------------------------------------------
    //Dialogue Control

    /*! Begin dialogue from the given node.
     @param _startNode the name of the node to start from
     @param _lineIndex the line to start from
     @param _forceStart if true the dialogue will be stopped if running
     @return true if Dialogue successfully started*/
    bool start(const std::string& _startNode = "", unsigned lineIndex = 0, bool _forceStart = true);

    /*! Begin dialogue from the top of the given node stack
     @param _nodestack the stack to initial state with
     @param _forceStart if true the dialogue will be stopped if running
     @return true if Dialogue successfully started*/
    bool start(const NodeStack& _nodeStack, bool _forceStart = true);

    /*! Stop dialogue - clearing the state
     @return true if the dialogue was running and successfully stopped*/
    bool stop();

    /*! Pause/Unpause the dialogue
     If paused the the delegate's onPause() will be called
     If unpaused then the delegate's onProgress() will be called with the next line*/
    void setIsPaused(bool _isPaused);

    /*! Get the current paused state
     @return true if the dialogue is current paused*/
    bool getIsPaused() const;

    /*! Select from options previously provided via onProgressed event
     @param _index the option index to select
     @return true if _index was valid and option was successfully selected*/
    bool selectOption(size_t _index);

    /*! Progress Dialogue to the next line
     @return true if progression was successful. False if there are options to select (call selectOption)*/
    bool progressDialogue();

    /*! Skip dialogue until next option selection or end of dialogue */
    void skipDialogue();

    //-------------------------------------------
    //Accessors
    void setDialogueResolver(const IDialogueResolver* _resolver);
    const IDialogueResolver* getDialogueResolver() const;

    void setDialogueDelegate(IDialogueDelegate* _delegate);
    IDialogueDelegate* getDialogueDelegate() const;

    const NodeState* getCurrentNodeState() const;
    const NodeStack* getNodeStack() const;

protected:

    //-------------------------------------------
    //Dialogue Configuration
    IDialogueDelegate* m_dialogueDelegate;
    const IDialogueResolver* m_dialogueResolver;
    std::map<std::string, DialogueNode*> m_nodes;

    //-------------------------------------------
    //Dialogue State
    NodeStack m_nodeStack;
    struct Option
    {
        std::string nextNode;
        std::function<void(void)> resolveActions;
    };
    std::vector<Option> m_presentedOptions;
	  bool m_isSkipping;
    bool m_isProgressing;
    bool m_isPaused;
    bool m_pendingStop;

    //-------------------------------------------
    //Internal Helpers
    bool run();
    bool advanceLine();
    bool present(const DialogueNode& _node, size_t _index);
    void resolveAction(const std::string& _actionName, const std::vector<std::string>& params);
    bool enterNode(const std::string& _nodeName, unsigned _lineIndex = 0);
    bool exitNode();
    void onDialogueEnded();
};
