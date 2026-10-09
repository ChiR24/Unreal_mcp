#include "Domains/AnimationAuthoring/ControlRig/McpAutomationBridge_ControlRigGraph.h"

#if MCP_HAS_CONTROLRIG_BLUEPRINT
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#endif

// edit_control_rig's hierarchy edits: the skeleton's bones imported, bones, nulls and controls added under a parent
// with a transform, and elements removed, through the hierarchy controller the Control Rig editor uses.
namespace McpAnimationAuthoring {
#if MCP_HAS_CONTROLRIG_BLUEPRINT
namespace {
// The element called Name: a bone first, then a null, then a control; invalid when none is.
FRigElementKey McpFindRigElement(URigHierarchy* Hierarchy, const FString& Name)
{
    for (const ERigElementType Type : {ERigElementType::Bone, ERigElementType::Null, ERigElementType::Control})
    {
        const FRigElementKey Key(FName(*Name), Type);
        if (Hierarchy && Hierarchy->Contains(Key))
        {
            return Key;
        }
    }
    return FRigElementKey();
}
} // namespace

bool McpEditRigHierarchy(FMcpRigEdit& Edit, const FString& Kind, const TSharedPtr<FJsonObject>& Step, FString& OutError, FString& OutCode)
{
    URigHierarchy* Hierarchy = Edit.Hierarchy->GetHierarchy();
    if (Kind == TEXT("import_bones"))
    {
        const FString SkeletonPath = GetJsonStringField(Step, TEXT("skeletonPath"));
        USkeletalMesh* Preview = Edit.Rig->GetPreviewMesh();
        USkeleton* Skeleton = !SkeletonPath.IsEmpty() ? LoadSkeletonFromPathAnim(SkeletonPath) : Preview ? Preview->GetSkeleton() : nullptr;
        if (!Skeleton)
        {
            return McpRefuseRigEdit(OutError, OutCode, TEXT("No skeleton to import: give skeletonPath, or give the rig a preview mesh."), TEXT("SKELETON_NOT_FOUND"));
        }
        // ImportBones answers only the bones it added; bones the rig already had are matched in place.
        Edit.Hierarchy->ImportBones(Skeleton, NAME_None, true, true, false, true);
        const TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
        Extra->SetNumberField(TEXT("bones"), Skeleton->GetReferenceSkeleton().GetNum());
        McpNoteRigMade(Edit, TEXT("importedFrom"), Skeleton->GetPathName(), Extra);
        return true;
    }
    const FString Name = GetJsonStringField(Step, TEXT("name"));
    if (Name.IsEmpty())
    {
        return McpRefuseRigEdit(OutError, OutCode, FString::Printf(TEXT("%s needs a name."), *Kind), TEXT("INVALID_ARGUMENT"));
    }
    if (Kind == TEXT("remove_element"))
    {
        const FRigElementKey Key = McpFindRigElement(Hierarchy, Name);
        return (Key.IsValid() && Edit.Hierarchy->RemoveElement(Key, true)) ||
               McpRefuseRigEdit(OutError, OutCode, FString::Printf(TEXT("'%s' is not a bone, null or control of the rig."), *Name), TEXT("ELEMENT_NOT_FOUND"));
    }
    const FString ParentName = GetJsonStringField(Step, TEXT("parent"));
    const FRigElementKey Parent = ParentName.IsEmpty() ? FRigElementKey() : McpFindRigElement(Hierarchy, ParentName);
    if (!ParentName.IsEmpty() && !Parent.IsValid())
    {
        return McpRefuseRigEdit(OutError, OutCode, FString::Printf(TEXT("parent '%s' is not a bone, null or control of the rig."), *ParentName), TEXT("ELEMENT_NOT_FOUND"));
    }
    const FTransform Transform(ExtractRotatorField(Step, TEXT("rotation"), FRotator::ZeroRotator),
                               ExtractVectorField(Step, TEXT("location"), FVector::ZeroVector),
                               ExtractVectorField(Step, TEXT("scale"), FVector::OneVector));
    const bool bGlobal = GetJsonBoolField(Step, TEXT("global"), false);
    FRigElementKey Added;
    if (Kind == TEXT("add_bone"))
    {
        Added = Edit.Hierarchy->AddBone(FName(*Name), Parent, Transform, bGlobal, ERigBoneType::User, true);
    }
    else if (Kind == TEXT("add_null"))
    {
        Added = Edit.Hierarchy->AddNull(FName(*Name), Parent, Transform, bGlobal, true);
    }
    else
    {
        const FString Type = GetJsonStringField(Step, TEXT("controlType"), TEXT("transform")).ToLower();
        FRigControlSettings Settings;
        FRigControlValue Value;
        if (Type == TEXT("float")) { Settings.ControlType = ERigControlType::Float; Value = URigHierarchy::MakeControlValueFromFloat(0.f); }
        else if (Type == TEXT("bool")) { Settings.ControlType = ERigControlType::Bool; Value = URigHierarchy::MakeControlValueFromBool(false); }
        else if (Type == TEXT("position")) { Settings.ControlType = ERigControlType::Position; Value = URigHierarchy::MakeControlValueFromVector(FVector::ZeroVector); }
        else if (Type == TEXT("rotator")) { Settings.ControlType = ERigControlType::Rotator; Value = URigHierarchy::MakeControlValueFromRotator(FRotator::ZeroRotator); }
        else if (Type == TEXT("transform")) { Settings.ControlType = ERigControlType::EulerTransform; Value = URigHierarchy::MakeControlValueFromEulerTransform(FEulerTransform::Identity); }
        else
        {
            return McpRefuseRigEdit(OutError, OutCode, FString::Printf(TEXT("controlType '%s' is not transform, float, bool, position or rotator."), *Type), TEXT("INVALID_ARGUMENT"));
        }
        Added = Edit.Hierarchy->AddControl(FName(*Name), Parent, Settings, Value, Transform, FTransform::Identity, true);
    }
    if (!Added.IsValid())
    {
        return McpRefuseRigEdit(OutError, OutCode, FString::Printf(TEXT("%s '%s' was not added."), *Kind, *Name), TEXT("ADD_FAILED"));
    }
    McpNoteRigMade(Edit, TEXT("element"), Added.Name.ToString());
    return true;
}
#endif

} // namespace McpAnimationAuthoring
