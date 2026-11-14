#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Foundation/BridgeHelpers/Blueprints/McpPropCdoComponents.h"
#include "Domains/Property/McpPropTarget.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"

#include "Algo/AnyOf.h"
#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"

namespace
{
// The names of Filter no class of Classes has: missingProperties, as get_component_details and get_property list them.
void McpAddMissingProperties(const TSharedPtr<FJsonObject>& Resp, const TArray<FName>& Filter, const TSet<const UClass*>& Classes)
{
    TArray<FString> Missing;
    for (const FName& Name : Filter)
    {
        if (!Algo::AnyOf(Classes, [&Name](const UClass* Class) { return FindFProperty<FProperty>(Class, Name) != nullptr; }))
        {
            Missing.Add(Name.ToString());
        }
    }
    if (Missing.Num() > 0)
    {
        Resp->SetArrayField(TEXT("missingProperties"), McpHandlerUtils::ToJsonStringArray(Missing));
    }
}
} // namespace

bool UEngineWireSubsystem::HandleInspectCdoAction(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
    BlueprintPath.TrimStartAndEndInline();
    if (BlueprintPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("blueprintPath is required for inspect_cdo"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString NormalizedPath;
    UBlueprint* Blueprint = nullptr;
    UObject* CDO = McpPropertyTarget::LoadBlueprintDefaultsOrReply(*this, RequestId, RequestingSocket, BlueprintPath, Blueprint, NormalizedPath);
    if (!CDO)
    {
        return true;
    }
    UClass* GeneratedClass = Blueprint->GeneratedClass;

    FString ComponentNameFilter;
    Payload->TryGetStringField(TEXT("componentName"), ComponentNameFilter);
    ComponentNameFilter.TrimStartAndEndInline();
    bool bDetailed = false;
    Payload->TryGetBoolField(TEXT("detailed"), bDetailed);

    TArray<FName> PropertyNameFilter;
    const TArray<TSharedPtr<FJsonValue>>* PropNamesArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("propertyNames"), PropNamesArr) && PropNamesArr)
    {
        for (const auto& Val : *PropNamesArr)
        {
            FString S;
            if (McpHandlerUtils::TryGetJsonValueString(Val, S))
            {
                PropertyNameFilter.Add(FName(*S));
            }
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("blueprintPath"), NormalizedPath);
    Resp->SetStringField(TEXT("className"), GeneratedClass->GetName());
    Resp->SetStringField(TEXT("classPath"), GeneratedClass->GetPathName());
    Resp->SetStringField(TEXT("parentClass"),
        GeneratedClass->GetSuperClass()
            ? GeneratedClass->GetSuperClass()->GetName()
            : TEXT("None"));

    if (!ComponentNameFilter.IsEmpty())
    {
        UActorComponent* FoundComp = McpPropertyCdoComponents::FindCdoComponent(Blueprint, CDO, ComponentNameFilter, false);
        if (!FoundComp)
        {
            // Name the candidates so the caller can correct in one round trip
            // instead of guessing (native components are often reachable under
            // both an object name and a UPROPERTY alias).
            const TArray<FString> Available =
                McpPropertyCdoComponents::CollectResolvableComponentNames(Blueprint, CDO);
            TArray<TSharedPtr<FJsonValue>> AvailableJson = McpHandlerUtils::ToJsonStringArray(Available);

            TSharedPtr<FJsonObject> ErrData = McpHandlerUtils::CreateResultObject();
            ErrData->SetArrayField(TEXT("availableComponents"), AvailableJson);

            SendAutomationResponse(
                RequestingSocket, RequestId, false,
                FString::Printf(TEXT("Component not found: %s. Available components: %s"),
                                *ComponentNameFilter,
                                Available.Num() > 0 ? *FString::Join(Available, TEXT(", "))
                                                    : TEXT("<none>")),
                ErrData, TEXT("COMPONENT_NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> CompJson;
        if (PropertyNameFilter.Num() > 0)
        {
            CompJson = McpPropertyReflection::ExportPropertiesToJson(FoundComp, PropertyNameFilter);
        }
        else
        {
            CompJson = McpPropertyReflection::ExportObjectToJsonBounded(FoundComp, false);
        }

        Resp->SetStringField(TEXT("componentName"), ComponentNameFilter);
        Resp->SetStringField(TEXT("templateObjectName"), FoundComp->GetName());
        Resp->SetStringField(TEXT("componentClass"), FoundComp->GetClass()->GetName());
        if (CompJson.IsValid())
        {
            Resp->SetObjectField(TEXT("properties"), CompJson);
        }
        McpAddMissingProperties(Resp, PropertyNameFilter, {FoundComp->GetClass()});
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("CDO component inspected"), Resp, FString());
        return true;
    }

    if (PropertyNameFilter.Num() > 0)
    {
        TSharedPtr<FJsonObject> CdoProps =
            McpPropertyReflection::ExportPropertiesToJson(CDO, PropertyNameFilter);
        if (CdoProps.IsValid())
        {
            Resp->SetObjectField(TEXT("cdoProperties"), CdoProps);
        }
    }
    else if (bDetailed)
    {
        TSharedPtr<FJsonObject> CdoProps =
            McpPropertyReflection::ExportObjectToJsonBounded(CDO, false);
        if (CdoProps.IsValid())
        {
            Resp->SetObjectField(TEXT("cdoProperties"), CdoProps);
        }
    }

    TArray<TSharedPtr<FJsonValue>> ComponentsArray;
    TSet<FString> SeenNames;
    TSet<const UClass*> ListedClasses = {CDO->GetClass()};
    TMap<FString, FString> ScsSourceMap = McpPropertyCdoComponents::BuildScsSourceMap(Blueprint);

    if (AActor* DefaultActor = Cast<AActor>(CDO))
    {
        TInlineComponentArray<UActorComponent*> CdoComponents;
        DefaultActor->GetComponents(CdoComponents);
        for (UActorComponent* Comp : CdoComponents)
        {
            if (!Comp) continue;
            const FString CompName = Comp->GetName();
            SeenNames.Add(CompName);
            ListedClasses.Add(Comp->GetClass());
            const FString Source = ScsSourceMap.Contains(CompName)
                ? TEXT("Native_Override") : TEXT("Native");
            ComponentsArray.Add(MakeShared<FJsonValueObject>(
                McpPropertyCdoComponents::BuildComponentSummary(Comp, CompName, Source,
                                      bDetailed, PropertyNameFilter)));
        }
    }

    McpPropertyCdoComponents::ForEachScsNode(Blueprint, [&](USCS_Node* Node, bool bInherited)
    {
        const FString VarName = Node->GetVariableName().ToString();
        if (!Node->ComponentTemplate || SeenNames.Contains(VarName))
        {
            return true;
        }
        SeenNames.Add(VarName);
        ListedClasses.Add(Node->ComponentTemplate->GetClass());
        bool bOverridden = false;
        TSharedPtr<FJsonObject> CompObj = McpPropertyCdoComponents::BuildComponentSummary(
            McpPropertyCdoComponents::EffectiveTemplate(Blueprint, Node, &bOverridden), VarName,
            bInherited ? TEXT("SCS_Inherited") : TEXT("SCS"), bDetailed, PropertyNameFilter);
        if (bOverridden)
        {
            CompObj->SetBoolField(TEXT("overridden"), true);
        }
        if (Node->ParentComponentOrVariableName != NAME_None)
        {
            CompObj->SetStringField(TEXT("attachParent"), Node->ParentComponentOrVariableName.ToString());
        }
        ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
        return true;
    });

    McpHandlerUtils::FilterRowsByListedNames(Payload, TEXT("componentNames"), ComponentsArray, Resp,
                                             TEXT("missingComponents"));
    McpAddMissingProperties(Resp, PropertyNameFilter, ListedClasses);
    Resp->SetArrayField(TEXT("components"), ComponentsArray);
    Resp->SetNumberField(TEXT("componentCount"), ComponentsArray.Num());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("CDO inspection completed"), Resp, FString());
    return true;
}
