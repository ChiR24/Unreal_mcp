#include "Domains/Networking/McpNetPrivate.h"

namespace McpNetworkingHandlers
{
static void AddActorNetworkingInfo(TSharedPtr<FJsonObject>& NetworkingInfo, AActor* Actor)
{
    NetworkingInfo->SetBoolField(TEXT("bReplicates"), Actor->GetIsReplicated());
    NetworkingInfo->SetBoolField(TEXT("bAlwaysRelevant"), Actor->bAlwaysRelevant);
    NetworkingInfo->SetBoolField(TEXT("bOnlyRelevantToOwner"), Actor->bOnlyRelevantToOwner);
#if MCP_HAS_ACTOR_NET_UPDATE_FREQUENCY_ACCESSORS
    NetworkingInfo->SetNumberField(TEXT("netUpdateFrequency"), Actor->GetNetUpdateFrequency());
    NetworkingInfo->SetNumberField(TEXT("minNetUpdateFrequency"), Actor->GetMinNetUpdateFrequency());
    NetworkingInfo->SetNumberField(TEXT("netCullDistanceSquared"), Actor->GetNetCullDistanceSquared());
#else
    NetworkingInfo->SetNumberField(TEXT("netUpdateFrequency"), Actor->NetUpdateFrequency);
    NetworkingInfo->SetNumberField(TEXT("minNetUpdateFrequency"), Actor->MinNetUpdateFrequency);
    NetworkingInfo->SetNumberField(TEXT("netCullDistanceSquared"), Actor->NetCullDistanceSquared);
#endif
    NetworkingInfo->SetNumberField(TEXT("netPriority"), Actor->NetPriority);
    NetworkingInfo->SetStringField(TEXT("netDormancy"), McpEnumName<ENetDormancy>(Actor->NetDormancy));
}

bool HandleGetNetworkingInfo(FNetworkingActionContext& Context)
{
    FString BlueprintPath = GetJsonStringField(Context.Payload, TEXT("blueprintPath"));
    FString ActorName = GetJsonStringField(Context.Payload, TEXT("actorName"));
    TSharedPtr<FJsonObject> NetworkingInfo = McpHandlerUtils::CreateResultObject();

    if (!BlueprintPath.IsEmpty())
    {
        UBlueprint* Blueprint = LoadBlueprintOrReply(Context, BlueprintPath);
        if (!Blueprint)
        {
            return true;
        }

        AActor* CDO = Blueprint->GeneratedClass ? Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
        if (CDO)
        {
            AddActorNetworkingInfo(NetworkingInfo, CDO);
        }
    }
    else if (!ActorName.IsEmpty())
    {
        // During Play, the running actor: in the editor world every actor has authority and no pawn a controller.
        AActor* Actor = FindNetActorOrReply(Context, ActorName);
        if (!Actor)
        {
            return true;
        }

        AddActorNetworkingInfo(NetworkingInfo, Actor);
        NetworkingInfo->SetStringField(TEXT("role"), McpEnumName<ENetRole>(Actor->GetLocalRole()));
        NetworkingInfo->SetStringField(TEXT("remoteRole"), McpEnumName<ENetRole>(Actor->GetRemoteRole()));
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
