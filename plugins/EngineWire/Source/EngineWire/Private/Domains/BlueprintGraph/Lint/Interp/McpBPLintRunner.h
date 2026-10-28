#pragma once

#include "CoreMinimal.h"
#include "Domains/BlueprintGraph/Lint/Interp/McpBPLintInterp.h"

class UEdGraphPin;

// The fixpoint over one entry's exec flow (McpBPLintInterp.cpp runs it, McpBPLintStep.cpp steps its nodes). Blueprint
// runs a chain of nodes until one fires an output with nothing linked; it then goes back to the Sequence or loop the
// chain ran in. The runner keeps that as a stack of frames, so a node reached in a Sequence's first output and the same
// node reached in a loop's body are kept apart.
namespace McpBPLint
{
struct FRunFrame
{
    const UEdGraphNode* Node = nullptr;
    int32 Next = INDEX_NONE; // a Sequence: the output that runs next; INDEX_NONE: a loop its body comes back to

    bool operator==(const FRunFrame& Other) const { return Node == Other.Node && Next == Other.Next; }
};

enum class ERunStep : uint8
{
    Run,    // the node runs
    Resume, // a Sequence goes on at output Output
    Exit,   // a loop's Completed runs
};

struct FRunKey
{
    const UEdGraphNode* Node = nullptr;
    int32 Stack = 0;
    ERunStep Step = ERunStep::Run;
    int32 Output = 0;

    bool operator==(const FRunKey& Other) const
    {
        return Node == Other.Node && Stack == Other.Stack && Step == Other.Step && Output == Other.Output;
    }
    friend uint32 GetTypeHash(const FRunKey& Key)
    {
        return HashCombine(HashCombine(PointerHash(Key.Node), ::GetTypeHash(Key.Stack)), ::GetTypeHash(static_cast<int32>(Key.Step) * 65536 + Key.Output));
    }
};

struct FRunVertex
{
    FRunKey Key;
    FState In;
    int32 Updates = 0;
    bool bQueued = false;
};

class FRunner
{
public:
    explicit FRunner(const FMembers& InMembers) : Members(InMembers) {}

    /** False when the budget ran out: nothing of this entry is kept. */
    bool Run(const UEdGraphNode* Entry, const FState& Start, FAnalysis& Out);

private:
    const FMembers& Members;
    TArray<TArray<FRunFrame>> Stacks = {TArray<FRunFrame>()};
    TArray<FRunVertex> Vertices;
    TMap<FRunKey, int32> Index;
    TArray<int32> Work;
    TMap<const UEdGraphNode*, FState> Returns;
    int32 Visits = 0;
    bool bOver = false;

    int32 Intern(TArray<FRunFrame>&& Frames);
    int32 Push(int32 Stack, const FRunFrame& Frame);
    void Reach(const FRunKey& Key, const FState& State);
    void Return(int32 Stack, const FState& State);
    void Follow(const UEdGraphPin* Exit, const FState& State, int32 Stack, int32 Hops = 0);
    void Break(const UEdGraphNode* Loop, int32 Stack, const FState& State);
    /** McpBPLintStep.cpp */
    void Step(int32 At);
    void Resume(const UEdGraphNode* Sequence, int32 Stack, int32 From, const FState& State);
    void Loop(const UEdGraphNode* Node, int32 Stack, const FState& State);
};
} // namespace McpBPLint
