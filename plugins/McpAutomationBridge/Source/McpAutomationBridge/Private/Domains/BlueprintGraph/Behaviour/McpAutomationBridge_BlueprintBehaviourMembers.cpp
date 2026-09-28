// The recipe's members: variables, event dispatchers, custom events and functions.
// Every name and type is checked against the Blueprint before anything changes;
// then each member becomes a build_graph member step (add_variable,
// add_event_dispatcher, add_function + its body with graphName set).
#include "Domains/BlueprintGraph/Behaviour/McpAutomationBridge_BlueprintBehaviour.h"

#include "Domains/Blueprint/Variables/McpAutomationBridge_BlueprintVariableObjectDefault.h"
#include "EdGraphSchema_K2.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsBlueprintGraph.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"

namespace McpBlueprintBehaviour::Detail
{
namespace
{
bool MemberTypesResolve(const TSharedPtr<FJsonObject>& Owner, const TCHAR* Field, const FString& Where, FString& OutError,
                        FString& OutCode)
{
    TArray<TSharedPtr<FJsonObject>> Params;
    OutCode = TEXT("INVALID_RECIPE");
    if (!ObjectsIn(Owner, Field, Params, OutError))
    {
        return false;
    }
    for (int32 Index = 0; Index < Params.Num(); ++Index)
    {
        const FString At = FString::Printf(TEXT("%s.%s[%d]"), *Where, Field, Index);
        if (!CheckKeys(Params[Index], {TEXT("name"), TEXT("type")}, At, OutError))
        {
            return false;
        }
        const McpBlueprintUtils::FTypeResolutionResult Type =
            McpBlueprintUtils::ResolvePinType(GetJsonStringField(Params[Index], TEXT("type")));
        if (GetJsonStringField(Params[Index], TEXT("name")).IsEmpty() || !Type.bSuccess)
        {
            OutCode = TEXT("TYPE_RESOLUTION_FAILED");
            OutError = FString::Printf(TEXT("%s needs a name and a type that resolves: %s"), *At, *Type.OutError);
            return false;
        }
    }
    return true;
}

TSharedPtr<FJsonObject> MemberStep(const TCHAR* Edit, const TSharedPtr<FJsonObject>& Source,
                                   std::initializer_list<const TCHAR*> Fields)
{
    TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
    Step->SetStringField(TEXT("edit"), Edit);
    for (const TCHAR* Field : Fields)
    {
        if (const TSharedPtr<FJsonValue> Value = Source->TryGetField(Field))
        {
            Step->SetField(Field, Value);
        }
    }
    return Step;
}

// A scalar default is the variable's export text, checked by importing it into the
// class default object the way the compiler will; arrays and objects go through
// the converter set_default uses.
bool RedefaultVariable(UBlueprint* Blueprint, FName Name, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
    FString Text;
    if (!McpJsonScalarToString(Value, Text))
    {
        return McpBlueprintHandlers::McpApplyVariableObjectDefault(Blueprint, Name, Value, OutError);
    }
    UClass* Generated = Blueprint->GeneratedClass;
    UObject* Defaults = Generated ? Generated->GetDefaultObject() : nullptr;
    const FProperty* Property = Defaults ? Generated->FindPropertyByName(Name) : nullptr;
    const int32 Index = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, Name);
    if (!Property || Index == INDEX_NONE ||
        !FBlueprintEditorUtils::PropertyValueFromString(Property, Text, reinterpret_cast<uint8*>(Defaults)))
    {
        OutError = FString::Printf(TEXT("'%s' is not a valid default for variable %s (%s)."), *Text, *Name.ToString(),
                                   Property ? *Property->GetCPPType() : TEXT("not compiled"));
        return false;
    }
    Blueprint->NewVariables[Index].DefaultValue = Text;
    return true;
}
} // namespace

