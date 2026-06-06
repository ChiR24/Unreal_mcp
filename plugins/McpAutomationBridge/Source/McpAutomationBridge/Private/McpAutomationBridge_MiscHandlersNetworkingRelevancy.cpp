#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_MiscHandlersSupport.h"
#include "McpHandlerUtils.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"

#if WITH_EDITOR
namespace McpMiscHandlers
{
bool HandleConfigureNetCullDistance(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"), TEXT(""));
    double CullDistance = GetNumberField(Payload, TEXT("cullDistance"), 15000.0);

    if (BlueprintPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("blueprintPath is required"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }
    if (!Blueprint->GeneratedClass)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Blueprint has no generated class"), nullptr, TEXT("INVALID_BLUEPRINT"));
        return true;
    }

    AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
    if (CDO)
    {
        double CullDistanceSquared = CullDistance * CullDistance;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
        CDO->SetNetCullDistanceSquared(static_cast<float>(CullDistanceSquared));
#else
        CDO->NetCullDistanceSquared = static_cast<float>(CullDistanceSquared);
#endif
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    ResponseJson->SetNumberField(TEXT("cullDistance"), CullDistance);
    ResponseJson->SetNumberField(TEXT("cullDistanceSquared"), CullDistance * CullDistance);
    McpHandlerUtils::AddVerification(ResponseJson, Blueprint);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Net cull distance set to %.0f"), CullDistance), ResponseJson);
    return true;
}
}
#endif
