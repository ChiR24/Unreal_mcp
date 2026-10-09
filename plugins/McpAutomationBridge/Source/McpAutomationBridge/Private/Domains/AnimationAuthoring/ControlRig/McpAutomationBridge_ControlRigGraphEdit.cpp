#include "Domains/AnimationAuthoring/ControlRig/McpAutomationBridge_ControlRigGraph.h"

#if MCP_HAS_CONTROLRIG_BLUEPRINT
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMPin.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#endif

// edit_control_rig: units added, wired, set and removed in the rig's graph, and bones, nulls and controls added to or
// removed from its hierarchy, one edit or many steps, then one compile and save. A Control Rig could only be created.
namespace McpAnimationAuthoring {
#if MCP_HAS_CONTROLRIG_BLUEPRINT
void McpNoteRigMade(FMcpRigEdit& Edit, const TCHAR* Field, const FString& Name, const TSharedPtr<FJsonObject>& Extra)
{
    const TSharedPtr<FJsonObject> Item = Extra.IsValid() ? Extra : MakeShared<FJsonObject>();
    Item->SetStringField(Field, Name);
    Edit.Made.Add(MakeShared<FJsonValueObject>(Item));
}

bool McpRefuseRigEdit(FString& OutError, FString& OutCode, const FString& Error, const TCHAR* Code)
{
    OutError = Error;
    OutCode = Code;
    return false;
}

namespace {
constexpr int32 McpMaxCompileErrors = 10;

// A pin value as RigVM's default text: text as given, numbers and true/false spelled out.
bool McpRigPinText(const TSharedPtr<FJsonValue>& Value, FString& OutText)
{
    if (!Value.IsValid())
    {
        return false;
    }
    if (Value->Type == EJson::String || Value->Type == EJson::Number)
    {
        OutText = Value->Type == EJson::String ? Value->AsString() : FString::SanitizeFloat(Value->AsNumber());
        return true;
    }
    if (Value->Type == EJson::Boolean)
    {
        OutText = Value->AsBool() ? TEXT("True") : TEXT("False");
        return true;
    }
    return false;
}

bool McpEditRigGraph(FMcpRigEdit& Edit, const FString& Kind, const TSharedPtr<FJsonObject>& Step, FString& OutError, FString& OutCode)
{
    URigVMGraph* Model = Edit.Graph->GetGraph();
    if (Kind == TEXT("add_unit"))
    {
        const FString UnitName = GetJsonStringField(Step, TEXT("unit"));
        UScriptStruct* Struct = McpFindRigUnitStruct(UnitName);
        if (!Struct)
        {
            return McpRefuseRigEdit(OutError, OutCode, FString::Printf(TEXT("unit '%s' is not a rig unit; list_rig_units finds one."), *UnitName), TEXT("UNIT_NOT_FOUND"));
        }
        const TArray<TSharedPtr<FJsonValue>>* At = nullptr;
        const bool bPlaced = Step->TryGetArrayField(TEXT("position"), At) && At && At->Num() == 2;
        const FVector2D Position = bPlaced ? FVector2D((*At)[0]->AsNumber(), (*At)[1]->AsNumber())
                                           : FVector2D(300.0 * Model->GetNodes().Num(), 0.0);
        URigVMUnitNode* Node = Edit.Graph->AddUnitNodeFromStructPath(Struct->GetPathName(), TEXT("Execute"), Position, GetJsonStringField(Step, TEXT("name")), true);
        if (!Node)
        {
            return McpRefuseRigEdit(OutError, OutCode, FString::Printf(TEXT("%s could not be added to the graph."), *Struct->GetName()), TEXT("ADD_FAILED"));
        }
        const TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
        Extra->SetStringField(TEXT("unit"), Struct->GetPathName());
        Extra->SetArrayField(TEXT("pins"), McpDescribeRigPins(Node, false));
        McpNoteRigMade(Edit, TEXT("node"), Node->GetName(), Extra);
        return true;
    }
    if (Kind == TEXT("connect") || Kind == TEXT("disconnect"))
    {
        const FString From = GetJsonStringField(Step, TEXT("from"));
        const FString To = GetJsonStringField(Step, TEXT("to"));
        for (const FString& Path : {From, To})
        {
            if (!Model->FindPin(Path))
            {
                return McpRefuseRigEdit(OutError, OutCode, FString::Printf(TEXT("pin '%s' is not in the graph; get_control_rig lists each node's pins as Node.Pin."), *Path), TEXT("PIN_NOT_FOUND"));
            }
        }
        const bool bDone = Kind == TEXT("connect") ? Edit.Graph->AddLink(From, To, true) : Edit.Graph->BreakLink(From, To, true);
        return bDone || McpRefuseRigEdit(OutError, OutCode, FString::Printf(TEXT("%s %s -> %s was refused: an output must feed an input of a matching type (or no such link exists)."), *Kind, *From, *To), TEXT("LINK_FAILED"));
    }
    if (Kind == TEXT("set_pin"))
    {
        const FString PinPath = GetJsonStringField(Step, TEXT("pin"));
        URigVMPin* Pin = Model->FindPin(PinPath);
        FString Text;
        if (!Pin)
        {
            return McpRefuseRigEdit(OutError, OutCode, FString::Printf(TEXT("pin '%s' is not in the graph; a struct member has its own pin (Node.Item.Name)."), *PinPath), TEXT("PIN_NOT_FOUND"));
        }
        if (!McpRigPinText(Step->TryGetField(TEXT("value")), Text))
        {
            return McpRefuseRigEdit(OutError, OutCode, TEXT("value must be text, a number or true/false; set a struct member through its own pin (Node.Transform.Translation.X)."), TEXT("INVALID_ARGUMENT"));
        }
        if (!Edit.Graph->SetPinDefaultValue(PinPath, Text, true, true))
        {
            return McpRefuseRigEdit(OutError, OutCode, FString::Printf(TEXT("%s did not take '%s'."), *PinPath, *Text), TEXT("SET_FAILED"));
        }
        const TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
        Extra->SetStringField(TEXT("value"), Pin->GetDefaultValue());
        McpNoteRigMade(Edit, TEXT("pin"), PinPath, Extra);
        return true;
    }
    if (Kind == TEXT("remove_node"))
    {
        const FString Name = GetJsonStringField(Step, TEXT("node"));
        if (!Model->FindNodeByName(FName(*Name)))
        {
            return McpRefuseRigEdit(OutError, OutCode, FString::Printf(TEXT("node '%s' is not in the graph."), *Name), TEXT("NODE_NOT_FOUND"));
        }
        return Edit.Graph->RemoveNodeByName(FName(*Name), true) || McpRefuseRigEdit(OutError, OutCode, FString::Printf(TEXT("node '%s' could not be removed."), *Name), TEXT("REMOVE_FAILED"));
    }
    return McpRefuseRigEdit(OutError, OutCode, FString::Printf(TEXT("edit '%s' is not add_unit, connect, disconnect, set_pin, remove_node, import_bones, add_bone, add_null, add_control or remove_element."), *Kind), TEXT("INVALID_ARGUMENT"));
}

} // namespace

TSharedPtr<FJsonObject> HandleEditControlRig(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    FString Error;
    FString Code;
    FMcpRigEdit Edit;
    Edit.Rig = McpLoadControlRig(GetJsonStringField(Params, TEXT("assetPath")), Error);
    if (!Edit.Rig)
    {
        ANIM_ERROR_RESPONSE(Error, TEXT("NOT_FOUND"));
    }
    Edit.Graph = Edit.Rig->GetOrCreateController();
    Edit.Hierarchy = Edit.Rig->GetHierarchyController();
    if (!Edit.Graph || !Edit.Graph->GetGraph() || !Edit.Hierarchy)
    {
        ANIM_ERROR_RESPONSE(TEXT("The rig's graph or hierarchy cannot be edited."), TEXT("NOT_EDITABLE"));
    }
    // A single edit is a batch of one.
    TArray<TSharedPtr<FJsonValue>> Steps;
    const TArray<TSharedPtr<FJsonValue>>* Given = nullptr;
    if (Params->TryGetArrayField(TEXT("steps"), Given) && Given)
    {
        Steps = *Given;
    }
    else
    {
        Steps.Add(MakeShared<FJsonValueObject>(Params));
    }
    int32 Applied = 0;
    int32 Failed = INDEX_NONE;
    for (int32 Index = 0; Index < Steps.Num() && Failed == INDEX_NONE; ++Index)
    {
        const TSharedPtr<FJsonObject>* Step = nullptr;
        const bool bObject = Steps[Index].IsValid() && Steps[Index]->TryGetObject(Step) && Step;
        const FString Kind = bObject ? GetJsonStringField(*Step, TEXT("edit")).ToLower() : FString();
        const bool bHierarchy = Kind == TEXT("import_bones") || Kind == TEXT("add_bone") || Kind == TEXT("add_null") ||
                                Kind == TEXT("add_control") || Kind == TEXT("remove_element");
        const bool bDone = bObject && (bHierarchy ? McpEditRigHierarchy(Edit, Kind, *Step, Error, Code)
                                                  : McpEditRigGraph(Edit, Kind, *Step, Error, Code));
        if (!bObject)
        {
            McpRefuseRigEdit(Error, Code, TEXT("each step is an object with an edit"), TEXT("INVALID_ARGUMENT"));
        }
        Applied += bDone ? 1 : 0;
        Failed = bDone ? INDEX_NONE : Index;
    }
    // What was applied is compiled and saved even when a later step failed.
    if (Applied > 0)
    {
        FCompilerResultsLog Results;
        FKismetEditorUtilities::CompileBlueprint(Edit.Rig, EBlueprintCompileOptions::None, &Results);
        TArray<TSharedPtr<FJsonValue>> Errors;
        for (const TSharedRef<FTokenizedMessage>& Message : Results.Messages)
        {
            if (Message->GetSeverity() == EMessageSeverity::Error && Errors.Num() < McpMaxCompileErrors)
            {
                Errors.Add(MakeShared<FJsonValueString>(Message->ToText().ToString()));
            }
        }
        Response->SetBoolField(TEXT("compiled"), Edit.Rig->Status != BS_Error);
        if (Errors.Num() > 0)
        {
            Response->SetArrayField(TEXT("compileErrors"), Errors);
        }
        const bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        Response->SetBoolField(TEXT("saved"), bSave && SaveAnimAsset(Edit.Rig, true));
    }
    Response->SetStringField(TEXT("assetPath"), Edit.Rig->GetPathName());
    Response->SetNumberField(TEXT("applied"), Applied);
    if (Edit.Made.Num() > 0)
    {
        Response->SetArrayField(TEXT("made"), Edit.Made);
    }
    if (Failed != INDEX_NONE)
    {
        if (Steps.Num() > 1)
        {
            Response->SetNumberField(TEXT("failedStep"), Failed);
        }
        ANIM_ERROR_RESPONSE(Steps.Num() > 1 ? FString::Printf(TEXT("Step %d failed: %s"), Failed, *Error) : Error, *Code);
    }
    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Applied %d edit(s) to %s."), Applied, *Edit.Rig->GetName()));
    return Response;
}
#endif

} // namespace McpAnimationAuthoring
