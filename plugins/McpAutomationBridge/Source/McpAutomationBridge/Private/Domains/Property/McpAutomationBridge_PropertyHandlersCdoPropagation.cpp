#include "Domains/Property/McpAutomationBridge_PropertyHandlersCdoPropagation.h"

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersComponentLookup.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersNestedPropertyPath.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersPropertyApply.h"
#include "UObject/UnrealType.h"

namespace McpPropertyCdoPropagation
{
namespace
{
FProperty* ResolveFollowerProperty(UObject* Object, const FString& Path, void*& OutContainer)
{
    FString ResolvedPath, Error;
    FProperty* Property = IsValid(Object) ? McpResolvePropertyPath(Object, Path, OutContainer, ResolvedPath, Error) : nullptr;
    return OutContainer ? Property : nullptr;
}

bool ExportFollowerText(UObject* Object, const FString& Path, FString& OutText)
{
    void* Container = nullptr;
    FProperty* Property = ResolveFollowerProperty(Object, Path, Container);
    if (!Property)
    {
        return false;
    }
    MCP_PROPERTY_EXPORT_TEXT(Property, OutText, Property->ContainerPtrToValuePtr<void>(Container), nullptr, nullptr, PPF_None);
    return true;
}
}

TArray<UObject*> CollectFollowers(UObject* Template, const FString& Path)
{
    TArray<UObject*> Followers;
    FString TemplateText;
    if (!Template || !Template->IsTemplate() || !ExportFollowerText(Template, Path, TemplateText))
    {
        return Followers;
    }
    TArray<UObject*> Instances;
    Template->GetArchetypeInstances(Instances);
    for (UObject* Instance : Instances)
    {
        // Direct copies placed in an editor level only: a derived class's copies follow that class's own default,
        // and the Content Browser thumbnail and PIE copies are no one's placed work.
        const UWorld* World = IsValid(Instance) ? Instance->GetWorld() : nullptr;
        if (!World || World->WorldType != EWorldType::Editor || Instance->GetArchetype() != Template)
        {
            continue;
        }
        FString Text;
        if (ExportFollowerText(Instance, Path, Text) && Text == TemplateText)
        {
            Followers.Add(Instance);
        }
    }
    return Followers;
}

int32 ApplyToFollowers(const TArray<UObject*>& Followers, const FString& Path, const TSharedPtr<FJsonValue>& Value)
{
    int32 Applied = 0;
    for (UObject* Instance : Followers)
    {
        void* Container = nullptr;
        FProperty* Property = ResolveFollowerProperty(Instance, Path, Container);
        FString Error;
        if (!Property)
        {
            continue;
        }
        Instance->Modify();
        if (ApplyJsonValueToProperty(Container, Property, Value, Error))
        {
            Instance->PostEditChange();
            McpRefreshComponentAfterEdit(Cast<UActorComponent>(Instance));
            ++Applied;
        }
    }
    return Applied;
}
}
