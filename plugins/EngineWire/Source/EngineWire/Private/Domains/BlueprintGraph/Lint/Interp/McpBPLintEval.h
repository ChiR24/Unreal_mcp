#pragma once

#include "CoreMinimal.h"
#include "Domains/BlueprintGraph/Lint/Interp/McpBPLintValue.h"

class UEdGraphNode;
class UEdGraphPin;
class UK2Node_CallFunction;
struct FEdGraphPinType;

// What a path knows where an exec node runs (McpBPLintState.cpp), and how pure inputs evaluate there (McpBPLintEval.cpp,
// McpBPLintPins.cpp), for the abstract interpreter (McpBPLintInterp.cpp) and the numeric lint rules.
namespace McpBPLint
{
/** How deep a chain of pure nodes is followed. */
constexpr int32 MaxEvalDepth = 64;

/** Numbers and Booleans of member and local variables (and loop indexes), and the lengths of array variables; a
 * variable not listed is unknown. bLive is false where no path has arrived yet. */
struct FState
{
    TMap<FName, FRange> Numbers;
    TMap<FName, FRange> Lengths;
    /** (A, B) -> what A - B can be on this path, from a check that compared the two (Health >= Damage, A != B); kept
     * both ways round. */
    TMap<TPair<FName, FName>, FRange> Differences;
    bool bLive = false;
};

bool SameState(const FState& A, const FState& B);
FState JoinStates(const FState& A, const FState& B);
FState WidenStates(const FState& Old, const FState& New);
/** Array's length changed: bounds that leaned on it no longer hold. */
void ForgetLength(FState& State, FName Array);
/** Variable changed: what was known of it against other variables no longer holds. */
void ForgetDifferences(FState& State, FName Variable);

/** The array an input reads: its variable (NAME_None for another array) and its length there. */
struct FArrayRead
{
    FName Variable;
    FRange Length;
};

/** Evaluates pure inputs where an exec node runs in State. */
struct FEval
{
    const FState& State;

    FRange Number(const UEdGraphPin* Input, int32 Depth = 0) const;
    FArrayRead Array(const UEdGraphPin* Input) const;
    /** Narrows Out (a copy of State) to the paths where Condition is bTrue; false when none is left. */
    bool Assume(FState& Out, const UEdGraphPin* Condition, bool bTrue, int32 Depth = 0) const;
};

/** The member or local variable an input reads straight from a getter, else NAME_None. */
FName ReadVariable(const UEdGraphPin* Input);
/** The variable or loop index a pin's value comes from, as the state keys it, else NAME_None. */
FName ValueKey(const UEdGraphPin* Input);
bool IsIntegerPin(const UEdGraphPin* Pin);
/** Whether a type is a number or a Boolean the interpreter follows (an enum is not). */
bool IsNumberType(const FEdGraphPinType& Type);
bool IsNumberPin(const UEdGraphPin* Pin);
FName LoopIndexKey(const UEdGraphNode* Loop);
/** The standard macro Node instances (ForLoop, WhileLoop, ...), else NAME_None. */
FName MacroName(const UEdGraphNode* Node);
/** A math call's operation and whether it works on integers: Divide_IntInt is ("Divide", true); else false. */
bool ScalarOp(const UK2Node_CallFunction* Call, FString& OutOp, bool& bOutInteger);
/** The comparison a math operation names (Less, EqualEqual, ...); false for any other. */
bool CompareOp(const FString& Op, ECompare& OutTest);
/** The data inputs of a node, its self pin and hidden pins left out, in pin order. */
TArray<const UEdGraphPin*> DataInputs(const UEdGraphNode* Node);
FName FunctionName(const UK2Node_CallFunction* Call);
} // namespace McpBPLint
