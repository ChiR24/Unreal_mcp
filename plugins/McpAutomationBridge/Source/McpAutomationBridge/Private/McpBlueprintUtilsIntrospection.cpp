#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpSafeOperations.h"
#include "EngineUtils.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"
#endif

#if WITH_EDITOR && MCP_HAS_EDGRAPH_SCHEMA_K2

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
            Obj->SetBoolField(TEXT("public"), (Var.PropertyFlags & CPF_BlueprintReadOnly) == 0);

            const FString CategoryStr = Var.Category.IsEmpty() ? FString() : Var.Category.ToString();
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
        TArray<TSharedPtr<FJsonValue>> Inputs;
        TArray<TSharedPtr<FJsonValue>> Outputs;

        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
            {
                // Collect input pins
                for (const TSharedPtr<FUserPinInfo>& PinInfo : EntryNode->UserDefinedPins)
                {
                    if (!PinInfo.IsValid())
                    {
                        continue;
                    }
                    TSharedPtr<FJsonObject> PinJson = MakeShared<FJsonObject>();
                    PinJson->SetStringField(TEXT("name"), PinInfo->PinName.ToString());
                    PinJson->SetStringField(TEXT("type"), DescribePinType(PinInfo->PinType));
                    Inputs.Add(MakeShared<FJsonValueObject>(PinJson));
                }
                bIsPublic = (EntryNode->GetFunctionFlags() & FUNC_Public) != 0;
            }
            else if (UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(Node))
            {
                // Collect output pins
                for (const TSharedPtr<FUserPinInfo>& PinInfo : ResultNode->UserDefinedPins)
                {
                    if (!PinInfo.IsValid())
                    {
                        continue;
                    }
                    TSharedPtr<FJsonObject> PinJson = MakeShared<FJsonObject>();
                    PinJson->SetStringField(TEXT("name"), PinInfo->PinName.ToString());
                    PinJson->SetStringField(TEXT("type"), DescribePinType(PinInfo->PinType));
                    Outputs.Add(MakeShared<FJsonValueObject>(PinJson));
                }
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

FProperty* FindBlueprintProperty(UBlueprint* Blueprint, const FString& PropertyName)
{
    if (!Blueprint || PropertyName.TrimStartAndEnd().IsEmpty())
    {
        return nullptr;
    }

    const FName PropFName(*PropertyName.TrimStartAndEnd());
    const TArray<UClass*> CandidateClasses = {
        Blueprint->GeneratedClass,
        Blueprint->SkeletonGeneratedClass,
        Blueprint->ParentClass
    };

    for (UClass* Candidate : CandidateClasses)
    {
        if (!Candidate)
        {
            continue;
        }

        if (FProperty* Found = Candidate->FindPropertyByName(PropFName))
        {
            return Found;
        }
    }

    return nullptr;
}

UFunction* FindBlueprintFunction(UBlueprint* Blueprint, const FString& FunctionName)
{
    if (!Blueprint || FunctionName.TrimStartAndEnd().IsEmpty())
    {
        return nullptr;
    }

    const FString CleanFunc = FunctionName.TrimStartAndEnd();

    UFunction* Found = FindObject<UFunction>(nullptr, *CleanFunc);
    if (Found)
    {
        return Found;
    }

    const FName FuncFName(*CleanFunc);
    const TArray<UClass*> CandidateClasses = {
        Blueprint->GeneratedClass,
        Blueprint->SkeletonGeneratedClass,
        Blueprint->ParentClass
    };

    for (UClass* Candidate : CandidateClasses)
    {
        if (Candidate)
        {
            UFunction* CandidateFunc = Candidate->FindFunctionByName(FuncFName);
            if (CandidateFunc)
            {
                return CandidateFunc;
            }
        }
    }

    // Try class.function format
    int32 DotIndex = INDEX_NONE;
    if (CleanFunc.FindChar('.', DotIndex))
    {
        const FString ClassPath = CleanFunc.Left(DotIndex);
        const FString FuncSegment = CleanFunc.Mid(DotIndex + 1);
        if (!ClassPath.IsEmpty() && !FuncSegment.IsEmpty())
        {
            if (UClass* ExplicitClass = FindObject<UClass>(nullptr, *ClassPath))
            {
                UFunction* ExplicitFunc = ExplicitClass->FindFunctionByName(FName(*FuncSegment));
                if (ExplicitFunc)
                {
                    return ExplicitFunc;
                }
            }
        }
    }

    return nullptr;
}
}

#endif
