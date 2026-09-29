#include "Domains/Navigation/McpAutomationBridge_NavigationHandlersPrivate.h"

#include "Safety/McpSafeOperations.h"

namespace McpNavigationHandlers
{
bool HandleConfigureNavAreaCost(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AreaClassPath = GetJsonStringField(Payload, TEXT("areaClass"));
    double AreaCost = GetJsonNumberField(Payload, TEXT("areaCost"), 1.0);
    if (AreaClassPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("areaClass is required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }
    if (!IsValidNavigationPath(AreaClassPath))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid areaClass: must not contain path traversal (..), slashes, or drive letters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    UClass* AreaClass = LoadClass<UNavArea>(nullptr, *AreaClassPath);
    if (!AreaClass)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("NavArea class not found: %s"), *AreaClassPath), nullptr, TEXT("INVALID_CLASS"));
        return true;
    }

    UNavArea* AreaCDO = AreaClass->GetDefaultObject<UNavArea>();
    if (!AreaCDO)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Could not get NavArea CDO"), nullptr, TEXT("CDO_FAILED"));
        return true;
    }

    // Write only what was passed: an omitted areaCost used to reset the cost to 1.
    const bool bHasCost = Payload->HasField(TEXT("areaCost"));
    const bool bHasEnteringCost = Payload->HasField(TEXT("fixedAreaEnteringCost"));
    if (!bHasCost && !bHasEnteringCost)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Pass areaCost, fixedAreaEnteringCost or both"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }
    // FixedAreaEnteringCost is protected; it is an editable UPROPERTY, so write it by reflection.
    // Found before anything is written, so a refusal leaves DefaultCost as it was.
    FFloatProperty* EnteringProp = bHasEnteringCost ? FindFProperty<FFloatProperty>(AreaClass, TEXT("FixedAreaEnteringCost")) : nullptr;
    if (bHasEnteringCost && !EnteringProp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("FixedAreaEnteringCost property not found on this NavArea class"), nullptr, TEXT("PROPERTY_NOT_FOUND"));
        return true;
    }
    AreaCDO->Modify();
    if (bHasCost)
    {
        AreaCDO->DefaultCost = AreaCost;
    }
    if (EnteringProp)
    {
        EnteringProp->SetPropertyValue_InContainer(AreaCDO, static_cast<float>(GetJsonNumberField(Payload, TEXT("fixedAreaEnteringCost"), 0.0)));
    }

    // A Blueprint NavArea keeps its defaults in its own package, so save it. A native class
    // (/Script/...) has no asset to save: the change lasts for this editor session only.
    UBlueprint* AreaBlueprint = UBlueprint::GetBlueprintFromClass(AreaClass);
    bool bPersisted = false;
    if (AreaBlueprint)
    {
        AreaBlueprint->MarkPackageDirty();
        bPersisted = McpSafeAssetSave(AreaBlueprint);
        if (!bPersisted)
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("The cost was applied in memory but saving %s failed; retry or save the asset manually"), *AreaBlueprint->GetPathName()), nullptr, TEXT("SAVE_FAILED"));
            return true;
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("areaClass"), AreaClassPath);
    Result->SetNumberField(TEXT("areaCost"), AreaCDO->DefaultCost);
    Result->SetNumberField(TEXT("fixedAreaEnteringCost"), AreaCDO->GetFixedAreaEnteringCost());
    Result->SetBoolField(TEXT("persisted"), bPersisted);

    const TCHAR* Message = bPersisted
        ? TEXT("Nav area cost configured and saved")
        : TEXT("Nav area cost configured for this editor session only: a native NavArea class cannot be saved. Use a Blueprint subclass of NavArea to keep the cost.");
    Self->SendAutomationResponse(Socket, RequestId, true, Message, Result);
    return true;
}
}
