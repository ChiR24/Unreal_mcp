#include "Domains/Character/McpAutomationBridge_CharacterHandlers.h"

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

    // Setting only the radius used to reset the half-height to 96 (and the
    // other way round); each size now changes only when sent.
    ACharacter* CharCDO = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    UCapsuleComponent* Capsule = CharCDO ? CharCDO->GetCapsuleComponent() : nullptr;
    if (!Capsule)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("%s is not a Character Blueprint with a capsule"), *BlueprintPath), TEXT("NOT_A_CHARACTER"));
        return true;
    }
    if (Payload->HasField(TEXT("capsuleRadius")))
    {
        Capsule->SetCapsuleRadius(static_cast<float>(GetJsonNumberField(Payload, TEXT("capsuleRadius"), 42.0)));
    }
    if (Payload->HasField(TEXT("capsuleHalfHeight")))
    {
        Capsule->SetCapsuleHalfHeight(static_cast<float>(GetJsonNumberField(Payload, TEXT("capsuleHalfHeight"), 96.0)));
    }
    const float CapsuleRadius = Capsule->GetUnscaledCapsuleRadius();
    const float CapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint); // compile so the added variables are usable (dogfood #39)
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

        if (Payload->HasField(TEXT("meshOffset")))
        {
            CharCDO->GetMesh()->SetRelativeLocation(ExtractVectorField(Payload, TEXT("meshOffset"), FVector::ZeroVector));
        }
        if (Payload->HasField(TEXT("meshRotation")))
        {
            CharCDO->GetMesh()->SetRelativeRotation(ExtractRotatorField(Payload, TEXT("meshRotation"), FRotator::ZeroRotator));
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint); // compile so the added variables are usable (dogfood #39)
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

    // An existing boom changes only in the fields sent: every call used to
    // write all four with their defaults, so setting the arm length alone
    // turned lag off and reset its speed. A new boom starts from the
    // third-person defaults for whatever the call leaves out.
    const auto ApplyArm = [&Payload](USpringArmComponent* Arm, bool bFresh)
    {
        if (bFresh || Payload->HasField(TEXT("springArmLength")))
            Arm->TargetArmLength = static_cast<float>(GetJsonNumberField(Payload, TEXT("springArmLength"), 300.0));
        if (bFresh || Payload->HasField(TEXT("cameraUsePawnControlRotation")))
            Arm->bUsePawnControlRotation = GetJsonBoolField(Payload, TEXT("cameraUsePawnControlRotation"), true);
        if (bFresh || Payload->HasField(TEXT("springArmLagEnabled")))
            Arm->bEnableCameraLag = GetJsonBoolField(Payload, TEXT("springArmLagEnabled"), false);
        if (bFresh || Payload->HasField(TEXT("springArmLagSpeed")))
            Arm->CameraLagSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("springArmLagSpeed"), 10.0));
    };
    USpringArmComponent* ConfiguredArm = nullptr;
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
            ApplyArm(SpringArm, false);
            ConfiguredArm = SpringArm;
            // One flag used to drive BOTH components, which cannot express any
            // working third-person rig: the boom must follow control rotation
            // while the camera stays arm-relative. Both-true made the camera
            // re-apply control rotation on top of the arm, defeating camera lag
            // and collision pull-in; both-false left the boom deaf to look
            // input. The flag is the RIG's (the boom's); a camera parented to a
            // boom is always arm-relative, as in Epic's own template.
            for (USCS_Node* ChildNode : Node->ChildNodes)
            {
                if (UCameraComponent* Camera = Cast<UCameraComponent>(
                        ChildNode ? ChildNode->ComponentTemplate : nullptr))
                {
                    Camera->bUsePawnControlRotation = false;
                }
            }
        }
    }

    if (!bHasSpringArm)
    {
        USCS_Node* SpringArmNode = Blueprint->SimpleConstructionScript->CreateNode(USpringArmComponent::StaticClass(), FName(TEXT("CameraBoom")));
        if (SpringArmNode)
        {
            if (USpringArmComponent* SpringArm = Cast<USpringArmComponent>(SpringArmNode->ComponentTemplate))
            {
                ApplyArm(SpringArm, true);
                ConfiguredArm = SpringArm;
            }
            Blueprint->SimpleConstructionScript->AddNode(SpringArmNode);
            if (USCS_Node* CameraNode = Blueprint->SimpleConstructionScript->CreateNode(UCameraComponent::StaticClass(), FName(TEXT("FollowCamera"))))
            {
                if (UCameraComponent* Camera = Cast<UCameraComponent>(CameraNode->ComponentTemplate))
                {
                    Camera->bUsePawnControlRotation = false;
                }
                // Attach via AddChildNode rather than SetParent + AddNode.
                // AddNode() registers the camera as a second ROOT whose parent is
                // only a textual name; at compile time the engine then cannot find
                // 'CameraBoom' ("FixupRootNodeParentReferences: Couldn't find
                // inherited parent component 'CameraBoom' for 'FollowCamera'") and
                // the spring-arm hierarchy silently flattens. AddChildNode() moves
                // the node under its actual parent (ChildNodes + AllNodes).
                SpringArmNode->AddChildNode(CameraNode);
            }
        }
    }

    if (!ConfiguredArm)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Could not create the camera boom"), TEXT("COMPONENT_CREATION_FAILED"));
        return true;
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("springArmLength"), ConfiguredArm->TargetArmLength);
    Result->SetBoolField(TEXT("usePawnControlRotation"), ConfiguredArm->bUsePawnControlRotation);
    Result->SetBoolField(TEXT("springArmUsePawnControlRotation"), ConfiguredArm->bUsePawnControlRotation);
    Result->SetBoolField(TEXT("cameraUsePawnControlRotation"), false);
    Result->SetBoolField(TEXT("lagEnabled"), ConfiguredArm->bEnableCameraLag);
    Result->SetNumberField(TEXT("springArmLagSpeed"), ConfiguredArm->CameraLagSpeed);
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Camera configured"), Result);
    return true;
}
}
