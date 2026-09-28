#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Safety/McpSafeOperations.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/LevelStreamingDynamic.h"
#include "McpAutomationBridgeLog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Engine/World.h"

namespace LevelStructureHelpers
{

namespace
{
// Strip a trailing ".umap" and a trailing ".ObjectName" so
// "/Game/Maps/Foo.Foo", "/Game/Maps/Foo.umap" and "/Game/Maps/Foo" compare
// equal. Only the last segment is considered: a dot inside a directory name
// ("/Game/Maps.v2/Arena") is part of the path.
FString NormalizeLevelPath(const FString& In)
{
    FString Path = In.TrimStartAndEnd();
    Path.RemoveFromEnd(TEXT(".umap"), ESearchCase::IgnoreCase);
    int32 LastSlash = INDEX_NONE;
    Path.FindLastChar(TEXT('/'), LastSlash);
    const int32 Dot = Path.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart, LastSlash + 1);
    if (Dot != INDEX_NONE)
    {
        Path.LeftInline(Dot);
    }
    return Path;
}
} // namespace

ULevel* ResolveTargetLevelForBlueprintRequest(
    UWorld* World, const TSharedPtr<FJsonObject>& Payload, FString& OutError, bool bAllowTransient)
{
    OutError.Reset();
    if (!World)
    {
        OutError = TEXT("No editor world available");
        return nullptr;
    }

    FString Requested;
    if (Payload.IsValid())
    {
        if (!Payload->TryGetStringField(TEXT("levelPath"), Requested) || Requested.IsEmpty())
        {
            Payload->TryGetStringField(TEXT("level"), Requested);
        }
    }
    const FString NormalizedRequested = NormalizeLevelPath(Requested);

    ULevel* Target = nullptr;
    if (!NormalizedRequested.IsEmpty())
    {
        // Match the requested package against every loaded level (persistent
        // and streaming sublevels) so an explicit levelPath is honoured instead
        // of silently editing whatever level happens to be open.
        for (ULevel* Level : World->GetLevels())
        {
            if (Level && NormalizeLevelPath(Level->GetOutermost()->GetName())
                             .Equals(NormalizedRequested, ESearchCase::IgnoreCase))
            {
                Target = Level;
                break;
            }
        }
        if (!Target)
        {
            OutError = FString::Printf(
                TEXT("Level '%s' is not open in the editor (currently editing '%s'). "
                     "Open it first with manage_level load, or omit levelPath to target the "
                     "level that is open."),
                *Requested, *World->GetOutermost()->GetName());
            return nullptr;
        }
    }
    else
    {
        // No explicit target: the level blueprint belongs to the persistent
        // level, not to whatever sublevel is selected in the Levels panel.
        Target = World->PersistentLevel;
        if (!Target)
        {
            OutError = TEXT("No persistent level available");
            return nullptr;
        }
    }

    // A transient (/Temp/) level cannot host a durable level blueprint: edits
    // report success, then vanish when the throwaway world is replaced. Refuse
    // a MUTATING request instead of writing into a level that will never be
    // saved. Opening or reading the blueprint writes nothing, so the open path
    // passes bAllowTransient and keeps working on an unsaved map.
    if (!bAllowTransient)
    {
        const FString TargetPackage = Target->GetOutermost()->GetName();
        if (TargetPackage.StartsWith(TEXT("/Temp/")))
        {
            OutError = FString::Printf(
                TEXT("The open level is a transient '/Temp/' level ('%s'), which has no durable "
                     "level blueprint. Open a saved level with manage_level load before editing its "
                     "level blueprint."),
                *TargetPackage);
            return nullptr;
        }
    }

    return Target;
}

UClass* ResolveLevelStreamingClass(const FString& StreamingMethod)
{
    if (StreamingMethod.Equals(TEXT("Blueprint"), ESearchCase::IgnoreCase))
    {
        return ULevelStreamingDynamic::StaticClass();
    }
    if (StreamingMethod.Equals(TEXT("AlwaysLoaded"), ESearchCase::IgnoreCase))
    {
        return ULevelStreamingAlwaysLoaded::StaticClass();
    }
    return nullptr;
}

void SendLevelEditResult(
    UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
    const TSharedPtr<FJsonObject>& Payload, ULevel* Level, const FString& Message, TSharedPtr<FJsonObject> Result)
{
    if (!Result.IsValid())
    {
        Result = McpHandlerUtils::CreateResultObject();
    }
    if (GetJsonBoolField(Payload, TEXT("save"), false))
    {
        const FString PackageName = Level ? Level->GetOutermost()->GetName() : FString();
        if (PackageName.IsEmpty() || PackageName.StartsWith(TEXT("/Temp/")))
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                TEXT("The change was made, but save was requested on an unsaved level; save the level to a /Game path with manage_level first."),
                Result, TEXT("SAVE_FAILED"));
            return;
        }
        if (!McpSafeLevelSave(Level, PackageName))
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("The change was made, but saving level %s failed; it exists only in this editor session."), *PackageName),
                Result, TEXT("SAVE_FAILED"));
            return;
        }
        Result->SetBoolField(TEXT("saved"), true);
    }
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, Result);
}

ULevelStreaming* FindOrAddStreamingLevel(UWorld* World, const FString& LevelName)
{
    // Package name, object path or short name, case-insensitively (dogfood #159: create_sublevel reported the
    // level under /Game/.../Sub but the configurators could not find it).
    FString WantedPackage = LevelName;
    int32 DotIndex = INDEX_NONE;
    if (WantedPackage.FindChar(TEXT('.'), DotIndex)) { WantedPackage.LeftInline(DotIndex); }
    const FString WantedShort = FPackageName::GetShortName(WantedPackage);
    for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
    {
        const FString PackageName = StreamingLevel ? StreamingLevel->GetWorldAssetPackageName() : FString();
        if (StreamingLevel && (PackageName.Equals(WantedPackage, ESearchCase::IgnoreCase) ||
                               FPackageName::GetShortName(PackageName).Equals(WantedShort, ESearchCase::IgnoreCase)))
        {
            return StreamingLevel;
        }
    }
    // A sublevel that exists on disk but is not streamed by this world yet.
    TArray<FString> Candidates;
    if (LevelName.StartsWith(TEXT("/Game/")))
    {
        Candidates.Add(LevelName);
    }
    Candidates.Add(FPaths::GetPath(World->GetOutermost()->GetName()) / LevelName);
    Candidates.Add(FString(TEXT("/Game/")) / LevelName);
    for (const FString& Candidate : Candidates)
    {
        if (!Candidate.EndsWith(TEXT(".umap")) && FPackageName::DoesPackageExist(Candidate))
        {
            ULevelStreamingDynamic* NewStreamingLevel = NewObject<ULevelStreamingDynamic>(World, ULevelStreamingDynamic::StaticClass());
            NewStreamingLevel->SetWorldAssetByPackageName(FName(*Candidate));
            NewStreamingLevel->LevelTransform = FTransform::Identity;
            NewStreamingLevel->SetShouldBeVisible(true);
            NewStreamingLevel->SetShouldBeLoaded(true);
            World->AddStreamingLevel(NewStreamingLevel);
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Created streaming reference for existing level: %s"), *Candidate);
            return NewStreamingLevel;
        }
    }
    return nullptr;
}

} // namespace LevelStructureHelpers
