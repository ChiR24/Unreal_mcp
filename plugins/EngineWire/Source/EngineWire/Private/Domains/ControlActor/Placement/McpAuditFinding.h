#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

class AActor;
class UWorld;

// One actor's audit_placement finding: its worst problem (kind, issue, severity in world units) and the lists that go
// with it. One per actor, so flagged counts actors rather than complaints.
struct FMcpPlacementFinding
{
    FString ActorName;
    FString Kind;
    FString Issue;
    double Severity = 0.0;
    bool bHasSuggestedZ = false;
    double SuggestedZ = 0.0;
    TArray<TSharedPtr<FJsonValue>> CoplanarFaces;
    TArray<TSharedPtr<FJsonValue>> Overlaps;
    TArray<TSharedPtr<FJsonValue>> ContentIssues;
    TArray<FString> ContentKinds;
};

namespace McpContentAudit
{
// One thing an actor shows unfinished: its kind (default_material, placeholder_text, duplicate), the component showing
// it (empty for the whole actor), the sentence naming it and its fix, and the size in cm of what shows it.
struct FIssue
{
    FString Kind;
    FString Component;
    FString Issue;
    double Size = 0.0;
};

/** Adds what Actor's visible parts show unfinished: a slot on the default grid material, a text still reading "Text". */
void FindUnfinished(const AActor* Actor, TArray<FIssue>& Out);
/** Another actor drawing the same meshes at the same transform as Actor (a copy pasted onto it), or null. */
const AActor* FindTwin(const AActor* Actor);
/** Where Actor's parts stand and what they draw, as two hashes that change when it moves or redraws. */
void Fingerprint(const AActor* Actor, uint32& OutWhere, uint32& OutDrawn);
/** The duplicate kind's sentence for Copy drawn on top of Original. */
FString DuplicateIssue(const AActor* Copy, const AActor* Original);

// Folds into Findings what each actor shows that was never finished: default_material (a visible mesh slot drawing the
// engine's default grid), placeholder_text (a text render still reading the engine default "Text") and duplicate (an
// exact copy of another actor in the same place, drawn twice). FindingIndex gains every actor it adds a finding for.
void MergeContent(UWorld* World, const FString& NameFilter, TArray<FMcpPlacementFinding>& Findings,
                  TMap<const AActor*, int32>& FindingIndex);
}
