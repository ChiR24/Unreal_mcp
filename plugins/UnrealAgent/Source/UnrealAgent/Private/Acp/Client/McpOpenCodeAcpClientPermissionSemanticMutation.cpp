#include "Acp/Client/McpOpenCodeAcpClientPermissionSemantics.h"

namespace UnrealAgent::OpenCodeAcp::PermissionSemantics
{
bool HasUnrealSemanticMutation(const FString& Value)
{
    const FString LowerValue = Value.ToLower().TrimStartAndEnd();
    FString CompactValue;
    CompactValue.Reserve(LowerValue.Len());
    for (const TCHAR Character : LowerValue)
    {
        if (FChar::IsAlnum(Character) || Character == TEXT('_'))
        {
            CompactValue.AppendChar(Character);
        }
    }
    if (HasInterpreterSemanticMutation(LowerValue, CompactValue)
        || HasShellPathExpansionMutation(LowerValue))
    {
        return true;
    }

    const TCHAR* UnrealPythonMutationMarkers[] = {
        TEXT(".add_"), TEXT(".attach_"), TEXT(".create_"), TEXT(".delete_"),
        TEXT(".destroy_"), TEXT(".detach_"), TEXT(".duplicate_"), TEXT(".execute_"),
        TEXT(".import_"), TEXT(".load_"), TEXT(".move_"), TEXT(".open_"),
        TEXT(".remove_"), TEXT(".rename_"), TEXT(".replace_"), TEXT(".save_"),
        TEXT(".set_"), TEXT(".spawn_"), TEXT(".start_"), TEXT(".stop_"),
        TEXT(".update_")
    };
    if (LowerValue.Contains(TEXT("unreal.")))
    {
        for (const TCHAR* Marker : UnrealPythonMutationMarkers)
        {
            if (LowerValue.Contains(Marker))
            {
                return true;
            }
        }
    }

    const TCHAR* MutationVerbs[] = {
        TEXT("add "), TEXT("attach "), TEXT("change "), TEXT("create "), TEXT("delete "),
        TEXT("destroy "), TEXT("detach "), TEXT("load "), TEXT("modify "), TEXT("move "),
        TEXT("open "), TEXT("remove "), TEXT("rename "), TEXT("replace "), TEXT("save "),
        TEXT("set "), TEXT("spawn "), TEXT("update ")
    };
    const TCHAR* UnrealTargets[] = {
        TEXT(" actor"), TEXT(" asset"), TEXT(" blueprint"), TEXT(" component"),
        TEXT(" content browser"), TEXT(" level"), TEXT(" map"), TEXT(" material"),
        TEXT(" niagara"), TEXT(" outliner"), TEXT(" sequence"), TEXT(" viewport"),
        TEXT(" world")
    };

    bool bHasMutationVerb = false;
    for (const TCHAR* Verb : MutationVerbs)
    {
        if (LowerValue.StartsWith(Verb)
            || LowerValue.Contains(FString(TEXT(" ")) + Verb))
        {
            bHasMutationVerb = true;
            break;
        }
    }
    if (!bHasMutationVerb)
    {
        return false;
    }
    for (const TCHAR* Target : UnrealTargets)
    {
        if (LowerValue.Contains(Target))
        {
            return true;
        }
    }
    return false;
}
}
