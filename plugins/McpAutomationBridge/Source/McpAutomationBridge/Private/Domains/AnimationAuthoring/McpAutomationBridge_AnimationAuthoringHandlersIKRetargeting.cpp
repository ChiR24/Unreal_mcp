#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"

namespace McpAnimationAuthoring {

#if MCP_HAS_IKRETARGETER && MCP_HAS_IKRETARGETER_CONTROLLER
TSharedPtr<FJsonObject> HandleSetRetargetChainMapping(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);
#endif

TSharedPtr<FJsonObject> HandleIKRetargetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("create_ik_retargeter"))
    {
#if MCP_HAS_IKRETARGET_FACTORY && MCP_HAS_IKRETARGETER
        FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Retargeting")));
        FString SourceIKRigPath = GetJsonStringField(Params, TEXT("sourceIKRigPath"), TEXT(""));
        FString TargetIKRigPath = GetJsonStringField(Params, TEXT("targetIKRigPath"), TEXT(""));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

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
#if ENGINE_MINOR_VERSION >= 6
// From 5.6 a retargeter does nothing without its op stack (chains live on the FK Chains op), and the
// factory makes an empty one; these are the ops the editor's own new-retargeter setup adds.
Controller->AddDefaultOps();
#endif
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
        return Response;
#elif MCP_HAS_IKRETARGETER
        ANIM_ERROR_RESPONSE(
            TEXT("create_ik_retargeter requires the IKRigEditor factory module in this build"),
            TEXT("IKRETARGET_FACTORY_UNAVAILABLE"));
#else
        ANIM_ERROR_RESPONSE(TEXT("IK Retargeter module not available"), TEXT("NOT_SUPPORTED"));
#endif
    }

    if (SubAction == TEXT("set_retarget_chain_mapping"))
    {
#if MCP_HAS_IKRETARGETER && MCP_HAS_IKRETARGETER_CONTROLLER
        return HandleSetRetargetChainMapping(Params, Response);
#else
        ANIM_ERROR_RESPONSE(TEXT("IK Retargeter editing is not available in this build"), TEXT("NOT_SUPPORTED"));
#endif
    }

    // ===== Utility =====
    return nullptr;
}

#if MCP_HAS_IKRETARGETER && MCP_HAS_IKRETARGETER_CONTROLLER
namespace
{
TArray<FName> RetargetRigChainNames(const UIKRigDefinition* Rig)
{
    TArray<FName> Names;
    if (Rig)
    {
        for (const FBoneChain& Chain : Rig->GetRetargetChains())
        {
            Names.Add(Chain.ChainName);
        }
    }
    return Names;
}

FString JoinRetargetChainNames(const TArray<FName>& Names)
{
    TArray<FString> Strings;
    for (const FName& Name : Names)
    {
        Strings.Add(Name.ToString());
    }
    return Strings.Num() > 0 ? FString::Join(Strings, TEXT(", ")) : FString(TEXT("none"));
}

// The source chain driving TargetChain; NAME_None when it is unmapped.
FName RetargetSourceChainFor(UIKRetargeterController* Controller, FName TargetChain)
{
#if ENGINE_MINOR_VERSION >= 2
    return Controller->GetSourceChain(TargetChain);
#else
    for (const TObjectPtr<URetargetChainSettings>& Settings : Controller->GetChainMappings())
    {
        if (Settings && Settings->TargetChain == TargetChain)
        {
            return Settings->SourceChain;
        }
    }
    return NAME_None;
#endif
}

bool SetRetargetSourceChain(UIKRetargeterController* Controller, FName SourceChain, FName TargetChain)
{
#if ENGINE_MINOR_VERSION >= 2
    return Controller->SetSourceChain(SourceChain, TargetChain);
#else
    for (const TObjectPtr<URetargetChainSettings>& Settings : Controller->GetChainMappings())
    {
        if (Settings && Settings->TargetChain == TargetChain)
        {
            Controller->SetSourceChainForTargetChain(Settings, SourceChain);
            return true;
        }
    }
    return false;
#endif
}
}

// set_retarget_chain_mapping: which source chain drives a target chain, or exact-then-fuzzy auto-mapping.
TSharedPtr<FJsonObject> HandleSetRetargetChainMapping(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    const FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath")));
    const FString TargetText = GetJsonStringField(Params, TEXT("targetChain"));
    const FString SourceText = GetJsonStringField(Params, TEXT("sourceChain"));
    const bool bAutoMap = GetJsonBoolField(Params, TEXT("autoMap"), false);
    UIKRetargeter* Retargeter = AssetPath.IsEmpty() ? nullptr : LoadObject<UIKRetargeter>(nullptr, *AssetPath);
    UIKRetargeterController* Controller = Retargeter ? UIKRetargeterController::GetController(Retargeter) : nullptr;
    if (!Controller)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("No IK Retargeter at '%s'."), *AssetPath), TEXT("ASSET_NOT_FOUND"));
    }
#if ENGINE_MINOR_VERSION >= 1
    const UIKRigDefinition* SourceRig = Controller->GetIKRig(ERetargetSourceOrTarget::Source);
    const UIKRigDefinition* TargetRig = Controller->GetIKRig(ERetargetSourceOrTarget::Target);
