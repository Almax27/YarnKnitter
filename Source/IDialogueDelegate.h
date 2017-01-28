#pragma once

struct DialogueContent;

class IDialogueDelegate
{
public:
    virtual ~IDialogueDelegate() {};

    virtual void onProgress(const DialogueContent& _content) = 0;
    virtual void onEnd() = 0;
    virtual void onPaused() = 0;
};
