#include "Domains/AssetWorkflow/Structs/McpAssetStructsShared.h"
#include "UObject/UnrealType.h"
#include <functional>


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
static bool DetectRecursiveStructRef(UEngineWireSubsystem& Bridge, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, const FString& RequestId, const FEdGraphPinType& Pin, UUserDefinedStruct* Self, const FString& MemberName)
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


// The pin type memberType names. Refused, with false, when it does not resolve or would put the struct inside itself:
// as the member's type, as a container's element, or through another struct that holds it.
static bool ResolveMemberTypeOrReply(UEngineWireSubsystem& Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UUserDefinedStruct* S, const FString& StructPath, const FString& MemberType, const FString& MemberName, FEdGraphPinType& OutPinType)
{
    const McpBlueprintUtils::FTypeResolutionResult Resolved = McpBlueprintUtils::ResolvePinType(MemberType, FName(*StructPath));
    if (!Resolved.bSuccess)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Type '%s' rejected: %s"), *MemberType, *Resolved.OutError),
            TEXT("TYPE_RESOLUTION_FAILED"));
        return false;
    }
    OutPinType = Resolved.PinType;
    if (OutPinType.PinSubCategoryObject.Get() == static_cast<UObject*>(S) || OutPinType.PinValueType.TerminalSubCategoryObject.Get() == static_cast<UObject*>(S))
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("Struct cannot reference itself as a member type"), TEXT("SELF_REFERENCE"));
        return false;
    }
    return !DetectRecursiveStructRef(Bridge, RequestingSocket, RequestId, OutPinType, S, MemberName);
}


bool HandleStructMemberAddRemoveActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();

    // add_struct_member
    if (Lower == TEXT("add_struct_member"))
    {
        // The contract declares `members`, so honour it: one call per field
        // turned a ten-field struct into ten round trips.
        if (AddStructMembersFromArray(Bridge, RequestId, Payload, RequestingSocket)) return true;

        FString StructPath = GetJsonStringField(Payload, TEXT("structPath"));
        FString MemberType = GetJsonStringField(Payload, TEXT("memberType"));
        FString MemberName = GetJsonStringField(Payload, TEXT("memberName"));
        bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);

        if (StructPath.IsEmpty() || MemberType.IsEmpty() || MemberName.IsEmpty())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: structPath, memberType or memberName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UUserDefinedStruct* S = LoadStructOrReply(Bridge, RequestId, RequestingSocket, StructPath);
        if (!S)
        {
            return true;
        }

        FEdGraphPinType PinType;
        if (!ResolveMemberTypeOrReply(Bridge, RequestId, RequestingSocket, S, StructPath, MemberType, MemberName, PinType))
        {
            return true;
        }

        if (!FStructureEditorUtils::AddVariable(S, PinType))
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Failed to add struct member"), TEXT("MEMBER_ADD_FAILED"));
            return true;
        }

        const FGuid G = FStructureEditorUtils::GetVarDesc(S).Last().VarGuid;
        FStructureEditorUtils::RenameVariable(S, G, MemberName);

        FString DefaultValue = GetJsonStringField(Payload, TEXT("defaultValue"));
        if (!DefaultValue.IsEmpty())
        {
            FStructureEditorUtils::ChangeVariableDefaultValue(S, G, DefaultValue);
        }
        ApplyStructMemberTooltipAndMetadata(S, G, Payload);

        FinishStructEdit(S, bSave);

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
        FString StructPath = GetJsonStringField(Payload, TEXT("structPath"));
        bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);

        UUserDefinedStruct* S = LoadStructOrReply(Bridge, RequestId, RequestingSocket, StructPath);
        if (!S)
        {
            return true;
        }

        FGuid G;
        if (!ResolveMemberOrReply(Bridge, RequestId, RequestingSocket, S, Payload, G))
        {
            return true;
        }

        FStructureEditorUtils::RemoveVariable(S, G);
        FinishStructEdit(S, bSave);

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
        FString StructPath = GetJsonStringField(Payload, TEXT("structPath"));
        FString NewMemberName = GetJsonStringField(Payload, TEXT("newMemberName"));
        bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);

        if (StructPath.IsEmpty() || NewMemberName.IsEmpty())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: structPath or newMemberName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        // Sanitize only the new name (the lookup key must match the existing member exactly).
        NewMemberName = SanitizeAssetName(NewMemberName);

        UUserDefinedStruct* S = LoadStructOrReply(Bridge, RequestId, RequestingSocket, StructPath);
        if (!S)
        {
            return true;
        }

        FGuid G;
        if (!ResolveMemberOrReply(Bridge, RequestId, RequestingSocket, S, Payload, G))
        {
            return true;
        }

        FStructureEditorUtils::RenameVariable(S, G, NewMemberName);
        FinishStructEdit(S, bSave);

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
        FString StructPath = GetJsonStringField(Payload, TEXT("structPath"));
        FString MemberName = GetJsonStringField(Payload, TEXT("memberName"));
        FString MemberType = GetJsonStringField(Payload, TEXT("memberType"));
        bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);

        if (StructPath.IsEmpty() || MemberType.IsEmpty())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: structPath or memberType"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UUserDefinedStruct* S = LoadStructOrReply(Bridge, RequestId, RequestingSocket, StructPath);
        if (!S)
        {
            return true;
        }

        FGuid G;
        if (!ResolveMemberOrReply(Bridge, RequestId, RequestingSocket, S, Payload, G))
        {
            return true;
        }

        FEdGraphPinType PinType;
        if (!ResolveMemberTypeOrReply(Bridge, RequestId, RequestingSocket, S, StructPath, MemberType, MemberName, PinType))
        {
            return true;
        }

        FStructureEditorUtils::ChangeVariableType(S, G, PinType);
        FinishStructEdit(S, bSave);

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

