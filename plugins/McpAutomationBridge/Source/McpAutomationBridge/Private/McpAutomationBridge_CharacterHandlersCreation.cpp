#include "McpAutomationBridge_CharacterHandlers.h"

#if WITH_EDITOR
namespace McpCharacterHandlers
{
bool HandleCreateCharacterBlueprint(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString Name = GetJsonStringField(Payload, TEXT("name"));
    const FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game"));
    if (Name.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    USkeletalMesh* RequestedMesh = nullptr;
    if (!SkeletalMeshPath.IsEmpty())
    {
        RequestedMesh = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPath);
        if (!RequestedMesh)
        {
            Self->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Skeletal mesh not found: %s"), *SkeletalMeshPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
    }

    FString Error;
    UBlueprint* Blueprint = CreateCharacterBlueprintAsset(Path, Name, Error);
    if (!Blueprint)
    {
        Self->SendAutomationError(Socket, RequestId, Error, TEXT("CREATION_FAILED"));
        return true;
    }

    bool bSkeletalMeshAssigned = false;
    if (RequestedMesh)
    {
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<USkeletalMeshComponent>())
            {
                if (USkeletalMeshComponent* MeshComp = Cast<USkeletalMeshComponent>(Node->ComponentTemplate))
                {
                    MeshComp->SetSkeletalMesh(RequestedMesh);
                    bSkeletalMeshAssigned = true;
                }
                break;
            }
        }
    }

    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Path / Name);
    Result->SetStringField(TEXT("name"), Name);
    Result->SetStringField(TEXT("parentClass"), TEXT("Character"));
    if (!SkeletalMeshPath.IsEmpty())
    {
        Result->SetStringField(TEXT("skeletalMesh"), SkeletalMeshPath);
        Result->SetBoolField(TEXT("skeletalMeshAssigned"), bSkeletalMeshAssigned);
    }
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Character blueprint created"), Result);
    return true;
}
}
#endif
