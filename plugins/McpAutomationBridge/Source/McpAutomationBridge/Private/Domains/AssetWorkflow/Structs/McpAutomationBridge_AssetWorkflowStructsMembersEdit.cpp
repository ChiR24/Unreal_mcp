#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

#if WITH_EDITOR


bool HandleStructMemberEditActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();

    if (Lower == TEXT("reorder_struct_members"))
    {
        FString StructPath = GetPayloadString(Payload, TEXT("structPath"));
        FString VarGuidStr = GetPayloadString(Payload, TEXT("varGuid"));
        FString MemberName = GetPayloadString(Payload, TEXT("memberName"));
        FString RelativeToStr = GetPayloadString(Payload, TEXT("relativeTo"));
        FString Position = GetPayloadString(Payload, TEXT("position"));
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

        const TArray<FStructVariableDescription>& Vars = FStructureEditorUtils::GetVarDesc(S);
        FStructureEditorUtils::EMovePosition MovePos;
        FGuid TargetGuid;

        if (Position == TEXT("first"))
        {
            if (Vars.Num() == 0) { Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("Struct has no members to reorder"), TEXT("INVALID_OPERATION")); return true; }
            TargetGuid = Vars[0].VarGuid;
            MovePos = FStructureEditorUtils::EMovePosition::PositionAbove;
        }
        else if (Position == TEXT("last"))
        {
            if (Vars.Num() == 0) { Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("Struct has no members to reorder"), TEXT("INVALID_OPERATION")); return true; }
            TargetGuid = Vars.Last().VarGuid;
            MovePos = FStructureEditorUtils::EMovePosition::PositionBelow;
        }
        else
        {
            if (RelativeToStr.IsEmpty()) { Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: relativeTo (required for before/after)"), TEXT("MISSING_PARAMETER")); return true; }
            // Pass value as both GUID and NAME: ResolveMemberGuid tries GUID first, then FriendlyName.
            const FGuid RelGuid = ResolveMemberGuid(S, RelativeToStr, RelativeToStr);
            if (!RelGuid.IsValid() || !FStructureEditorUtils::GetVarDescByGuid(S, RelGuid)) { Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("Relative target member not found"), TEXT("MEMBER_NOT_FOUND")); return true; }
            if (RelGuid == G) { Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("Cannot move a member relative to itself"), TEXT("INVALID_OPERATION")); return true; }
            TargetGuid = RelGuid;
            MovePos = (Position == TEXT("after")) ? FStructureEditorUtils::EMovePosition::PositionBelow : FStructureEditorUtils::EMovePosition::PositionAbove;
        }

        if (TargetGuid == G)
        {
            // No-op: the member is already at the requested anchor (single-member
            // struct, or already first/last). Report success without moving.
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("varGuid"), G.ToString());
            Result->SetStringField(TEXT("position"), Position.IsEmpty() ? TEXT("none") : Position);
            McpHandlerUtils::AddVerification(Result, S);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                TEXT("Struct member reordered"), Result);
            return true;
        }

        FStructureEditorUtils::MoveVariable(S, G, TargetGuid, MovePos);
        FStructureEditorUtils::CompileStructure(S);
        McpRefreshStructDependents(S);
        S->GetOutermost()->MarkPackageDirty();
        if (bSave) { McpSafeAssetSave(S); }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("varGuid"), G.ToString());
        Result->SetStringField(TEXT("position"), Position);
        McpHandlerUtils::AddVerification(Result, S);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Struct member reordered"), Result);
        return true;
    }

    if (Lower == TEXT("set_struct_member_default"))
    {
        FString StructPath = GetPayloadString(Payload, TEXT("structPath"));
        FString VarGuidStr = GetPayloadString(Payload, TEXT("varGuid"));
        FString MemberName = GetPayloadString(Payload, TEXT("memberName"));
        bool bSave = GetPayloadBool(Payload, TEXT("save"), false);
        const TSharedPtr<FJsonValue>* DefaultVal = Payload->Values.Find(TEXT("defaultValue"));

        if (StructPath.IsEmpty() || !DefaultVal) { Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: structPath or defaultValue"), TEXT("MISSING_PARAMETER")); return true; }

        UUserDefinedStruct* S = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
        if (!S) { Bridge.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Struct not found: %s"), *StructPath), TEXT("ASSET_NOT_FOUND")); return true; }

        const FGuid G = ResolveMemberGuid(S, VarGuidStr, MemberName);
        if (!G.IsValid()) { Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("Struct member not found"), TEXT("MEMBER_NOT_FOUND")); return true; }

        FStructureEditorUtils::CompileStructure(S);
        FProperty* Prop = FStructureEditorUtils::GetPropertyByGuid(S, G);
        if (!Prop) { Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("Struct member property not found"), TEXT("MEMBER_NOT_FOUND")); return true; }

        const FString ExportText = BuildDefaultExportText(S, Prop, *DefaultVal);

        // [29] A non-empty defaultValue that cannot be converted yields an empty
        // export text. Applying it would silently clear the existing default.
        // Reject invalid input instead of clearing; only treat empty export text
        // as a valid clear when the input itself was empty/null.
        const TSharedPtr<FJsonValue>& DefaultValueJson = *DefaultVal;
        const bool bInputIsEmpty =
            !DefaultValueJson.IsValid() ||
            DefaultValueJson->Type == EJson::Null ||
            (DefaultValueJson->Type == EJson::String && DefaultValueJson->AsString().IsEmpty());
        if (ExportText.IsEmpty() && !bInputIsEmpty)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("defaultValue could not be converted to the member property type"), TEXT("INVALID_DEFAULT"));
            return true;
        }

        FStructureEditorUtils::ChangeVariableDefaultValue(S, G, ExportText);
        FStructureEditorUtils::CompileStructure(S);
        McpRefreshStructDependents(S);
        S->GetOutermost()->MarkPackageDirty();
        if (bSave) { McpSafeAssetSave(S); }

        const FStructVariableDescription* VarDesc = FStructureEditorUtils::GetVarDescByGuid(S, G);
        const FString VerifiedDefault = VarDesc ? VarDesc->DefaultValue : ExportText;

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("varGuid"), G.ToString());
        Result->SetStringField(TEXT("default"), VerifiedDefault);
        McpHandlerUtils::AddVerification(Result, S);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Struct member default value set"), Result);
        return true;
    }

    if (Lower == TEXT("set_struct_member_metadata"))
    {
        FString StructPath = GetPayloadString(Payload, TEXT("structPath"));
        FString VarGuidStr = GetPayloadString(Payload, TEXT("varGuid"));
        FString MemberName = GetPayloadString(Payload, TEXT("memberName"));
        FString Tooltip = GetPayloadString(Payload, TEXT("tooltip"));
        bool bSave = GetPayloadBool(Payload, TEXT("save"), false);

        if (StructPath.IsEmpty()) { Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: structPath"), TEXT("MISSING_PARAMETER")); return true; }

        UUserDefinedStruct* S = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
        if (!S) { Bridge.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Struct not found: %s"), *StructPath), TEXT("ASSET_NOT_FOUND")); return true; }

        const FGuid G = ResolveMemberGuid(S, VarGuidStr, MemberName);
        if (!G.IsValid()) { Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("Struct member not found"), TEXT("MEMBER_NOT_FOUND")); return true; }

        const TSharedPtr<FJsonObject>* MetaObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("metadata"), MetaObj) && MetaObj && (*MetaObj).IsValid())
        {
            for (const auto& Pair : (*MetaObj)->Values)
            {
                FStructureEditorUtils::SetMetaData(S, G, *Pair.Key, Pair.Value->AsString());
            }
        }
        if (!Tooltip.IsEmpty()) { FStructureEditorUtils::ChangeVariableTooltip(S, G, Tooltip); }

        FStructureEditorUtils::CompileStructure(S);
        McpRefreshStructDependents(S);
        S->GetOutermost()->MarkPackageDirty();
        if (bSave) { McpSafeAssetSave(S); }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("varGuid"), G.ToString());
        McpHandlerUtils::AddVerification(Result, S);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Struct member metadata updated"), Result);
        return true;
    }

    return false;
}

#endif // WITH_EDITOR