bool PlanMembers(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Recipe, FPlan& Plan, FString& OutError,
                 FString& OutCode)
{
    TArray<TSharedPtr<FJsonObject>> Dispatchers;
    TArray<TSharedPtr<FJsonObject>> Functions;
    OutCode = TEXT("INVALID_RECIPE");
    if (!ObjectsIn(Recipe, TEXT("variables"), Plan.Variables, OutError) ||
        !ObjectsIn(Recipe, TEXT("dispatchers"), Dispatchers, OutError) ||
        !ObjectsIn(Recipe, TEXT("customEvents"), Plan.CustomEvents, OutError) ||
        !ObjectsIn(Recipe, TEXT("functions"), Functions, OutError))
    {
        return false;
    }
    FKismetNameValidator Validator(Blueprint);
    TSet<FName> Names;
    auto Fail = [&OutError, &OutCode](const TCHAR* Code, const FString& Message)
    {
        OutCode = Code;
        OutError = Message;
        return false;
    };
    // Unique inside the recipe, and (when new) free on the Blueprint and its parents.
    auto Claim = [&](const FString& Name, const FString& Where, bool bNew)
    {
        bool bDuplicate = false;
        Names.Add(FName(*Name), &bDuplicate);
        if (Name.IsEmpty() || bDuplicate)
        {
            return Fail(TEXT("INVALID_RECIPE"), FString::Printf(TEXT("%s has no name or repeats '%s'."), *Where, *Name));
        }
        return !bNew || Validator.IsValid(FName(*Name)) == EValidatorResult::Ok ||
               Fail(TEXT("NAME_CONFLICT"), FString::Printf(TEXT("%s: '%s' is already a variable, function, component or "
                                                               "event of this Blueprint or its parent class."), *Where, *Name));
    };
    for (int32 Index = 0; Index < Plan.Variables.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>& Var = Plan.Variables[Index];
        const FString Where = FString::Printf(TEXT("variables[%d]"), Index);
        const FString Name = GetJsonStringField(Var, TEXT("variableName"));
        const int32 Existing = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, FName(*Name));
        if (!CheckKeys(Var, {TEXT("variableName"), TEXT("variableType"), TEXT("defaultValue"), TEXT("category"),
                             TEXT("isReplicated"), TEXT("instanceEditable"), TEXT("exposeOnSpawn"), TEXT("keepExisting")},
                       Where, OutError) ||
            !Claim(Name, Where, Existing == INDEX_NONE))
        {
            return false;
        }
        const McpBlueprintUtils::FTypeResolutionResult Type =
            McpBlueprintUtils::ResolvePinType(GetJsonStringField(Var, TEXT("variableType")));
        if (!Type.bSuccess)
        {
            return Fail(TEXT("TYPE_RESOLUTION_FAILED"), FString::Printf(TEXT("%s %s: %s"), *Where, *Name, *Type.OutError));
        }
        // A Blueprint "Float" is a double when the editor makes it and a float when the resolver does.
        FEdGraphPinType Have = Existing != INDEX_NONE ? Blueprint->NewVariables[Existing].VarType : Type.PinType;
        FEdGraphPinType Want = Type.PinType;
        Have.PinSubCategory = Have.PinCategory == UEdGraphSchema_K2::PC_Real ? FName() : Have.PinSubCategory;
        Want.PinSubCategory = Want.PinCategory == UEdGraphSchema_K2::PC_Real ? FName() : Want.PinSubCategory;
        if (Have != Want)
        {
            return Fail(TEXT("VARIABLE_TYPE_CONFLICT"),
                        FString::Printf(TEXT("%s: %s already exists as %s; the recipe declares it %s."), *Where, *Name,
                                        *McpBlueprintUtils::DescribePinType(Blueprint->NewVariables[Existing].VarType),
                                        *McpBlueprintUtils::DescribePinType(Type.PinType)));
        }
        AddOp(Plan, MemberStep(TEXT("add_variable"), Var, {TEXT("variableName"), TEXT("variableType"), TEXT("defaultValue"),
                                                           TEXT("category"), TEXT("isReplicated")}),
              Where + TEXT(" ") + Name);
    }
    for (int32 Index = 0; Index < Dispatchers.Num(); ++Index)
    {
        const FString Where = FString::Printf(TEXT("dispatchers[%d]"), Index);
        const FString Name = GetJsonStringField(Dispatchers[Index], TEXT("name"));
        const bool bExists = Blueprint->DelegateSignatureGraphs.ContainsByPredicate(
            [&Name](const TObjectPtr<UEdGraph>& Graph) { return Graph && Graph->GetFName() == FName(*Name); });
        if (!CheckKeys(Dispatchers[Index], {TEXT("name"), TEXT("parameters")}, Where, OutError) ||
            !Claim(Name, Where, !bExists) || !MemberTypesResolve(Dispatchers[Index], TEXT("parameters"), Where, OutError, OutCode))
        {
            return false;
        }
        TSharedPtr<FJsonObject> Step = MemberStep(TEXT("add_event_dispatcher"), Dispatchers[Index], {TEXT("parameters")});
        Step->SetStringField(TEXT("dispatcherName"), Name);
        AddOp(Plan, Step, Where + TEXT(" ") + Name);
    }
    for (int32 Index = 0; Index < Plan.CustomEvents.Num(); ++Index)
    {
        const FString Where = FString::Printf(TEXT("customEvents[%d]"), Index);
        const FString Name = GetJsonStringField(Plan.CustomEvents[Index], TEXT("eventName"));
        const UK2Node_Event* Existing = FBlueprintEditorUtils::FindCustomEventNode(Blueprint, FName(*Name));
        if (!CheckKeys(Plan.CustomEvents[Index], {TEXT("id"), TEXT("eventName"), TEXT("shared")}, Where, OutError) ||
            !Claim(Name, Where, Existing == nullptr))
        {
            return false;
        }
        // Users often have their own OnDeath: an event this behaviour does not own is never taken over.
        if (Existing && Existing->NodeComment != HubTag && !IsOwnedBy(*Existing, Plan.Tag))
        {
            return Fail(TEXT("FUNCTION_EXISTS"), FString::Printf(TEXT("%s: custom event %s already exists and belongs to "
                                                                      "this Blueprint's own graph or another behaviour."),
                                                                 *Where, *Name));
        }
        Plan.Rebuilt.Add(FName(*Name));
    }
    for (int32 Index = 0; Index < Functions.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>& Function = Functions[Index];
        const FString Where = FString::Printf(TEXT("functions[%d]"), Index);
        const FString Name = GetJsonStringField(Function, TEXT("functionName"));
        const TObjectPtr<UEdGraph>* Existing = Blueprint->FunctionGraphs.FindByPredicate(
            [&Name](const TObjectPtr<UEdGraph>& Graph) { return Graph && Graph->GetFName() == FName(*Name); });
        TArray<TSharedPtr<FJsonObject>> Body;
        if (!CheckKeys(Function, {TEXT("id"), TEXT("functionName"), TEXT("inputs"), TEXT("outputs"), TEXT("pure"),
                                  TEXT("isPublic"), TEXT("operations")}, Where, OutError) ||
            !Claim(Name, Where, Existing == nullptr) || !ObjectsIn(Function, TEXT("operations"), Body, OutError) ||
            !MemberTypesResolve(Function, TEXT("inputs"), Where, OutError, OutCode) ||
            !MemberTypesResolve(Function, TEXT("outputs"), Where, OutError, OutCode))
        {
            return false;
        }
        if (Existing && !(Plan.bReplace && IsFunctionOwnedBy(*Existing, Plan.Tag)))
        {
            return Fail(TEXT("FUNCTION_EXISTS"), FString::Printf(TEXT("%s: function %s already exists and is not this "
                                                                      "behaviour's to replace."), *Where, *Name));
        }
        AddOp(Plan, MemberStep(TEXT("add_function"), Function, {TEXT("id"), TEXT("functionName"), TEXT("inputs"),
                                                                TEXT("outputs"), TEXT("pure"), TEXT("isPublic")}),
              Where + TEXT(" ") + Name);
        for (int32 Step = 0; Step < Body.Num(); ++Step)
        {
            TSharedPtr<FJsonObject> Op = CopyObject(Body[Step]);
            Op->SetStringField(TEXT("graphName"), Name);
            AddOp(Plan, Op, FString::Printf(TEXT("%s.operations[%d]"), *Where, Step));
        }
        Plan.Rebuilt.Add(FName(*Name));
    }
    return true;
}

