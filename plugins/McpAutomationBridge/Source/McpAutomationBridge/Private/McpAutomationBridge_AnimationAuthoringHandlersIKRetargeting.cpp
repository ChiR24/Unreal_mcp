#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleIKRetargetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("create_ik_retargeter"))
    {
#if MCP_HAS_IKRETARGET_FACTORY && MCP_HAS_IKRETARGETER
        FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Retargeting")));
        FString SourceIKRigPath = GetStringFieldAnimAuth(Params, TEXT("sourceIKRigPath"), TEXT(""));
        FString TargetIKRigPath = GetStringFieldAnimAuth(Params, TEXT("targetIKRigPath"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }

        // Create the IK Retargeter using factory
        FString FullPath = Path / Name;
        FString PackageName = FullPath;
        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create package for IK Retargeter"), TEXT("PACKAGE_ERROR"));
        }

        UIKRetargetFactory* Factory = NewObject<UIKRetargetFactory>();

        UIKRetargeter* Retargeter = Cast<UIKRetargeter>(Factory->FactoryCreateNew(
            UIKRetargeter::StaticClass(),
            Package,
            FName(*Name),
            RF_Public | RF_Standalone,
            nullptr,
            GWarn
        ));

        if (!Retargeter)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create IK Retargeter"), TEXT("CREATION_FAILED"));
        }

// Set source and target IK Rigs using the controller (UE 5.1+ requires this as direct access is private)
#if MCP_HAS_IKRETARGETER_CONTROLLER
if (UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter))
{
if (!SourceIKRigPath.IsEmpty())
{
UIKRigDefinition* SourceRig = Cast<UIKRigDefinition>(StaticLoadObject(UIKRigDefinition::StaticClass(), nullptr, *SourceIKRigPath));
if (SourceRig)
{
MCP_IKRETARGETER_SET_SOURCE_IKRIG(Controller, SourceRig);
}
}
if (!TargetIKRigPath.IsEmpty())
{
UIKRigDefinition* TargetRig = Cast<UIKRigDefinition>(StaticLoadObject(UIKRigDefinition::StaticClass(), nullptr, *TargetIKRigPath));
if (TargetRig)
{
MCP_IKRETARGETER_SET_TARGET_IKRIG(Controller, TargetRig);
}
}
}
#else
// Fallback for UE 5.0 where direct access was public
if (!SourceIKRigPath.IsEmpty())
{
UIKRigDefinition* SourceRig = Cast<UIKRigDefinition>(StaticLoadObject(UIKRigDefinition::StaticClass(), nullptr, *SourceIKRigPath));
if (SourceRig)
{
Retargeter->SourceIKRigAsset = SourceRig;
}
}
if (!TargetIKRigPath.IsEmpty())
{
UIKRigDefinition* TargetRig = Cast<UIKRigDefinition>(StaticLoadObject(UIKRigDefinition::StaticClass(), nullptr, *TargetIKRigPath));
if (TargetRig)
{
Retargeter->TargetIKRigAsset = TargetRig;
}
}
#endif

        if (!SaveAnimAsset(Retargeter, bSave))
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to save IK Retargeter asset"), TEXT("SAVE_FAILED"));
        }

        Response->SetStringField(TEXT("assetPath"), Retargeter->GetPathName());
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("IK Retargeter '%s' created successfully"), *Name));
#elif MCP_HAS_IKRETARGETER
        ANIM_ERROR_RESPONSE(
            TEXT("create_ik_retargeter requires the IKRigEditor factory module in this build"),
            TEXT("IKRETARGET_FACTORY_UNAVAILABLE"));
#else
        ANIM_ERROR_RESPONSE(TEXT("IK Retargeter module not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }

    if (SubAction == TEXT("set_retarget_chain_mapping"))
    {
#if MCP_HAS_IKRETARGETER
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString SourceChain = GetStringFieldAnimAuth(Params, TEXT("sourceChain"), TEXT(""));
        FString TargetChain = GetStringFieldAnimAuth(Params, TEXT("targetChain"), TEXT(""));

        if (SourceChain.IsEmpty() || TargetChain.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("sourceChain and targetChain are required"), TEXT("MISSING_CHAINS"));
        }

        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Chain mapping '%s' -> '%s' set"), *SourceChain, *TargetChain));
#else
        ANIM_ERROR_RESPONSE(TEXT("IK Retargeter module not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }

    // ===== Utility =====
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
