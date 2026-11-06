#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

class AActor;
class FMcpBridgeWebSocket;
class FProperty;
class UEngineWireSubsystem;
class UFunction;
class UWorld;

// control_editor fuzz_blueprint (plugin Domains/Play): a Blueprint actor of a Play session this server started is called
// through its own functions with edge-case inputs, one input at a time, and every Blueprint error a call raises comes back
// with the node it came from.
namespace McpFuzz
{
// One value a parameter is called with: what the reply shows, and what is written.
struct FValue
{
    TSharedPtr<FJsonValue> Shown;
    double Number = 0.0;
    bool bOn = false;
    FString Text;
    TWeakObjectPtr<UObject> Object;
    FVector Vector = FVector::ZeroVector;
    int32 Count = 0; // an array's length; its elements take the element type's first value
};

// One function to call: its inputs and the values each is tried with.
struct FPlan
{
    UFunction* Function = nullptr;
    TArray<const FProperty*> Inputs;
    TArray<TArray<FValue>> Values;
};

// The values a parameter is tried with. The first is the one every call keeps while another input is varied (1, false,
// "A", an object of the parameter's class from the running game); the rest are tried one at a time (0, -1, the int
// limits, +-1e9, empty text, None, an empty array). A type with no edge cases has its first value only.
TArray<FValue> ValuesFor(const FProperty* Property, UWorld* World, AActor* Target);
void Write(const FProperty* Property, void* Where, const FValue& Value, UWorld* World, AActor* Target);

// Calls every plan's function on Target in turn, at most MaxCalls calls and 20 s in all: the result holds the calls made,
// the failures, a row per function and what ended the run early.
TSharedPtr<FJsonObject> CallAll(const TArray<FPlan>& Plans, UWorld* World, AActor* Target, int32 MaxCalls);

bool Handle(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket);
} // namespace McpFuzz
