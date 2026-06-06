#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class USCS_Node;
class USimpleConstructionScript;

namespace McpSCSHandlers {

#if WITH_EDITOR
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
#endif

#if !WITH_EDITOR
TSharedPtr<FJsonObject> UnsupportedSCSAction();
#endif

}
