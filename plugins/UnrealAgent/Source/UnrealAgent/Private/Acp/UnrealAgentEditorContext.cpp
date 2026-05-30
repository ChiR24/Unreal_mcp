#include "UnrealAgentEditorContext.h"

#include "UnrealAgentEvidenceLedger.h"
#include "UnrealAgentStudioKit.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace
{
    FString NormalizeEditorContextProjectDirectory(const FString& ProjectDirectory)
    {
        FString Normalized = FPaths::ConvertRelativePathToFull(ProjectDirectory.IsEmpty() ? FPaths::ProjectDir() : ProjectDirectory);
        FPaths::NormalizeDirectoryName(Normalized);
        return Normalized;
    }

    void AppendLine(FString& Text, const FString& Line)
    {
        Text += Line;
        Text += LINE_TERMINATOR;
    }

    FString GetCurrentMapName()
    {
        if (GEditor == nullptr)
        {
            return TEXT("unavailable");
        }

        UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
        if (EditorWorld == nullptr)
        {
            return TEXT("unavailable");
        }

        FString MapName = EditorWorld->GetMapName();
        if (EditorWorld->StreamingLevelsPrefix.Len() > 0)
        {
            MapName.RemoveFromStart(EditorWorld->StreamingLevelsPrefix);
        }
        return MapName.IsEmpty() ? TEXT("untitled") : MapName;
    }

    int32 CountDirtyPackages()
    {
        TArray<UPackage*> DirtyContentPackages;
        TArray<UPackage*> DirtyWorldPackages;
        FEditorFileUtils::GetDirtyContentPackages(DirtyContentPackages);
        FEditorFileUtils::GetDirtyWorldPackages(DirtyWorldPackages);
        return DirtyContentPackages.Num() + DirtyWorldPackages.Num();
    }

    TArray<FString> GetSelectedActorDescriptions(int32 MaxSelectedActors, int32& OutTotalSelectedActors)
    {
        TArray<FString> Descriptions;
        OutTotalSelectedActors = 0;
        if (GEditor == nullptr || GEditor->GetSelectedActors() == nullptr)
        {
            return Descriptions;
        }

        USelection* SelectedActors = GEditor->GetSelectedActors();
        OutTotalSelectedActors = SelectedActors->Num();
        for (FSelectionIterator It(*SelectedActors); It && Descriptions.Num() < MaxSelectedActors; ++It)
        {
            const AActor* Actor = Cast<AActor>(*It);
            if (Actor == nullptr)
            {
                continue;
            }

            const FString ClassName = Actor->GetClass() != nullptr ? Actor->GetClass()->GetName() : TEXT("UnknownClass");
            Descriptions.Add(FString::Printf(TEXT("- %s (%s) path=%s"), *Actor->GetActorLabel(), *ClassName, *Actor->GetPathName()));
        }
        return Descriptions;
    }

    TArray<FString> GetSelectedObjectDescriptions(int32 MaxSelectedObjects, int32& OutTotalSelectedObjects)
    {
        TArray<FString> Descriptions;
        OutTotalSelectedObjects = 0;
        if (GEditor == nullptr || GEditor->GetSelectedObjects() == nullptr)
        {
            return Descriptions;
        }

        USelection* SelectedObjects = GEditor->GetSelectedObjects();
        OutTotalSelectedObjects = SelectedObjects->Num();
        for (FSelectionIterator It(*SelectedObjects); It && Descriptions.Num() < MaxSelectedObjects; ++It)
        {
            const UObject* Object = Cast<UObject>(*It);
            if (Object == nullptr || Object->IsA<AActor>())
            {
                continue;
            }

            const FString ClassName = Object->GetClass() != nullptr ? Object->GetClass()->GetName() : TEXT("UnknownClass");
            Descriptions.Add(FString::Printf(TEXT("- %s (%s) path=%s"), *Object->GetName(), *ClassName, *Object->GetPathName()));
        }
        return Descriptions;
    }

    FString TruncateEnvelope(FString Envelope, int32 MaxCharacters)
    {
        const int32 SafeMaxCharacters = FMath::Max(1024, MaxCharacters);
        if (Envelope.Len() <= SafeMaxCharacters)
        {
            return Envelope;
        }
        return Envelope.Left(SafeMaxCharacters - 64) + TEXT("\n[truncated editor context]\n</unreal_editor_context>\n");
    }
}

