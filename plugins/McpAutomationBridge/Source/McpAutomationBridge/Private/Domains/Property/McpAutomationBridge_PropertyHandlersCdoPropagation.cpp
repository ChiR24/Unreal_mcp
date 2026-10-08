#include "Domains/Property/McpAutomationBridge_PropertyHandlersCdoPropagation.h"
#include "Domains/Property/McpAutomationBridge_PropertyHandlersTarget.h"

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

// Every archetype from Instance up to Template still holds TemplateText: a derived Blueprint's default that overrode
// the value keeps it, and so do the copies that follow that derived default.
bool FollowsTemplate(UObject* Instance, UObject* Template, const FString& Path, const FString& TemplateText)
{
    for (UObject* Each = Instance; Each && Each != Template; Each = Each->GetArchetype())
    {
        FString Text;
        if (!ExportFollowerText(Each, Path, Text) || Text != TemplateText)
        {
            return false;
        }
    }
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
        // Copies placed in an editor level and a derived Blueprint's own defaults, which the details panel updates too
        // (a child Blueprint kept the old value, and so did its placed copies). The Content Browser thumbnail, PIE
        // copies and what a compile left behind are no one's work.
        const UWorld* World = IsValid(Instance) ? Instance->GetWorld() : nullptr;
        const bool bPlaced = World && World->WorldType == EWorldType::Editor;
        if ((bPlaced || (IsValid(Instance) && Instance->IsTemplate() && !McpPropertyTarget::IsSupersededTarget(Instance)))
            && FollowsTemplate(Instance, Template, Path, TemplateText))
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
        if (McpPropertyTarget::WriteValue(Instance, Path, Property, Container, Value, Error))
        {
            Instance->PostEditChange();
            McpRefreshComponentAfterEdit(Cast<UActorComponent>(Instance));
            ++Applied;
        }
    }
    return Applied;
}
}
