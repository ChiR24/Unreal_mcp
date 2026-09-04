#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "Dom/JsonValue.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersPropertyApply.h"
#include "UObject/UnrealType.h"

namespace McpAIHandlers
{
// Applies each {name: value} pair whose name is a property of Type onto
// Container through the shared JSON importer (scalars, enums by name, structs
// and arrays). Names Type does not declare are left untouched so the caller
// can offer them to its next target; failed imports are reported as
// "name: reason". Returns the number of properties written.
int32 ApplyAIJsonProperties(const UStruct* Type, void* Container, const TSharedPtr<FJsonObject>& Properties, TArray<FString>& OutApplied, TArray<FString>& OutFailed)
{
    if (!Type || !Container || !Properties.IsValid())
    {
        return 0;
    }

    int32 Applied = 0;
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Properties->Values)
    {
        FProperty* Property = Type->FindPropertyByName(FName(*Pair.Key));
        if (!Property || !Pair.Value.IsValid())
        {
            continue;
        }

        FString Error;
        if (ApplyJsonValueToProperty(Container, Property, Pair.Value, Error))
        {
            OutApplied.AddUnique(Pair.Key);
            ++Applied;
        }
        else
        {
            OutFailed.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *Error));
        }
    }
    return Applied;
}

void ListAIPropertyNames(const UStruct* Type, TArray<FString>& OutNames)
{
    for (TFieldIterator<FProperty> It(Type); It; ++It)
    {
        OutNames.AddUnique(It->GetName());
    }
}
}
#endif
