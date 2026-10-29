// configure_camera_component view first_person: the Character looks through a camera at its eyes. A pawn views through
// its first active camera, so every other camera (the third-person boom's FollowCamera) stops activating on its own and
// FirstPersonCamera, made at the capsule's centre plus eyeHeight when missing, does; nothing is deleted, so view
// third_person turns the boom's camera back on. The body turns with the aim (useControllerRotationYaw on,
// orientToMovement off), as in Epic's first-person template, and the player no longer sees the character's own mesh
// (owner no see), which a camera inside the head would otherwise look out through.

#include "Domains/Character/McpCharacterHandlers.h"

namespace McpCharacterHandlers
{
bool HandleFirstPersonCamera(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                             FCharacterSocket Socket, UBlueprint* Blueprint)
{
    ACharacter* Character = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    USimpleConstructionScript* Scs = Blueprint->SimpleConstructionScript;
    if (!Character || !Scs)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("%s is not a Character Blueprint."), *Blueprint->GetPathName()),
            TEXT("NOT_A_CHARACTER"));
        return true;
    }
    const float EyeHeight = static_cast<float>(GetJsonNumberField(Payload, TEXT("eyeHeight"), Character->BaseEyeHeight));
    UCameraComponent* Eyes = nullptr;
    TArray<TSharedPtr<FJsonValue>> TurnedOff;
    for (USCS_Node* Node : Scs->GetAllNodes())
    {
        UCameraComponent* Camera = Cast<UCameraComponent>(Node ? Node->ComponentTemplate : nullptr);
        if (Camera && Node->GetVariableName() == FirstPersonCameraName())
        {
            Eyes = Camera;
        }
        else if (Camera && Camera->bAutoActivate)
        {
            Camera->bAutoActivate = false;
            TurnedOff.Add(MakeShared<FJsonValueString>(Node->GetVariableName().ToString()));
        }
    }
    if (!Eyes)
    {
        USCS_Node* Node = Scs->CreateNode(UCameraComponent::StaticClass(), FirstPersonCameraName());
        Eyes = Node ? Cast<UCameraComponent>(Node->ComponentTemplate) : nullptr;
        if (!Eyes)
        {
            Self->SendAutomationError(Socket, RequestId, TEXT("Could not create the first-person camera."), TEXT("COMPONENT_CREATION_FAILED"));
            return true;
        }
        Scs->AddNode(Node);
    }
    Eyes->SetRelativeLocation(FVector(0.0, 0.0, EyeHeight));
    Eyes->bUsePawnControlRotation = true;
    Eyes->bAutoActivate = true;
    Character->bUseControllerRotationYaw = true;
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    if (Movement)
    {
        Movement->bOrientRotationToMovement = false;
    }
    USkeletalMeshComponent* Body = Character->GetMesh();
    if (Body)
    {
        Body->SetOwnerNoSee(true);
    }
    // Read before the compile below, which replaces the class default object these values sit on.
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), GetJsonStringField(Payload, TEXT("blueprintPath")));
    Result->SetStringField(TEXT("view"), TEXT("first_person"));
    Result->SetStringField(TEXT("activeCamera"), FirstPersonCameraName().ToString());
    Result->SetNumberField(TEXT("eyeHeight"), Eyes->GetRelativeLocation().Z);
    Result->SetArrayField(TEXT("camerasTurnedOff"), TurnedOff);
    Result->SetBoolField(TEXT("useControllerRotationYaw"), Character->bUseControllerRotationYaw);
    Result->SetBoolField(TEXT("orientToMovement"), Movement && Movement->bOrientRotationToMovement);
    Result->SetBoolField(TEXT("bodyHiddenFromOwner"), Body && Body->bOwnerNoSee);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    if (!CommitCharacterEdit(Self, RequestId, Blueprint, Socket))
    {
        return true;
    }
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("First-person camera configured"), Result);
    return true;
}
} // namespace McpCharacterHandlers
