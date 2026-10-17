#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/ObjectKey.h"

struct FFrame;
struct FBlueprintExceptionInfo;

// Blueprint runtime errors raised while a game plays in the editor (an Accessed None, an infinite loop), each read
// back to the node, graph and Blueprint that raised it. The log names only the script function and a byte offset,
// and from UE 5.8 only once per object, so a frame step or motion read could not say which node to fix or how often.
// The list holds the current play session, or the last one until the next play starts.
class FMcpScriptErrors
{
public:
    static FMcpScriptErrors& Get();

    void Watch(bool bOn);

    // One line per error raised since a reading of FMcpLogHistory::Now(), with how often it fired in that window.
    // OutMessages gets each error's engine text, so the LogScript lines repeating it can be left out.
    TArray<FString> LinesSince(double SinceSeconds, TArray<FString>& OutMessages) const;

    // The session's errors, for runtime_report: node, graph, Blueprint, the objects that raised it, how often, and
    // the object's Blueprint variables at the first hit.
    TArray<TSharedPtr<FJsonValue>> ToJson() const;

private:
    struct FError
    {
        FString Message;
        FString BlueprintPath;
        FString Graph;
        FString Node;
        FString NodeId;
        FString Function;
        TArray<FString> Objects;
        double GameSeconds = 0.0;
        int32 Count = 0;
        TArray<double> Times;
        TSharedPtr<FJsonObject> Values;
    };

    void OnScriptException(const UObject* ActiveObject, const FFrame& StackFrame, const FBlueprintExceptionInfo& Info);
    void Record(FError& Error, const UObject* ActiveObject);

    TArray<FError> Errors;
    TMap<TPair<FObjectKey, int32>, int32> ByCodeLocation;
    FDelegateHandle ExceptionHandle;
    FDelegateHandle BeginPieHandle;
};
