#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Safety/McpSafeOperations.h"
#include "EngineUtils.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#include "EditorAssetLibrary.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"


namespace McpBlueprintUtils
{

TArray<TSharedPtr<FJsonValue>> CollectBlueprintVariables(UBlueprint* Blueprint)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    if (!Blueprint)
    {
        return Out;
    }

    TArray<UBlueprint*> Chain;
    {
        UBlueprint* Current = Blueprint;
        while (Current)
        {
            Chain.Add(Current);
            UClass* ParentClass = Current->ParentClass;
            UBlueprint* ParentBP = ParentClass
                ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy)
                : nullptr;
            if (!ParentBP || ParentBP == Current || Chain.Contains(ParentBP))
            {
                break;
            }
            Current = ParentBP;
        }
    }

    for (int32 ChainIdx = Chain.Num() - 1; ChainIdx >= 0; --ChainIdx)
    {
        UBlueprint* CurrentBP = Chain[ChainIdx];
        const bool bInherited = (CurrentBP != Blueprint);

        for (const FBPVariableDescription& Var : CurrentBP->NewVariables)
        {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"), Var.VarName.ToString());
            Obj->SetStringField(TEXT("type"), DescribePinType(Var.VarType));
            Obj->SetBoolField(TEXT("replicated"), (Var.PropertyFlags & CPF_Net) != 0);
            // Instance Editable, the flag add_variable's isPublic sets.
            Obj->SetBoolField(TEXT("public"), (Var.PropertyFlags & CPF_DisableEditOnInstance) == 0);

            const FString CategoryStr = Var.Category.ToString();
            if (!CategoryStr.IsEmpty())
            {
                Obj->SetStringField(TEXT("category"), CategoryStr);
            }

            if (bInherited)
            {
                Obj->SetBoolField(TEXT("inherited"), true);
                Obj->SetStringField(TEXT("declaringBlueprint"), CurrentBP->GetName());
            }

            Out.Add(MakeShared<FJsonValueObject>(Obj));
        }
    }

    return Out;
}

TArray<TSharedPtr<FJsonValue>> CollectBlueprintFunctions(UBlueprint* Blueprint)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    if (!Blueprint)
    {
        return Out;
    }

    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (!Graph)
        {
            continue;
        }

        TSharedPtr<FJsonObject> Fn = MakeShared<FJsonObject>();
        Fn->SetStringField(TEXT("name"), Graph->GetName());

        bool bIsPublic = true;
        bool bResultRead = false;
        TArray<TSharedPtr<FJsonValue>> Inputs;
        TArray<TSharedPtr<FJsonValue>> Outputs;

        for (UEdGraphNode* Node : Graph->Nodes)
        {
            // The entry node's user pins are the inputs, the result node's the outputs.
            UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node);
            UK2Node_EditablePinBase* PinNode = EntryNode ? static_cast<UK2Node_EditablePinBase*>(EntryNode)
                                                         : Cast<UK2Node_FunctionResult>(Node);
            // Every return node of a function carries the same outputs: a function with two
            // returns listed each output twice.
            if (!PinNode || (!EntryNode && bResultRead))
            {
                continue;
            }
            bResultRead |= !EntryNode;
            for (const TSharedPtr<FUserPinInfo>& PinInfo : PinNode->UserDefinedPins)
            {
                if (PinInfo.IsValid())
                {
                    TSharedPtr<FJsonObject> PinJson = MakeShared<FJsonObject>();
                    PinJson->SetStringField(TEXT("name"), PinInfo->PinName.ToString());
                    PinJson->SetStringField(TEXT("type"), DescribePinType(PinInfo->PinType));
                    (EntryNode ? Inputs : Outputs).Add(MakeShared<FJsonValueObject>(PinJson));
                }
            }
            if (EntryNode)
            {
                bIsPublic = (EntryNode->GetFunctionFlags() & FUNC_Public) != 0;
            }
        }

        Fn->SetBoolField(TEXT("public"), bIsPublic);
        if (Inputs.Num() > 0)
        {
            Fn->SetArrayField(TEXT("inputs"), Inputs);
        }
        if (Outputs.Num() > 0)
        {
            Fn->SetArrayField(TEXT("outputs"), Outputs);
        }

        Out.Add(MakeShared<FJsonValueObject>(Fn));
    }

    return Out;
}
}

