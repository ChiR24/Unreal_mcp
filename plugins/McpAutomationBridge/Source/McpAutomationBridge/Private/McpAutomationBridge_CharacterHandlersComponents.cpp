#include "McpAutomationBridge_CharacterHandlers.h"

#if WITH_EDITOR
namespace McpCharacterHandlers
{
bool HandleConfigureCapsuleComponent(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    const float CapsuleRadius = static_cast<float>(GetJsonNumberField(Payload, TEXT("capsuleRadius"), 42.0));
    const float CapsuleHalfHeight = static_cast<float>(GetJsonNumberField(Payload, TEXT("capsuleHalfHeight"), 96.0));
    ACharacter* CharCDO = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (CharCDO && CharCDO->GetCapsuleComponent())
    {
        CharCDO->GetCapsuleComponent()->SetCapsuleRadius(CapsuleRadius);
        CharCDO->GetCapsuleComponent()->SetCapsuleHalfHeight(CapsuleHalfHeight);
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("capsuleRadius"), CapsuleRadius);
    Result->SetNumberField(TEXT("capsuleHalfHeight"), CapsuleHalfHeight);
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Capsule configured"), Result);
    return true;
}

bool HandleConfigureMeshComponent(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    const FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    const FString AnimBPPath = GetJsonStringField(Payload, TEXT("animBlueprintPath"));
    USkeletalMesh* RequestedMesh = nullptr;
    UAnimBlueprint* RequestedAnimBP = nullptr;
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
    if (!AnimBPPath.IsEmpty())
    {
        RequestedAnimBP = LoadObject<UAnimBlueprint>(nullptr, *AnimBPPath);
        if (!RequestedAnimBP || !RequestedAnimBP->GeneratedClass)
        {
            Self->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Animation Blueprint not found: %s"), *AnimBPPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
    }

    bool bSkeletalMeshAssigned = false;
    bool bAnimBlueprintAssigned = false;
    ACharacter* CharCDO = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (CharCDO && CharCDO->GetMesh())
    {
        if (RequestedMesh)
        {
            CharCDO->GetMesh()->SetSkeletalMesh(RequestedMesh);
            bSkeletalMeshAssigned = true;
        }
        if (RequestedAnimBP)
        {
            CharCDO->GetMesh()->SetAnimInstanceClass(RequestedAnimBP->GeneratedClass);
            bAnimBlueprintAssigned = true;
        }

        const TSharedPtr<FJsonObject>* OffsetObj;
        if (Payload->TryGetObjectField(TEXT("meshOffset"), OffsetObj))
        {
            CharCDO->GetMesh()->SetRelativeLocation(VectorFromJson(*OffsetObj));
        }
        const TSharedPtr<FJsonObject>* RotObj;
        if (Payload->TryGetObjectField(TEXT("meshRotation"), RotObj))
        {
            CharCDO->GetMesh()->SetRelativeRotation(RotatorFromJson(*RotObj));
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    if (!SkeletalMeshPath.IsEmpty())
    {
        Result->SetStringField(TEXT("skeletalMesh"), SkeletalMeshPath);
        Result->SetBoolField(TEXT("skeletalMeshAssigned"), bSkeletalMeshAssigned);
    }
    if (!AnimBPPath.IsEmpty())
    {
        Result->SetStringField(TEXT("animBlueprint"), AnimBPPath);
        Result->SetBoolField(TEXT("animBlueprintAssigned"), bAnimBlueprintAssigned);
    }
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh configured"), Result);
    return true;
}

bool HandleConfigureCameraComponent(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    const float SpringArmLength = static_cast<float>(GetJsonNumberField(Payload, TEXT("springArmLength"), 300.0));
    const bool UsePawnControlRotation = GetJsonBoolField(Payload, TEXT("cameraUsePawnControlRotation"), true);
    const bool LagEnabled = GetJsonBoolField(Payload, TEXT("springArmLagEnabled"), false);
    const float LagSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("springArmLagSpeed"), 10.0));
    bool bHasSpringArm = false;

    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (!Node || !Node->ComponentTemplate)
        {
            continue;
        }
        if (USpringArmComponent* SpringArm = Cast<USpringArmComponent>(Node->ComponentTemplate))
        {
            bHasSpringArm = true;
            SpringArm->TargetArmLength = SpringArmLength;
            SpringArm->bUsePawnControlRotation = UsePawnControlRotation;
            SpringArm->bEnableCameraLag = LagEnabled;
            SpringArm->CameraLagSpeed = LagSpeed;
        }
    }

    if (!bHasSpringArm)
    {
        USCS_Node* SpringArmNode = Blueprint->SimpleConstructionScript->CreateNode(USpringArmComponent::StaticClass(), FName(TEXT("CameraBoom")));
        if (SpringArmNode)
        {
            if (USpringArmComponent* SpringArm = Cast<USpringArmComponent>(SpringArmNode->ComponentTemplate))
            {
                SpringArm->TargetArmLength = SpringArmLength;
                SpringArm->bUsePawnControlRotation = UsePawnControlRotation;
                SpringArm->bEnableCameraLag = LagEnabled;
                SpringArm->CameraLagSpeed = LagSpeed;
            }
            Blueprint->SimpleConstructionScript->AddNode(SpringArmNode);
            if (USCS_Node* CameraNode = Blueprint->SimpleConstructionScript->CreateNode(UCameraComponent::StaticClass(), FName(TEXT("FollowCamera"))))
            {
                CameraNode->SetParent(SpringArmNode);
                Blueprint->SimpleConstructionScript->AddNode(CameraNode);
            }
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("springArmLength"), SpringArmLength);
    Result->SetBoolField(TEXT("usePawnControlRotation"), UsePawnControlRotation);
    Result->SetBoolField(TEXT("lagEnabled"), LagEnabled);
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Camera configured"), Result);
    return true;
}
}
#endif
