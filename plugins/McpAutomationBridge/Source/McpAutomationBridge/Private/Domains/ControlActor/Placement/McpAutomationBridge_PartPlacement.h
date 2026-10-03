#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

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
    // Set when the Blueprint could not be measured at all.
    FString Error;
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
