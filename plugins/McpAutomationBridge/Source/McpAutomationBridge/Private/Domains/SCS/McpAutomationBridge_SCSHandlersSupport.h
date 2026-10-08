#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UBlueprint;
class USCS_Node;
class USimpleConstructionScript;

namespace McpSCSHandlers {

// Marks Result failed with Error/Code and returns it, for `return SCSFail(Result, ...)`.
TSharedPtr<FJsonObject> SCSFail(TSharedPtr<FJsonObject> Result, const FString &Error, const TCHAR *Code);
// The Blueprint at BlueprintPath; null after marking Result ASSET_NOT_FOUND, or SCS_NOT_FOUND when bRequireScs and it
// has no SimpleConstructionScript.
UBlueprint *LoadScsBlueprint(const FString &BlueprintPath, const TSharedPtr<FJsonObject> &Result, bool bRequireScs = true);
bool IsPlayInEditorActive();
TSharedPtr<FJsonObject> PIEActiveError();
FString GetSCSNodeName(const USCS_Node *Node);
USCS_Node *FindSCSNodeByVariableName(USimpleConstructionScript *SCS,
                                     const FString &Name);
USCS_Node *FindSCSParentNode(USimpleConstructionScript *SCS,
                             USCS_Node *ChildNode);
bool IsSCSRootAlias(const FString &Name);
bool IsSCSRootNode(USimpleConstructionScript *SCS, USCS_Node *Node);
TSharedPtr<FJsonObject> MakeTransformJson(const FTransform &Transform);
void AddSCSNodeVerification(TSharedPtr<FJsonObject> Result,
                            USimpleConstructionScript *SCS, USCS_Node *Node);
bool SCSParentMatches(USimpleConstructionScript *SCS, USCS_Node *Node,
                      const FString &ExpectedParentName);
// A level-placed actor with a BuoyancyComponent only learns it starts in water from the water body's begin-overlap,
// which level load sends only when the actor sets bGenerateOverlapEventsDuringLevelStreaming; without it buoyancy never
// started and every placed floater sank. Sets the flag on the class default and on placed actors when the
// Blueprint's components include a BuoyancyComponent (the water tool's own buoyancy path sets it on its actor).
void EnableLoadOverlapsForBuoyancy(UBlueprint *Blueprint);

}
