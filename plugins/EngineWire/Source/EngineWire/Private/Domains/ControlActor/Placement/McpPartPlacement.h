#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class AActor;
class UBlueprint;
class UStaticMesh;

// Placement inside one actor. McpPlacement compares whole actors against the level, so a seated
// character's legs buried 12 cm inside the body it sits on, or feet below the actor's own capsule, read as a clean
// placement: both parts belong to the same actor. This measures the actor's mesh parts on their
// real triangles against each other and against the actor's ground.
namespace McpPartPlacement
{
struct FPartIssue
{
    FString Component;
    // "sunk": below the actor's ground (a Character's capsule bottom); "buried": inside another part.
    FString Kind;
    FString Other;
    // World units: how far below the ground, or how deep inside Other.
    double Depth = 0.0;
    // Share of the part's surface samples that lie inside Other (buried only).
    double InsideShare = 0.0;
    FString Issue;
};

struct FPartAudit
{
    int32 Examined = 0;
    bool bHasGround = false;
    double GroundZ = 0.0;
    // Worst first.
    TArray<FPartIssue> Issues;
    // Warning sentences about the Focus parts that are not placement: a part under an unevenly scaled parent.
    TArray<FString> Notes;
    // Set when the Blueprint could not be measured at all.
    FString Error;
};

// Drawing a preview instance, or compiling the Blueprint, sets the usage flags its materials need (Nanite,
// skeletal, instanced), and while a mesh is still loading or building the editor cannot even mark them dirty: a
// Blueprint saved without them left the flag off on disk, so a part only spawned in play drew the default material
// there and in a cooked game. On scope exit, saves the project materials of the Blueprint's parts whose needed flag
// is set but not on disk.
class FMcpPreviewMaterialSaver
{
public:
    explicit FMcpPreviewMaterialSaver(UBlueprint* InBlueprint) : Blueprint(InBlueprint) {}
    ~FMcpPreviewMaterialSaver();

private:
    TWeakObjectPtr<UBlueprint> Blueprint;
};

// Measures a temporary instance of the Blueprint's class in a preview world, so the level is not
// touched. Focus (component names) keeps only issues involving one of them; empty keeps all.
// A part tagged mcp.placement.ok (component tag) is a deliberate embed and is left out.
FPartAudit AuditBlueprintParts(UBlueprint* Blueprint, const TSet<FString>& Focus, double Tolerance = 0.5);

// The same, focused on the parts that draw Mesh: what a replaced mesh did to the Blueprints using it.
FPartAudit AuditBlueprintPartsUsingMesh(UBlueprint* Blueprint, const UStaticMesh* Mesh, double Tolerance = 0.5);

// For a reply that replaced Mesh in place (convert_to_static_mesh / convert_to_nanite onto an existing path): each
// loaded Blueprint that uses it is measured for the parts that draw it, and what sank is added as partWarnings
// (each with blueprintPath) and warnings sentences. Returns the number of issues found.
int32 AppendMeshUserWarnings(UStaticMesh* Mesh, const TSharedPtr<FJsonObject>& Result);

TSharedPtr<FJsonObject> IssueToJson(const FPartIssue& Issue);

// Adds a sentence for each Focus part attached to a parent scaled unevenly in the world: the part takes that scale
// too, so its size and offsets are stretched the same way.
void NoteInheritedScale(AActor* Actor, const TSet<FString>& Focus, TArray<FString>& OutNotes);

// For an edit's reply: partWarnings[] plus one warnings[] sentence per issue involving Focus.
// Returns the number of issues found (the reply lists the worst eight).
int32 AppendPartWarnings(UBlueprint* Blueprint, const TSet<FString>& Focus, const TSharedPtr<FJsonObject>& Result);
int32 AppendPartWarnings(const FString& BlueprintPath, const TSet<FString>& Focus, const TSharedPtr<FJsonObject>& Result);

// audit_placement with blueprintPath: every part of that Blueprint, worst first, in the level
// audit's reply shape (examined, flagged, returned, byKind, problems[]). Null with OutError set
// when the Blueprint cannot be measured.
TSharedPtr<FJsonObject> BuildBlueprintAuditReply(const FString& BlueprintPath, double Tolerance, int32 Limit,
                                                 FString& OutError);
} // namespace McpPartPlacement
