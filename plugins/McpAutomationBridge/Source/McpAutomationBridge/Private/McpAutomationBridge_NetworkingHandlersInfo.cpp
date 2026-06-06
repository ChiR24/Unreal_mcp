#include "McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
static void AddActorNetworkingInfo(TSharedPtr<FJsonObject>& NetworkingInfo, AActor* Actor)
{
    NetworkingInfo->SetBoolField(TEXT("bReplicates"), Actor->GetIsReplicated());
    NetworkingInfo->SetBoolField(TEXT("bAlwaysRelevant"), Actor->bAlwaysRelevant);
    NetworkingInfo->SetBoolField(TEXT("bOnlyRelevantToOwner"), Actor->bOnlyRelevantToOwner);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
    NetworkingInfo->SetNumberField(TEXT("netUpdateFrequency"), Actor->GetNetUpdateFrequency());
    NetworkingInfo->SetNumberField(TEXT("minNetUpdateFrequency"), Actor->GetMinNetUpdateFrequency());
    NetworkingInfo->SetNumberField(TEXT("netCullDistanceSquared"), Actor->GetNetCullDistanceSquared());
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    NetworkingInfo->SetNumberField(TEXT("netUpdateFrequency"), Actor->NetUpdateFrequency);
    NetworkingInfo->SetNumberField(TEXT("minNetUpdateFrequency"), Actor->MinNetUpdateFrequency);
    NetworkingInfo->SetNumberField(TEXT("netCullDistanceSquared"), Actor->NetCullDistanceSquared);
#else
    NetworkingInfo->SetNumberField(TEXT("netUpdateFrequency"), 0.0);
    NetworkingInfo->SetNumberField(TEXT("minNetUpdateFrequency"), 0.0);
    NetworkingInfo->SetNumberField(TEXT("netCullDistanceSquared"), 0.0);
#endif
    NetworkingInfo->SetNumberField(TEXT("netPriority"), Actor->NetPriority);
    NetworkingInfo->SetStringField(TEXT("netDormancy"), NetDormancyToString(Actor->NetDormancy));
}

bool HandleGetNetworkingInfo(FNetworkingActionContext& Context)
{
    FString BlueprintPath = GetStringField(Context.Payload, TEXT("blueprintPath"));
    FString ActorName = GetStringField(Context.Payload, TEXT("actorName"));
    TSharedPtr<FJsonObject> NetworkingInfo = McpHandlerUtils::CreateResultObject();

    if (!BlueprintPath.IsEmpty())
    {
        UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
        if (!Blueprint)
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CDO)
        {
            AddActorNetworkingInfo(NetworkingInfo, CDO);
        }
    }
    else if (!ActorName.IsEmpty())
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World)
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
            return true;
        }

        AActor* Actor = FindActorByName(World, ActorName);
        if (!Actor)
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Actor not found"), TEXT("NOT_FOUND"));
            return true;
        }

        AddActorNetworkingInfo(NetworkingInfo, Actor);
        NetworkingInfo->SetStringField(TEXT("role"), NetRoleToString(Actor->GetLocalRole()));
        NetworkingInfo->SetStringField(TEXT("remoteRole"), NetRoleToString(Actor->GetRemoteRole()));
        NetworkingInfo->SetBoolField(TEXT("hasAuthority"), Actor->HasAuthority());
    }
    else
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Must provide either blueprintPath or actorName"), TEXT("INVALID_PARAMS"));
        return true;
    }

    Context.ResultJson->SetBoolField(TEXT("success"), true);
    Context.ResultJson->SetObjectField(TEXT("networkingInfo"), NetworkingInfo);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Networking info retrieved"), Context.ResultJson);
    return true;
}
}
