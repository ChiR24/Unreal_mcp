#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"
#include "UObject/UnrealType.h"
#include <functional>

#if WITH_EDITOR

// Detects whether Target is reachable from Start by walking member property
// types (struct fields and container element/value props), guarding cycles with
// an already-visited set. Object properties reference a UClass, never a
// UUserDefinedStruct, so they cannot form a struct cycle and are skipped.
static bool ReferencesStruct(const UUserDefinedStruct* Start, const UUserDefinedStruct* Target, TSet<const UUserDefinedStruct*>& Visited)
{
    if (Start == Target) { return true; }
    if (!Start || Visited.Contains(Start)) { return false; }
    Visited.Add(Start);
    bool bFound = false;
    std::function<void(const FProperty*)> Check = [&](const FProperty* P)
    {
        if (bFound || !P) { return; }
        if (const FStructProperty* SP = CastField<FStructProperty>(P))
        {
            if (const UUserDefinedStruct* U = Cast<UUserDefinedStruct>(SP->Struct))
                if (ReferencesStruct(U, Target, Visited)) { bFound = true; }
        }
        else if (const FArrayProperty* AP = CastField<FArrayProperty>(P)) { Check(AP->Inner); }
        else if (const FSetProperty* SP2 = CastField<FSetProperty>(P)) { Check(SP2->ElementProp); }
        else if (const FMapProperty* MP = CastField<FMapProperty>(P)) { Check(MP->KeyProp); Check(MP->ValueProp); }
    };
    for (TFieldIterator<FProperty> It(Start); It && !bFound; ++It) { Check(*It); }
    return bFound;
}

// Rejects (c) indirect recursive (cyclic) reference: if the member's resolved
// type (base or container element/value) transitively references SelfStruct.
// Direct/container element self-reference (a)/(b) is handled by the caller's
// existing SELF_REFERENCE check. Returns true (and sends the error) when rejected.
static bool DetectRecursiveStructRef(UMcpAutomationBridgeSubsystem& Bridge, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, const FString& RequestId, const FEdGraphPinType& Pin, UUserDefinedStruct* Self, const FString& MemberName)
{
    const UUserDefinedStruct* Base = Cast<UUserDefinedStruct>(Pin.PinSubCategoryObject.Get());
    const UUserDefinedStruct* Elem = Cast<UUserDefinedStruct>(Pin.PinValueType.TerminalSubCategoryObject.Get());
    TSet<const UUserDefinedStruct*> Visited;
    if ((Base && ReferencesStruct(Base, Self, Visited)) || (Elem && ReferencesStruct(Elem, Self, Visited)))
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Struct '%s' would create a recursive (cyclic) reference through member '%s'"), *Self->GetName(), *MemberName), TEXT("RECURSIVE_REFERENCE")); return true;
    }
    return false;
}



bool HandleStructMemberAddRemoveActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();

    // add_struct_member
    if (Lower == TEXT("add_struct_member"))
    {
        FString StructPath = GetPayloadString(Payload, TEXT("structPath"));
        FString MemberType = GetPayloadString(Payload, TEXT("memberType"));
        FString MemberName = GetPayloadString(Payload, TEXT("memberName"));
        bool bSave = GetPayloadBool(Payload, TEXT("save"), false);

        if (StructPath.IsEmpty() || MemberType.IsEmpty() || MemberName.IsEmpty())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: structPath, memberType or memberName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UUserDefinedStruct* S = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
        if (!S)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Struct not found: %s"), *StructPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        const FName SelfStructPath(*StructPath);
        const McpBlueprintUtils::FTypeResolutionResult Resolved =
            McpBlueprintUtils::ResolvePinType(MemberType, SelfStructPath);
        if (!Resolved.bSuccess)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Type '%s' rejected: %s"), *MemberType, *Resolved.OutError),
                TEXT("TYPE_RESOLUTION_FAILED"));
            return true;
        }
        const FEdGraphPinType PinType = Resolved.PinType;

        UObject* Sub = PinType.PinSubCategoryObject.Get();
        if (Sub == static_cast<UObject*>(S) || PinType.PinValueType.TerminalSubCategoryObject.Get() == static_cast<UObject*>(S))
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("Struct cannot reference itself as a member type"), TEXT("SELF_REFERENCE")); return true;
        }

        if (DetectRecursiveStructRef(Bridge, RequestingSocket, RequestId, PinType, S, MemberName)) return true;

        if (!FStructureEditorUtils::AddVariable(S, PinType))
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Failed to add struct member"), TEXT("MEMBER_ADD_FAILED"));
            return true;
        }

        const FGuid G = FStructureEditorUtils::GetVarDesc(S).Last().VarGuid;
        FStructureEditorUtils::RenameVariable(S, G, MemberName);

        FString DefaultValue = GetPayloadString(Payload, TEXT("defaultValue"));
        if (!DefaultValue.IsEmpty())
        {
            FStructureEditorUtils::ChangeVariableDefaultValue(S, G, DefaultValue);
        }

        FStructureEditorUtils::CompileStructure(S);
        S->GetOutermost()->MarkPackageDirty();
        if (bSave)
        {
            McpSafeAssetSave(S);
        }

        // Auto-trigger dependent refresh after the member-add mutation (issue #510).
        McpRefreshStructDependents(S);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("varGuid"), G.ToString());
        Result->SetStringField(TEXT("memberName"), MemberName);
        McpHandlerUtils::AddVerification(Result, S);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct member added"), Result);
        return true;
    }

    // remove_struct_member
    if (Lower == TEXT("remove_struct_member"))
    {
        FString StructPath = GetPayloadString(Payload, TEXT("structPath"));
        FString VarGuidStr = GetPayloadString(Payload, TEXT("varGuid"));
        FString MemberName = GetPayloadString(Payload, TEXT("memberName"));
        bool bSave = GetPayloadBool(Payload, TEXT("save"), false);

        if (StructPath.IsEmpty())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: structPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UUserDefinedStruct* S = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
        if (!S)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Struct not found: %s"), *StructPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        const FGuid G = ResolveMemberGuid(S, VarGuidStr, MemberName);
        if (!G.IsValid())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Struct member not found"), TEXT("MEMBER_NOT_FOUND"));
            return true;
        }

        FStructureEditorUtils::RemoveVariable(S, G);
        FStructureEditorUtils::CompileStructure(S);
        S->GetOutermost()->MarkPackageDirty();
        if (bSave)
        {
            McpSafeAssetSave(S);
        }

        // Auto-trigger dependent refresh after the member-remove mutation (issue #510).
        McpRefreshStructDependents(S);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("varGuid"), G.ToString());
        Result->SetBoolField(TEXT("removed"), true);
        McpHandlerUtils::AddVerification(Result, S);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct member removed"), Result);
        return true;
    }

    // rename_struct_member
    if (Lower == TEXT("rename_struct_member"))
    {
        FString StructPath = GetPayloadString(Payload, TEXT("structPath"));
        FString VarGuidStr = GetPayloadString(Payload, TEXT("varGuid"));
        FString MemberName = GetPayloadString(Payload, TEXT("memberName"));
        FString NewMemberName = GetPayloadString(Payload, TEXT("newMemberName"));
        bool bSave = GetPayloadBool(Payload, TEXT("save"), false);

        if (StructPath.IsEmpty() || NewMemberName.IsEmpty())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: structPath or newMemberName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        // Sanitize member names before rename (mirrors the create_struct name policy).
        MemberName = SanitizeAssetName(MemberName); NewMemberName = SanitizeAssetName(NewMemberName);

        UUserDefinedStruct* S = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
        if (!S)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Struct not found: %s"), *StructPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        const FGuid G = ResolveMemberGuid(S, VarGuidStr, MemberName);
        if (!G.IsValid())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Struct member not found"), TEXT("MEMBER_NOT_FOUND"));
            return true;
        }

        FStructureEditorUtils::RenameVariable(S, G, NewMemberName);
        FStructureEditorUtils::CompileStructure(S);
        S->GetOutermost()->MarkPackageDirty();
        if (bSave)
        {
            McpSafeAssetSave(S);
        }

        // Auto-trigger dependent refresh after the member-rename mutation (issue #510).
        McpRefreshStructDependents(S);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("varGuid"), G.ToString());
        Result->SetStringField(TEXT("memberName"), NewMemberName);
        McpHandlerUtils::AddVerification(Result, S);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct member renamed"), Result);
        return true;
    }

    // set_struct_member_type
    if (Lower == TEXT("set_struct_member_type"))
    {
        FString StructPath = GetPayloadString(Payload, TEXT("structPath"));
        FString VarGuidStr = GetPayloadString(Payload, TEXT("varGuid"));
        FString MemberName = GetPayloadString(Payload, TEXT("memberName"));
        FString MemberType = GetPayloadString(Payload, TEXT("memberType"));
        bool bSave = GetPayloadBool(Payload, TEXT("save"), false);

        if (StructPath.IsEmpty() || MemberType.IsEmpty())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: structPath or memberType"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UUserDefinedStruct* S = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
        if (!S)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Struct not found: %s"), *StructPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        const FGuid G = ResolveMemberGuid(S, VarGuidStr, MemberName);
        if (!G.IsValid())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Struct member not found"), TEXT("MEMBER_NOT_FOUND"));
            return true;
        }

        const FName SelfStructPath(*StructPath);
        const McpBlueprintUtils::FTypeResolutionResult Resolved =
            McpBlueprintUtils::ResolvePinType(MemberType, SelfStructPath);
        if (!Resolved.bSuccess)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Type '%s' rejected: %s"), *MemberType, *Resolved.OutError),
                TEXT("TYPE_RESOLUTION_FAILED"));
            return true;
        }
        const FEdGraphPinType PinType = Resolved.PinType;

        UObject* Sub = PinType.PinSubCategoryObject.Get();
        if (Sub == static_cast<UObject*>(S) || PinType.PinValueType.TerminalSubCategoryObject.Get() == static_cast<UObject*>(S))
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("Struct cannot reference itself as a member type"), TEXT("SELF_REFERENCE")); return true;
        }

        if (DetectRecursiveStructRef(Bridge, RequestingSocket, RequestId, PinType, S, MemberName)) return true;

        FStructureEditorUtils::ChangeVariableType(S, G, PinType);
        FStructureEditorUtils::CompileStructure(S);
        S->GetOutermost()->MarkPackageDirty();
        if (bSave)
        {
            McpSafeAssetSave(S);
        }

        // Auto-trigger dependent refresh after the member-type mutation (issue #510).
        McpRefreshStructDependents(S);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("varGuid"), G.ToString());
        Result->SetStringField(TEXT("type"), PinTypeToSummary(PinType));
        McpHandlerUtils::AddVerification(Result, S);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct member type changed"), Result);
        return true;
    }

    return false;
}

#endif // WITH_EDITOR