#else
    const UIKRigDefinition* SourceRig = Retargeter->GetSourceIKRig();
    const UIKRigDefinition* TargetRig = Retargeter->GetTargetIKRig();
#endif
    if (!SourceRig || !TargetRig)
    {
        ANIM_ERROR_RESPONSE(TEXT("The retargeter needs both a source and a target IK Rig before chains can be mapped (create_ik_retargeter sourceIKRigPath, targetIKRigPath)."), TEXT("RIGS_NOT_SET"));
    }
    const TArray<FName> SourceChains = RetargetRigChainNames(SourceRig);
    const TArray<FName> TargetChains = RetargetRigChainNames(TargetRig);
    const FName Target(*TargetText);
    const FName Source = SourceText.IsEmpty() || SourceText.Equals(TEXT("None"), ESearchCase::IgnoreCase) ? NAME_None : FName(*SourceText);
    if (!bAutoMap && (TargetText.IsEmpty() || !TargetChains.Contains(Target) || (!Source.IsNone() && !SourceChains.Contains(Source))))
    {
        Response->SetStringField(TEXT("sourceChains"), JoinRetargetChainNames(SourceChains));
        Response->SetStringField(TEXT("targetChains"), JoinRetargetChainNames(TargetChains));
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Chain not found. targetChain must be one of: %s. sourceChain must be one of: %s (or None to clear)."),
            *JoinRetargetChainNames(TargetChains), *JoinRetargetChainNames(SourceChains)), TEXT("CHAIN_NOT_FOUND"));
    }
    if (bAutoMap && (SourceChains.Num() == 0 || TargetChains.Num() == 0))
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Nothing to map: the source rig has chains %s and the target rig has chains %s. Add retarget chains to both IK Rigs first."),
            *JoinRetargetChainNames(SourceChains), *JoinRetargetChainNames(TargetChains)), TEXT("NO_RETARGET_CHAINS"));
    }
    TMap<FName, FName> Before;
    for (const FName& Chain : TargetChains) { Before.Add(Chain, RetargetSourceChainFor(Controller, Chain)); }
    // From 5.6 chain mappings live on retarget ops; an empty op stack maps nothing, so it gets the default ops,
    // which are removed again if the mapping then fails.
    bool bOpsAdded = false;
#if ENGINE_MINOR_VERSION >= 6
    if (Controller->GetNumRetargetOps() == 0)
    {
        Controller->AddDefaultOps();
        bOpsAdded = true;
    }
#endif
    int32 Mapped = 0;
    if (bAutoMap)
    {
#if ENGINE_MINOR_VERSION >= 2
        Controller->AutoMapChains(EAutoMapChainType::Exact, false);
        Controller->AutoMapChains(EAutoMapChainType::Fuzzy, false);
#else
        Controller->AutoMapChains();
#endif
        for (const FName& Chain : TargetChains) { Mapped += RetargetSourceChainFor(Controller, Chain).IsNone() ? 0 : 1; }
        if (Mapped == 0)
        {
#if ENGINE_MINOR_VERSION >= 6
            if (bOpsAdded) { Controller->RemoveAllOps(); }
#endif
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Auto-mapping matched no chains (source: %s; target: %s). Map them one at a time with targetChain and sourceChain."),
                *JoinRetargetChainNames(SourceChains), *JoinRetargetChainNames(TargetChains)), TEXT("NO_CHAINS_MAPPED"));
        }
    }
    else if (!SetRetargetSourceChain(Controller, Source, Target) || RetargetSourceChainFor(Controller, Target) != Source)
    {
#if ENGINE_MINOR_VERSION >= 6
        if (bOpsAdded) { Controller->RemoveAllOps(); }
#endif
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("The retargeter did not accept %s -> %s."), *Source.ToString(), *Target.ToString()), TEXT("MAPPING_FAILED"));
    }
    bool bChanged = bOpsAdded;
    for (const FName& Chain : TargetChains) { bChanged |= Before[Chain] != RetargetSourceChainFor(Controller, Chain); }
    TArray<TSharedPtr<FJsonValue>> Mapping;
    for (const FName& Chain : TargetChains)
    {
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("targetChain"), Chain.ToString());
        Entry->SetStringField(TEXT("sourceChain"), RetargetSourceChainFor(Controller, Chain).ToString());
        Mapping.Add(MakeShared<FJsonValueObject>(Entry));
    }
    const bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
    const bool bSaved = bSave && SaveAnimAsset(Retargeter, true);
    Response->SetArrayField(TEXT("mapping"), Mapping);
    Response->SetBoolField(TEXT("changed"), bChanged);
    Response->SetBoolField(TEXT("opsAdded"), bOpsAdded);
    Response->SetBoolField(TEXT("saved"), bSaved);
    Response->SetStringField(TEXT("assetPath"), Retargeter->GetPathName());
    ANIM_SUCCESS_RESPONSE(bAutoMap ? FString::Printf(TEXT("Auto-mapped %d of %d target chains"), Mapped, TargetChains.Num()) : FString::Printf(TEXT("%s now drives %s"), *Source.ToString(), *Target.ToString()));
    return Response;
}
#endif

} // namespace McpAnimationAuthoring
