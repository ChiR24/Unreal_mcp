#include "Domains/MetaHuman/McpAutomationBridge_MetaHumanHandlers.h"

#if WITH_EDITOR
namespace McpMetaHumanHandlers
{
// metahuman_status -- read-only readiness probe.
//
// MetaHuman has three independent prerequisites that each fail in a different
// place: the plugin has to be enabled, "Core Data" has to be installed next to
// the engine, and the character has to be rigged before it can be assembled.
// Discovering them one failed build at a time is expensive, so this answers all
// three at once, and deliberately performs no mutation -- it never registers
// the character for editing, because that is a state change a read must not make.
bool HandleMetaHumanStatus(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, FMetaHumanSocket Socket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    UClass* CharacterClass = FindMetaHumanCharacterClass();
    UObject* Subsystem = FindMetaHumanSubsystem();
    const bool bPluginAvailable = (CharacterClass != nullptr) && (Subsystem != nullptr);
    Result->SetBoolField(TEXT("pluginAvailable"), bPluginAvailable);

    FString CoreDataDetail;
    const bool bCoreData = IsCoreDataInstalled(CoreDataDetail);
    Result->SetBoolField(TEXT("coreDataInstalled"), bCoreData);
    Result->SetStringField(TEXT("coreDataDetail"), CoreDataDetail);

    TArray<FString> Blockers;
    if (!bPluginAvailable)
    {
        Blockers.Add(TEXT("The MetaHumanCharacter plugin is not enabled on this editor (UE 5.6+ only)."));
    }
    if (!bCoreData)
    {
        Blockers.Add(CoreDataDetail);
    }

    // Report the reflected surface so a caller can tell an older MetaHuman from
    // a missing one without a failed call.
    if (Subsystem)
    {
        TArray<TSharedPtr<FJsonValue>> Functions;
        for (TFieldIterator<UFunction> It(Subsystem->GetClass()); It; ++It)
        {
            Functions.Add(MakeShared<FJsonValueString>(It->GetName()));
        }
        Result->SetNumberField(TEXT("reflectedFunctionCount"), Functions.Num());
        Result->SetArrayField(TEXT("reflectedFunctions"), Functions);
    }

    const FString AssetPath = GetJsonStringField(Payload, TEXT("characterPath"));
    if (!AssetPath.IsEmpty())
    {
        Result->SetStringField(TEXT("characterPath"), AssetPath);
        UObject* Character = LoadMetaHumanCharacter(AssetPath);
        Result->SetBoolField(TEXT("characterExists"), Character != nullptr);

        if (Character && Subsystem)
        {
            TSharedPtr<FJsonObject> Results;
            FString Error;
            bool bOpenForEditing = false;
            if (InvokeMetaHumanFunction(Subsystem, TEXT("IsObjectAddedForEditing"),
                    MakeShared<FJsonObject>(), Character, Results, Error) && Results.IsValid())
            {
                Results->TryGetBoolField(TEXT("ReturnValue"), bOpenForEditing);
            }
            Result->SetBoolField(TEXT("openForEditing"), bOpenForEditing);

            // CanBuildMetaHuman is only meaningful once the character is
            // registered for editing, so report it as unknown rather than
            // reporting a false that just means "not open yet".
            if (bOpenForEditing)
            {
                TSharedPtr<FJsonObject> BuildArgs = MakeShared<FJsonObject>();
                BuildArgs->SetBoolField(TEXT("bInLogError"), false);
                bool bCanBuild = false;
                if (InvokeMetaHumanFunction(Subsystem, TEXT("CanBuildMetaHuman"),
                        BuildArgs, Character, Results, Error) && Results.IsValid())
                {
                    Results->TryGetBoolField(TEXT("ReturnValue"), bCanBuild);
                }
                Result->SetBoolField(TEXT("canBuild"), bCanBuild);
                if (!bCanBuild)
                {
                    Blockers.Add(TEXT("The character is not rigged. Run manage_character rig_metahuman first; ")
                        TEXT("auto-rigging is an Epic cloud service and needs the editor signed in to an Epic account."));
                }
            }
            else
            {
                Result->SetStringField(TEXT("canBuild"), TEXT("unknown"));
                Blockers.Add(TEXT("The character is not open for editing, so build readiness is unknown. ")
                    TEXT("Any mutating metahuman action opens it automatically."));
            }
        }
        else if (!Character)
        {
            Blockers.Add(FString::Printf(TEXT("No MetaHuman character asset at %s."), *AssetPath));
        }
    }

    TArray<TSharedPtr<FJsonValue>> BlockerValues;
    for (const FString& Blocker : Blockers)
    {
        BlockerValues.Add(MakeShared<FJsonValueString>(Blocker));
    }
    Result->SetArrayField(TEXT("blockers"), BlockerValues);
    Result->SetBoolField(TEXT("ready"), Blockers.Num() == 0);

    Self->SendAutomationResponse(Socket, RequestId, true,
        Blockers.Num() == 0
            ? TEXT("MetaHuman is ready.")
            : FString::Printf(TEXT("MetaHuman has %d blocker(s)."), Blockers.Num()),
        Result);
    return true;
}
}
#endif
