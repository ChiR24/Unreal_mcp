#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"

#include "Domains/Combat/McpAutomationBridge_CombatHandlersPrivate.h"

namespace
{
// The skeletal mesh a bone-bound hitbox hangs from: an SCS mesh node, else an inherited native
// mesh (a Character's CharacterMesh0). Both null when the Blueprint has none.
void FindHitboxMeshParent(UBlueprint* Blueprint, USCS_Node*& OutNode, USkeletalMeshComponent*& OutNative)
{
    OutNode = nullptr;
    OutNative = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<USkeletalMeshComponent>())
        {
            OutNode = Node;
            return;
        }
    }
    if (AActor* CDO = Blueprint->GeneratedClass ? Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr)
    {
        OutNative = CDO->FindComponentByClass<USkeletalMeshComponent>();
    }
}

// Re-parent the hitbox node under the mesh and pin it to Bone.
FString AttachHitboxToBone(UBlueprint* Blueprint, UActorComponent* Hitbox, const FName Bone)
{
    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    USCS_Node* HitboxNode = nullptr;
    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        if (Node && Node->ComponentTemplate == Hitbox) { HitboxNode = Node; break; }
    }
    USCS_Node* MeshNode = nullptr;
    USkeletalMeshComponent* NativeMesh = nullptr;
    FindHitboxMeshParent(Blueprint, MeshNode, NativeMesh);
    if (!HitboxNode || (!MeshNode && !NativeMesh))
    {
        return FString();
    }
    SCS->RemoveNode(HitboxNode, false);
    if (MeshNode)
    {
        MeshNode->AddChildNode(HitboxNode);
    }
    else
    {
        SCS->AddNode(HitboxNode);
        HitboxNode->SetParent(NativeMesh);
    }
    HitboxNode->AttachToName = Bone;
    return MeshNode ? MeshNode->GetVariableName().ToString() : NativeMesh->GetName();
}
}