bool CommitVariables(UBlueprint* Blueprint, const FPlan& Plan, const FSnapshot& Before,
                     const TSharedPtr<FJsonObject>& Report, FString& OutError)
{
    TArray<TSharedPtr<FJsonValue>> Rows;
    // Flags and defaults land in NewVariables; the compile after this puts them on the class.
    for (const TSharedPtr<FJsonObject>& Var : Plan.Variables)
    {
        const FName Name(*GetJsonStringField(Var, TEXT("variableName")));
        const int32 Index = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, Name);
        const bool bExisted = Before.Variables.ContainsByPredicate(
            [&Name](const FBPVariableDescription& Old) { return Old.VarName == Name; });
        if (Index == INDEX_NONE)
        {
            OutError = FString::Printf(TEXT("Variable %s is not on the Blueprint after its add_variable step."), *Name.ToString());
            return false;
        }
        FBPVariableDescription& Desc = Blueprint->NewVariables[Index];
        bool bFlag = false;
        if (Var->TryGetBoolField(TEXT("instanceEditable"), bFlag))
        {
            Desc.PropertyFlags = bFlag ? (Desc.PropertyFlags & ~CPF_DisableEditOnInstance) : (Desc.PropertyFlags | CPF_DisableEditOnInstance);
        }
        // Expose on Spawn needs an instance-editable variable (the compiler warns otherwise).
        if (Var->TryGetBoolField(TEXT("exposeOnSpawn"), bFlag) && bFlag)
        {
            Desc.SetMetaData(FBlueprintMetadata::MD_ExposeOnSpawn, TEXT("true"));
            Desc.PropertyFlags &= ~CPF_DisableEditOnInstance;
        }
        else if (Var->HasField(TEXT("exposeOnSpawn")))
        {
            Desc.RemoveMetaData(FBlueprintMetadata::MD_ExposeOnSpawn);
        }
        const TSharedPtr<FJsonValue> Default = Var->TryGetField(TEXT("defaultValue"));
        const bool bRedefault = bExisted && !GetJsonBoolField(Var, TEXT("keepExisting")) && Default.IsValid() &&
                                Default->Type != EJson::Null;
        if (bRedefault && !RedefaultVariable(Blueprint, Name, Default, OutError))
        {
            return false;
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Name.ToString());
        Row->SetStringField(TEXT("status"), !bExisted ? TEXT("added") : (bRedefault ? TEXT("redefaulted") : TEXT("kept")));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Report->SetArrayField(TEXT("variables"), Rows);
    return true;
}
} // namespace McpBlueprintBehaviour::Detail
