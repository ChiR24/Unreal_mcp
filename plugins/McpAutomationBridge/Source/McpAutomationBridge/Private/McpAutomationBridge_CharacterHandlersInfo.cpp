#include "McpAutomationBridge_CharacterHandlers.h"

#if WITH_EDITOR
namespace McpCharacterHandlers
{
static FString GetSceneComponentParentName(const USceneComponent* Component)
{
    if (!Component)
    {
        return TEXT("");
    }
    const USceneComponent* Parent = Component->GetAttachParent();
    return Parent ? Parent->GetName() : TEXT("");
}

static TSharedPtr<FJsonObject> CreateCameraComponentReport(const UCameraComponent* Camera)
{
    TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
    if (!Camera)
    {
        return Report;
    }

    Report->SetStringField(TEXT("name"), Camera->GetName());
    Report->SetStringField(TEXT("class"), Camera->GetClass()->GetName());
    Report->SetStringField(TEXT("attachParent"), GetSceneComponentParentName(Camera));
    Report->SetBoolField(TEXT("active"), Camera->IsActive());
    Report->SetBoolField(TEXT("visible"), Camera->IsVisible());
    Report->SetBoolField(TEXT("usePawnControlRotation"), Camera->bUsePawnControlRotation);
    Report->SetNumberField(TEXT("fieldOfView"), Camera->FieldOfView);
    Report->SetObjectField(TEXT("relativeLocation"), McpHandlerUtils::VectorToJson(Camera->GetRelativeLocation()));
    Report->SetObjectField(TEXT("relativeRotation"), McpHandlerUtils::RotatorToJson(Camera->GetRelativeRotation()));
    Report->SetObjectField(TEXT("worldLocation"), McpHandlerUtils::VectorToJson(Camera->GetComponentLocation()));
    Report->SetObjectField(TEXT("worldRotation"), McpHandlerUtils::RotatorToJson(Camera->GetComponentRotation()));
    return Report;
}

static TSharedPtr<FJsonObject> CreateSpringArmComponentReport(const USpringArmComponent* SpringArm)
{
    TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
    if (!SpringArm)
    {
        return Report;
    }

    Report->SetStringField(TEXT("name"), SpringArm->GetName());
    Report->SetStringField(TEXT("class"), SpringArm->GetClass()->GetName());
    Report->SetStringField(TEXT("attachParent"), GetSceneComponentParentName(SpringArm));
    Report->SetBoolField(TEXT("active"), SpringArm->IsActive());
    Report->SetBoolField(TEXT("visible"), SpringArm->IsVisible());
    Report->SetNumberField(TEXT("targetArmLength"), SpringArm->TargetArmLength);
    Report->SetBoolField(TEXT("usePawnControlRotation"), SpringArm->bUsePawnControlRotation);
    Report->SetBoolField(TEXT("enableCameraLag"), SpringArm->bEnableCameraLag);
    Report->SetNumberField(TEXT("cameraLagSpeed"), SpringArm->CameraLagSpeed);
    Report->SetObjectField(TEXT("relativeLocation"), McpHandlerUtils::VectorToJson(SpringArm->GetRelativeLocation()));
    Report->SetObjectField(TEXT("relativeRotation"), McpHandlerUtils::RotatorToJson(SpringArm->GetRelativeRotation()));
    Report->SetObjectField(TEXT("worldLocation"), McpHandlerUtils::VectorToJson(SpringArm->GetComponentLocation()));
    Report->SetObjectField(TEXT("worldRotation"), McpHandlerUtils::RotatorToJson(SpringArm->GetComponentRotation()));
    return Report;
}

static void AddPlayerViewStateReport(UWorld* World, TSharedPtr<FJsonObject> Result)
{
    TSharedPtr<FJsonObject> ViewState = MakeShared<FJsonObject>();
    ViewState->SetBoolField(TEXT("isPIE"), World && World->WorldType == EWorldType::PIE);
    if (!World)
    {
        ViewState->SetStringField(TEXT("status"), TEXT("No active PIE world"));
        Result->SetObjectField(TEXT("playerViewState"), ViewState);
        return;
    }

    APlayerController* PlayerController = World->GetFirstPlayerController();
    if (!PlayerController)
    {
        ViewState->SetStringField(TEXT("status"), TEXT("No player controller"));
        Result->SetObjectField(TEXT("playerViewState"), ViewState);
        return;
    }

    ViewState->SetStringField(TEXT("playerController"), PlayerController->GetName());
    ViewState->SetStringField(TEXT("playerControllerClass"), PlayerController->GetClass()->GetName());
    if (APawn* Pawn = PlayerController->GetPawn())
    {
        ViewState->SetStringField(TEXT("pawn"), Pawn->GetName());
        ViewState->SetStringField(TEXT("pawnClass"), Pawn->GetClass()->GetName());
        ViewState->SetBoolField(TEXT("bFindCameraComponentWhenViewTarget"), Pawn->bFindCameraComponentWhenViewTarget);
    }
    if (AActor* ViewTarget = PlayerController->GetViewTarget())
    {
        ViewState->SetStringField(TEXT("viewTarget"), ViewTarget->GetName());
        ViewState->SetStringField(TEXT("viewTargetClass"), ViewTarget->GetClass()->GetName());
        ViewState->SetBoolField(TEXT("viewTargetFindsCameraComponent"), ViewTarget->bFindCameraComponentWhenViewTarget);
    }
    if (APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
    {
        TSharedPtr<FJsonObject> CameraManagerJson = MakeShared<FJsonObject>();
        CameraManagerJson->SetStringField(TEXT("name"), CameraManager->GetName());
        CameraManagerJson->SetObjectField(TEXT("location"), McpHandlerUtils::VectorToJson(CameraManager->GetCameraLocation()));
        CameraManagerJson->SetObjectField(TEXT("rotation"), McpHandlerUtils::RotatorToJson(CameraManager->GetCameraRotation()));
        CameraManagerJson->SetNumberField(TEXT("fov"), CameraManager->GetFOVAngle());
        ViewState->SetObjectField(TEXT("playerCameraManager"), CameraManagerJson);
    }
    Result->SetObjectField(TEXT("playerViewState"), ViewState);
}

bool HandleGetCharacterInfo(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetStringField(TEXT("assetName"), Blueprint->GetName());
    ACharacter* CharCDO = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (CharCDO)
    {
        if (CharCDO->GetCapsuleComponent())
        {
            Result->SetNumberField(TEXT("capsuleRadius"), CharCDO->GetCapsuleComponent()->GetUnscaledCapsuleRadius());
            Result->SetNumberField(TEXT("capsuleHalfHeight"), CharCDO->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
        }
        if (CharCDO->GetCharacterMovement())
        {
            UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();
            Result->SetNumberField(TEXT("walkSpeed"), Movement->MaxWalkSpeed);
            Result->SetNumberField(TEXT("jumpZVelocity"), Movement->JumpZVelocity);
            Result->SetNumberField(TEXT("airControl"), Movement->AirControl);
            Result->SetBoolField(TEXT("orientToMovement"), Movement->bOrientRotationToMovement);
            Result->SetNumberField(TEXT("gravityScale"), Movement->GravityScale);
            Result->SetNumberField(TEXT("customMovementSpeed"), Movement->MaxCustomMovementSpeed);
        }
        Result->SetNumberField(TEXT("maxJumpCount"), CharCDO->JumpMaxCount);
        Result->SetBoolField(TEXT("useControllerRotationYaw"), CharCDO->bUseControllerRotationYaw);
    }

    bool bHasSpringArm = false;
    bool bHasCamera = false;
    TArray<TSharedPtr<FJsonValue>> SpringArmTemplates;
    TArray<TSharedPtr<FJsonValue>> CameraTemplates;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (!Node || !Node->ComponentTemplate)
        {
            continue;
        }
        if (USpringArmComponent* SpringArm = Cast<USpringArmComponent>(Node->ComponentTemplate))
        {
            bHasSpringArm = true;
            SpringArmTemplates.Add(MakeShared<FJsonValueObject>(CreateSpringArmComponentReport(SpringArm)));
        }
        else if (UCameraComponent* Camera = Cast<UCameraComponent>(Node->ComponentTemplate))
        {
            bHasCamera = true;
            CameraTemplates.Add(MakeShared<FJsonValueObject>(CreateCameraComponentReport(Camera)));
        }
    }

    Result->SetBoolField(TEXT("hasSpringArm"), bHasSpringArm);
    Result->SetBoolField(TEXT("hasCamera"), bHasCamera);
    Result->SetArrayField(TEXT("springArmTemplates"), SpringArmTemplates);
    Result->SetArrayField(TEXT("cameraTemplates"), CameraTemplates);
    if (CharCDO)
    {
        Result->SetBoolField(TEXT("bFindCameraComponentWhenViewTarget"), CharCDO->bFindCameraComponentWhenViewTarget);
    }
    AddPlayerViewStateReport((GEditor && GEditor->PlayWorld) ? GEditor->PlayWorld.Get() : nullptr, Result);

    TArray<TSharedPtr<FJsonValue>> MovementVars;
    for (const FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        const FString VarName = Var.VarName.ToString();
        if (VarName.StartsWith(TEXT("bIs")) || VarName.StartsWith(TEXT("bCan")) ||
            VarName.Contains(TEXT("Speed")) || VarName.Contains(TEXT("Movement")))
        {
            MovementVars.Add(MakeShared<FJsonValueString>(VarName));
        }
    }
    if (MovementVars.Num() > 0)
    {
        Result->SetArrayField(TEXT("movementVariables"), MovementVars);
    }

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Character info retrieved"), Result);
    return true;
}
}
#endif
