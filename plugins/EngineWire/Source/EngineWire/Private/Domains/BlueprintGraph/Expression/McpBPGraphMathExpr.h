#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UBlueprint;
class UEdGraphNode;

// A Math Expression node reaches a library function by the name its own operator table files it under, and reads a
// Blueprint variable wherever the expression names one. Both were invisible to a caller: FMax, FClamp and frac failed
// as a bare "does not parse", and an input named like a member variable (VZ) made no pin, so the next connect_pins
// failed PIN_NOT_FOUND. These say which, in the reply.
namespace McpBlueprintMathExpression
{
// The functions Expression calls that no Math Expression node can reach, each with the spelling it accepts
// ("No math function is called FMax (try Max), frac (try Fraction)."); empty when every call is known.
FString DescribeUnknownFunctions(const FString& Expression);

// The operators Expression uses in a way no Math Expression node takes: a unary ! or minus, and && or || on a name
// that is not a bool variable (every unknown name becomes a number input). ExtraBools are bool variables a batch
// declares before the expression's step runs (not in the class yet). Empty when none.
FString DescribeOperatorMisuse(const UBlueprint* Blueprint, const FString& Expression,
                               const TSet<FName>& ExtraBools = TSet<FName>());

// A true/false value used as a number: a bracketed comparison or && / ||, or a bool variable, as an operand of + - * /
// or %. The node turns no bool into a number, so the parse succeeds and only the Blueprint compile fails. Also a lone
// name that is no bool variable (a number input) compared by == or != with a true/false value, which does not parse.
// Empty when none.
FString DescribeBoolArithmetic(const UBlueprint* Blueprint, const FString& Expression,
                               const TSet<FName>& ExtraBools = TSet<FName>());

// Each operand of && or || (Parts as TokenizeExpression gives them) that is a number: a name that is no bool variable,
// a number, or arithmetic with no comparison on its own bracket level. An operand holding a call or a comparison is
// not judged. One note per operand; empty when none.
TArray<FString> DescribeNumberLogic(const UBlueprint* Blueprint, const TArray<FString>& Parts,
                                    const TSet<FName>& ExtraBools);

// Whether Name reads true/false: a bool variable of the Blueprint, one of ExtraBools, or the literal true or false.
bool IsBoolName(const UBlueprint* Blueprint, const FString& Name, const TSet<FName>& ExtraBools);

// All of the above in one text; empty when Expression shows none of those problems.
FString DescribeProblems(const UBlueprint* Blueprint, const FString& Expression,
                         const TSet<FName>& ExtraBools = TSet<FName>());

// Expression as tokens: names, numbers, two-character operators (&& || == != <= >=) and single characters.
TArray<FString> TokenizeExpression(const FString& Expression);

// Expression with each unary minus written as a subtraction from zero, which the node parses and means the same:
// -150 -> (0 - 150), a * -b -> a * (0 - b), -max(a, b) -> (0 - max(a, b)). A minus with no operand is left as is.
FString RewriteUnaryMinus(const FString& Expression);

// After a parse: inputPins, the pins the node made, and boundToMembers, the names it reads from the Blueprint's
// own variables instead (no pin to wire).
void DescribeInputs(const UBlueprint* Blueprint, const UEdGraphNode& Node, const FString& Expression,
                    const TSharedPtr<FJsonObject>& Result);

// After Copy was duplicated from Source: rewrites each Math Expression's member references from Source's variable
// GUIDs to their names and rebuilds the node, so it binds the copy's variables. Returns how many nodes it rewrote.
int32 RebindDuplicatedExpressions(UBlueprint& Source, UBlueprint& Copy);
}
