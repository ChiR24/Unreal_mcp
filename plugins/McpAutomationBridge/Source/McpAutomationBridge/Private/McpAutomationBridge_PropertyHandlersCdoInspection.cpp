#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_PropertyHandlersCdoComponents.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"

#if WITH_EDITOR
#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleInspectCdoAction(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("inspect_cdo: payload missing"),
                            TEXT("INVALID_PAYLOAD"));
        return true;
    }

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

    FString NormalizedPath, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, NormalizedPath, LoadError);
    if (!Blueprint)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Blueprint not found: %s (%s)"),
                                            *BlueprintPath, *LoadError),
                            TEXT("BLUEPRINT_NOT_FOUND"));
        return true;
    }

    UClass* GeneratedClass = Blueprint->GeneratedClass;
    if (!GeneratedClass)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Blueprint has no GeneratedClass (not compiled?)"),
                            TEXT("CDO_NOT_FOUND"));
        return true;
    }

    UObject* CDO = GeneratedClass->GetDefaultObject();
    if (!CDO)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Failed to get Class Default Object"),
                            TEXT("CDO_NOT_FOUND"));
        return true;
    }

    FString ComponentNameFilter;
    Payload->TryGetStringField(TEXT("componentName"), ComponentNameFilter);
    ComponentNameFilter.TrimStartAndEndInline();
    bool bDetailed = false;
    Payload->TryGetBoolField(TEXT("detailed"), bDetailed);

    TArray<FName> PropertyNameFilter;
    FString PropertyPathFilter;
    if (Payload->TryGetStringField(TEXT("propertyPath"), PropertyPathFilter))
    {
        PropertyPathFilter.TrimStartAndEndInline();
        if (!PropertyPathFilter.IsEmpty())
        {
            PropertyNameFilter.Add(FName(*PropertyPathFilter));
        }
    }
    const TArray<TSharedPtr<FJsonValue>>* PropNamesArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("propertyNames"), PropNamesArr) && PropNamesArr)
    {
        for (const auto& Val : *PropNamesArr)
        {
            FString S;
            if (Val->TryGetString(S))
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
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Component not found: %s"), *ComponentNameFilter),
                                TEXT("COMPONENT_NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> CompJson;
        if (PropertyNameFilter.Num() > 0)
        {
            CompJson = McpPropertyReflection::ExportPropertiesToJson(FoundComp, PropertyNameFilter);
        }
        else
        {
            CompJson = McpPropertyReflection::ExportObjectToJson(FoundComp, false);
        }

        Resp->SetStringField(TEXT("componentName"), ComponentNameFilter);
        Resp->SetStringField(TEXT("templateObjectName"), FoundComp->GetName());
        Resp->SetStringField(TEXT("componentClass"), FoundComp->GetClass()->GetName());
        if (CompJson.IsValid())
        {
            Resp->SetObjectField(TEXT("properties"), CompJson);
        }
        Resp->SetBoolField(TEXT("success"), true);
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
            McpPropertyReflection::ExportObjectToJson(CDO, false);
        if (CdoProps.IsValid())
        {
            Resp->SetObjectField(TEXT("cdoProperties"), CdoProps);
        }
    }

    TArray<TSharedPtr<FJsonValue>> ComponentsArray;
    TSet<FString> SeenNames;
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
            const FString Source = ScsSourceMap.Contains(CompName)
                ? TEXT("Native_Override") : TEXT("Native");
            ComponentsArray.Add(MakeShared<FJsonValueObject>(
                McpPropertyCdoComponents::BuildComponentSummary(Comp, CompName, Source,
                                      bDetailed, PropertyNameFilter)));
        }
    }

    for (UBlueprint* Bp = Blueprint; Bp != nullptr;)
    {
        if (Bp->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Bp->SimpleConstructionScript->GetAllNodes())
            {
                if (!Node || !Node->ComponentTemplate) continue;
                const FString VarName = Node->GetVariableName().ToString();
                if (SeenNames.Contains(VarName)) continue;
                SeenNames.Add(VarName);
                const FString Source = (Bp == Blueprint) ? TEXT("SCS") : TEXT("SCS_Inherited");

                TSharedPtr<FJsonObject> CompObj = McpPropertyCdoComponents::BuildComponentSummary(
                    Node->ComponentTemplate, VarName, Source,
                    bDetailed, PropertyNameFilter);
                if (Node->ParentComponentOrVariableName != NAME_None)
                {
                    CompObj->SetStringField(TEXT("attachParent"),
                        Node->ParentComponentOrVariableName.ToString());
                }
                ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
            }
        }
        UClass* ParentClass = Bp->ParentClass;
        Bp = ParentClass ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy) : nullptr;
    }

    Resp->SetArrayField(TEXT("components"), ComponentsArray);
    Resp->SetNumberField(TEXT("componentCount"), ComponentsArray.Num());
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("CDO inspection completed"), Resp, FString());
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("inspect_cdo requires editor build."),
                        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