FUnrealAgentEditorContextSnapshot FUnrealAgentEditorContext::Capture(const FString& ProjectDirectory, const FUnrealAgentEditorContextOptions& Options)
{
    FUnrealAgentEditorContextSnapshot Snapshot;
    const FString NormalizedProjectDirectory = NormalizeEditorContextProjectDirectory(ProjectDirectory);
    const FString MapName = GetCurrentMapName();
    Snapshot.bPieActive = GEditor != nullptr && GEditor->PlayWorld != nullptr;
    Snapshot.DirtyPackageCount = CountDirtyPackages();

    int32 TotalSelectedActors = 0;
    int32 TotalSelectedObjects = 0;
    const TArray<FString> SelectedActors = GetSelectedActorDescriptions(Options.MaxSelectedActors, TotalSelectedActors);
    const TArray<FString> SelectedObjects = GetSelectedObjectDescriptions(Options.MaxSelectedObjects, TotalSelectedObjects);
    Snapshot.SelectedActorCount = TotalSelectedActors;
    Snapshot.SelectedObjectCount = TotalSelectedObjects;

    const FUnrealAgentEvidenceSummary EvidenceSummary = FUnrealAgentEvidenceLedger::LoadSummary(NormalizedProjectDirectory);

    FString Envelope;
    AppendLine(Envelope, TEXT("<unreal_editor_context version=\"1\">"));
    AppendLine(Envelope, FString::Printf(TEXT("generatedAt: %s"), *FDateTime::UtcNow().ToIso8601()));
    AppendLine(Envelope, FString::Printf(TEXT("projectName: %s"), FApp::GetProjectName()));
    AppendLine(Envelope, TEXT("projectDir: [redacted project root]"));
    AppendLine(Envelope, FString::Printf(TEXT("engineVersion: %s"), *FEngineVersion::Current().ToString()));
    AppendLine(Envelope, FString::Printf(TEXT("currentMap: %s"), *MapName));
    AppendLine(Envelope, FString::Printf(TEXT("pieActive: %s"), Snapshot.bPieActive ? TEXT("true") : TEXT("false")));
    AppendLine(Envelope, FString::Printf(TEXT("dirtyPackages: %d"), Snapshot.DirtyPackageCount));
    AppendLine(Envelope, FString::Printf(TEXT("selectedActors: %d"), TotalSelectedActors));
    for (const FString& SelectedActor : SelectedActors)
    {
        AppendLine(Envelope, SelectedActor);
    }
    if (TotalSelectedActors > SelectedActors.Num())
    {
        AppendLine(Envelope, FString::Printf(TEXT("- ... %d more selected actors omitted"), TotalSelectedActors - SelectedActors.Num()));
    }
    AppendLine(Envelope, FString::Printf(TEXT("selectedObjects: %d"), TotalSelectedObjects));
    for (const FString& SelectedObject : SelectedObjects)
    {
        AppendLine(Envelope, SelectedObject);
    }
    if (TotalSelectedObjects > SelectedObjects.Num())
    {
        AppendLine(Envelope, FString::Printf(TEXT("- ... %d more selected objects omitted"), TotalSelectedObjects - SelectedObjects.Num()));
    }
    AppendLine(Envelope, FString::Printf(TEXT("evidenceLedgerEntries: %d"), EvidenceSummary.EvidenceCount));
    if (!EvidenceSummary.LastSummary.IsEmpty())
    {
        AppendLine(Envelope, FString::Printf(TEXT("latestEvidenceSummary: %s"), *EvidenceSummary.LastSummary));
    }
    AppendLine(Envelope, TEXT("rules: Treat this as a fast editor snapshot. Confirm stale or high-impact facts with MCP inspect before changing assets, actors, maps, Blueprints, settings, tests, or packaging."));
    AppendLine(Envelope, TEXT("privacy: Sensitive credential values are redacted before prompt injection."));
    AppendLine(Envelope, TEXT("</unreal_editor_context>"));

    Snapshot.Envelope = TruncateEnvelope(FUnrealAgentStudioKit::RedactSensitiveText(Envelope), Options.MaxCharacters);
    Snapshot.Summary = FString::Printf(
        TEXT("Editor context: map=%s, PIE=%s, selection=%d actors/%d objects, dirty=%d, evidence=%d"),
        *MapName,
        Snapshot.bPieActive ? TEXT("active") : TEXT("inactive"),
        Snapshot.SelectedActorCount,
        Snapshot.SelectedObjectCount,
        Snapshot.DirtyPackageCount,
        EvidenceSummary.EvidenceCount);
    return Snapshot;
}
