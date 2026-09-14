#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/Level.h"
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

UWorld* GetEditorWorld()
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

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

}
#endif