namespace McpCombatHandlers
{
bool FCombatActionContext::HandleDamageExecution() const
{
    if (SubAction == TEXT("setup_hitbox_component"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->SimpleConstructionScript)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found."), TEXT("NOT_FOUND"));
            return true;
        }

        FString HitboxType = GetJsonStringField(Payload, TEXT("hitboxType"), TEXT("Capsule"));
        FString BoneName = GetJsonStringField(Payload, TEXT("hitboxBoneName"), TEXT(""));
        if (!BoneName.IsEmpty())
        {
            // Check before creating anything: a bone needs a skeletal mesh to belong to.
            USCS_Node* MeshNode = nullptr;
            USkeletalMeshComponent* NativeMesh = nullptr;
            FindHitboxMeshParent(Blueprint, MeshNode, NativeMesh);
            if (!MeshNode && !NativeMesh)
            {
                SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("hitboxBoneName '%s' needs a SkeletalMeshComponent on the Blueprint; add one or omit hitboxBoneName."), *BoneName),
                    TEXT("NO_SKELETAL_MESH"));
                return true;
            }
        }
        bool bIsDamageZoneHead = GetJsonBoolField(Payload, TEXT("isDamageZoneHead"), false);
        double DamageMultiplier = GetJsonNumberField(Payload, TEXT("damageMultiplier"), 1.0);
        TSharedPtr<FJsonObject> AppliedHitboxSize = MakeShared<FJsonObject>();
        UActorComponent* CreatedHitbox = nullptr;
        // GetObjectField on an omitted hitboxSize handed back an empty object, so its
        // defaults (34/88, 50) overwrote the component's size. Only what was passed changes.
        const TSharedPtr<FJsonObject>* SizePtr = nullptr;
        const TSharedPtr<FJsonObject> Size = Payload->TryGetObjectField(TEXT("hitboxSize"), SizePtr) && SizePtr ? *SizePtr : nullptr;

        if (HitboxType == TEXT("Capsule"))
        {
            UCapsuleComponent* Hitbox = GetOrCreateSCSComponent<UCapsuleComponent>(Blueprint, TEXT("HitboxCapsule"));
            CreatedHitbox = Hitbox;
            if (Hitbox)
            {
                if (Size.IsValid() && Size->HasField(TEXT("radius")))
                {
                    Hitbox->SetCapsuleRadius(static_cast<float>(Size->GetNumberField(TEXT("radius"))));
                }
                if (Size.IsValid() && Size->HasField(TEXT("halfHeight")))
                {
                    Hitbox->SetCapsuleHalfHeight(static_cast<float>(Size->GetNumberField(TEXT("halfHeight"))));
                }
                AppliedHitboxSize->SetNumberField(TEXT("radius"), Hitbox->GetUnscaledCapsuleRadius());
                AppliedHitboxSize->SetNumberField(TEXT("halfHeight"), Hitbox->GetUnscaledCapsuleHalfHeight());
            }
        }
        else if (HitboxType == TEXT("Box"))
        {
            UBoxComponent* Hitbox = GetOrCreateSCSComponent<UBoxComponent>(Blueprint, TEXT("HitboxBox"));
            CreatedHitbox = Hitbox;
            if (Hitbox)
            {
                if (Size.IsValid() && Size->HasField(TEXT("extent")))
                {
                    Hitbox->SetBoxExtent(ExtractVectorField(Size, TEXT("extent"), Hitbox->GetUnscaledBoxExtent()));
                }
                AppliedHitboxSize->SetObjectField(TEXT("extent"), McpHandlerUtils::VectorToJson(Hitbox->GetUnscaledBoxExtent()));
            }
        }
        else if (HitboxType == TEXT("Sphere"))
        {
            USphereComponent* Hitbox = GetOrCreateSCSComponent<USphereComponent>(Blueprint, TEXT("HitboxSphere"));
            CreatedHitbox = Hitbox;
            if (Hitbox)
            {
                if (Size.IsValid() && Size->HasField(TEXT("radius")))
                {
                    Hitbox->SetSphereRadius(static_cast<float>(Size->GetNumberField(TEXT("radius"))));
                }
                AppliedHitboxSize->SetNumberField(TEXT("radius"), Hitbox->GetUnscaledSphereRadius());
            }
        }

        if (!CreatedHitbox)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Could not create a %s hitbox; hitboxType must be Capsule, Box or Sphere."), *HitboxType), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        const FString BoneParent = BoneName.IsEmpty() ? FString() : AttachHitboxToBone(Blueprint, CreatedHitbox, FName(*BoneName));

        AddBlueprintVariableCombat(Blueprint, TEXT("bIsHeadshotZone"), MakeBoolPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("HitboxDamageMultiplier"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FBoolProperty* HeadProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsHeadshotZone")))
                {
                    HeadProp->SetPropertyValue_InContainer(CDO, bIsDamageZoneHead);
                }
                if (FDoubleProperty* MultProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HitboxDamageMultiplier")))
                {
                    MultProp->SetPropertyValue_InContainer(CDO, DamageMultiplier);
                }
            }
        }

        if (!McpSafeAssetSave(Blueprint))
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s was changed in the editor but could not be saved to disk."), *Blueprint->GetPathName()), TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("hitboxType"), HitboxType);
        Result->SetObjectField(TEXT("hitboxSize"), AppliedHitboxSize);
        if (!BoneParent.IsEmpty())
        {
            Result->SetStringField(TEXT("hitboxBoneName"), BoneName);
            Result->SetStringField(TEXT("attachedToComponent"), BoneParent);
        }
        Result->SetBoolField(TEXT("isDamageZoneHead"), bIsDamageZoneHead);
        Result->SetNumberField(TEXT("damageMultiplier"), DamageMultiplier);

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hitbox component configured."), Result);
        return true;
    }

    if (SubAction == TEXT("configure_hit_detection"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found."), TEXT("NOT_FOUND"));
            return true;
        }

        FString HitboxType = GetJsonStringField(Payload, TEXT("hitboxType"), TEXT("Capsule"));
        double DamageMultiplier = GetJsonNumberField(Payload, TEXT("damageMultiplier"), 1.0);

        if (HitboxType == TEXT("Capsule"))
        {
            GetOrCreateSCSComponent<UCapsuleComponent>(Blueprint, TEXT("HitboxCapsule"));
        }
        else if (HitboxType == TEXT("Box"))
        {
            GetOrCreateSCSComponent<UBoxComponent>(Blueprint, TEXT("HitboxBox"));
        }
        else
        {
            GetOrCreateSCSComponent<USphereComponent>(Blueprint, TEXT("HitboxSphere"));
        }

        AddBlueprintVariableCombat(Blueprint, TEXT("HitboxDamageMultiplier"), MakeFloatPinType());
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // damageMultiplier was read and then dropped -- the variable was created
        // but never given the requested value, unlike setup_hitbox_component
        // above, which writes the same variable through the CDO.
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* MultProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HitboxDamageMultiplier")))
                {
                    MultProp->SetPropertyValue_InContainer(CDO, DamageMultiplier);
                }
            }
        }

        if (!McpSafeAssetSave(Blueprint))
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s was changed in the editor but could not be saved to disk."), *Blueprint->GetPathName()), TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("hitboxType"), HitboxType);
        Result->SetNumberField(TEXT("damageMultiplier"), DamageMultiplier);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hit detection configured."), Result);
        return true;
    }

    return false;
}
}
