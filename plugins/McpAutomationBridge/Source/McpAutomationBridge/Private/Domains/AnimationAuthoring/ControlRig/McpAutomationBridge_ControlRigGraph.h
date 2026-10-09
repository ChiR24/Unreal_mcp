#pragma once

#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"

// Control Rig graphs and hierarchies: edit_control_rig, get_control_rig and list_rig_units. A rig's logic lives in its
// RigVM graph (units wired pin to pin) and its bones, nulls and controls in its hierarchy; both are edited through the
// controllers the Control Rig editor itself uses, so every edit can be undone and compiles like a hand-made one.
namespace McpAnimationAuthoring {

// edit_control_rig, get_control_rig and list_rig_units; null for any other action.
TSharedPtr<FJsonObject> HandleControlRigGraphActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);

#if MCP_HAS_CONTROLRIG_BLUEPRINT
// The Control Rig at assetPath, or null with OutError set.
UControlRigBlueprint* McpLoadControlRig(const FString& AssetPath, FString& OutError);
// A node's pins as {name, direction, type, default, linked}; with bSubPins, struct members too, by full pin path.
TArray<TSharedPtr<FJsonValue>> McpDescribeRigPins(const class URigVMNode* Node, bool bSubPins);
// The unit struct a name stands for: an object path, a struct name with or without its RigUnit_/RigVMFunction_
// prefix, or a display name.
UScriptStruct* McpFindRigUnitStruct(const FString& Name);
// edit_control_rig: one edit or a batch of steps, then compile and save.
TSharedPtr<FJsonObject> HandleEditControlRig(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response);

// The rig an edit_control_rig call works on, and what its edits made so a later step or call can name it.
struct FMcpRigEdit
{
    UControlRigBlueprint* Rig = nullptr;
    class URigVMController* Graph = nullptr;
    class URigHierarchyController* Hierarchy = nullptr;
    TArray<TSharedPtr<FJsonValue>> Made;
};
// Adds {Field: Name} (with Extra's fields) to what the edits made.
void McpNoteRigMade(FMcpRigEdit& Edit, const TCHAR* Field, const FString& Name, const TSharedPtr<FJsonObject>& Extra = nullptr);
// Sets OutError and OutCode, and returns false.
bool McpRefuseRigEdit(FString& OutError, FString& OutCode, const FString& Error, const TCHAR* Code);
// import_bones, add_bone, add_null, add_control and remove_element.
bool McpEditRigHierarchy(FMcpRigEdit& Edit, const FString& Kind, const TSharedPtr<FJsonObject>& Step, FString& OutError, FString& OutCode);
#endif

} // namespace McpAnimationAuthoring
