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
// that is not a bool variable (every unknown name becomes a number input). Empty when none.
FString DescribeOperatorMisuse(const UBlueprint* Blueprint, const FString& Expression);

// After a parse: inputPins, the pins the node made, and boundToMembers, the names it reads from the Blueprint's
// own variables instead (no pin to wire).
void DescribeInputs(const UBlueprint* Blueprint, const UEdGraphNode& Node, const FString& Expression,
                    const TSharedPtr<FJsonObject>& Result);
}
