// =============================================================================
// McpHandlerUtils.cpp
// =============================================================================
// Implementation of centralized utility functions for MCP Automation Bridge handlers.
// =============================================================================

#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpSafeOperations.h"
#include "EngineUtils.h"

// Define log category declared in McpSafeOperations.h
DEFINE_LOG_CATEGORY(LogMcpSafeOperations);

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

// K2Node includes for blueprint graph operations
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"
#endif

// =============================================================================
// JSON Value Conversion
// =============================================================================

namespace McpHandlerUtils
{

FString JsonValueToString(const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid())
    {
        return FString();
    }

    switch (Value->Type)
    {
    case EJson::String:
        return Value->AsString();
    case EJson::Number:
        return LexToString(Value->AsNumber());
    case EJson::Boolean:
        return Value->AsBool() ? TEXT("true") : TEXT("false");
    case EJson::Null:
        return FString();
    default:
        break;
    }

    // Handle object and array types by serializing
    FString Serialized;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
    
    if (Value->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> Obj = Value->AsObject();
        if (Obj.IsValid())
        {
            FJsonSerializer::Serialize(Obj.ToSharedRef(), *Writer, true);
        }
    }
    else if (Value->Type == EJson::Array)
    {
        FJsonSerializer::Serialize(Value->AsArray(), *Writer, true);
    }
    else
    {
        Writer->WriteValue(Value->AsString());
    }
    
    Writer->Close();
    return Serialized;
}

// =============================================================================
// Asset Path Utilities
// =============================================================================

FString ValidateAssetPath(const FString& Path)
{
    if (Path.IsEmpty())
    {
        return FString();
    }

    FString CleanPath = Path;
    
    // Reject Windows absolute paths
    if (CleanPath.Len() >= 2 && CleanPath[1] == TEXT(':'))
    {
        UE_LOG(LogTemp, Warning, TEXT("ValidateAssetPath: Rejected Windows absolute path: %s"), *Path);
        return FString();
    }

    // Normalize slashes
    CleanPath.ReplaceInline(TEXT("\\"), TEXT("/"));
    
    // Remove double slashes
    while (CleanPath.Contains(TEXT("//")))
    {
        CleanPath = CleanPath.Replace(TEXT("//"), TEXT("/"));
    }

    // Reject path traversal
    if (CleanPath.Contains(TEXT("..")))
    {
        UE_LOG(LogTemp, Warning, TEXT("ValidateAssetPath: Rejected path containing '..': %s"), *Path);
        return FString();
    }

    // Ensure path starts with /
    if (!CleanPath.StartsWith(TEXT("/")))
    {
        CleanPath = TEXT("/") + CleanPath;
    }

    // Validate root
    const bool bValidRoot = CleanPath.StartsWith(TEXT("/Game")) ||
                           CleanPath.StartsWith(TEXT("/Engine")) ||
                           CleanPath.StartsWith(TEXT("/Script"));

    if (!bValidRoot)
    {
        // Check for plugin-like paths (e.g., /MyPlugin/Content/Asset)
        TArray<FString> Segments;
        CleanPath.ParseIntoArray(Segments, TEXT("/"), true);
        const bool bLooksLikePluginPath = Segments.Num() >= 3;
        
        if (!bLooksLikePluginPath)
        {
            UE_LOG(LogTemp, Warning, TEXT("ValidateAssetPath: Rejected path without valid root: %s"), *Path);
            return FString();
        }
    }

    return CleanPath;
}

// =============================================================================
// Actor/Component Utilities
// =============================================================================

#if WITH_EDITOR
AActor* FindActorByName(const FString& ActorName, bool bExactMatch)
{
    UWorld* World = nullptr;
    if (GEditor)
    {
        World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
    }
    if (!World)
    {
        return nullptr;
    }

    FString SearchName = ActorName;
    if (bExactMatch)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (Actor && Actor->GetName().Equals(SearchName, ESearchCase::IgnoreCase))
            {
                return Actor;
            }
        }
    }
    else
    {
        // Partial match - actors starting with the name
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (Actor && Actor->GetName().StartsWith(SearchName, ESearchCase::IgnoreCase))
            {
                return Actor;
            }
        }
    }

    return nullptr;
}

UActorComponent* FindActorComponentByName(AActor* Actor, const FString& ComponentName)
{
    if (!Actor || ComponentName.IsEmpty())
    {
        return nullptr;
    }

    const FString Needle = ComponentName.ToLower();
    UActorComponent* ExactMatch = nullptr;
    UActorComponent* StartsWithMatch = nullptr;

    TArray<UActorComponent*> Components;
    Actor->GetComponents(Components);

    for (UActorComponent* Comp : Components)
    {
        if (!Comp)
        {
            continue;
        }

        const FString CompName = Comp->GetName().ToLower();
        const FString CompPath = Comp->GetPathName().ToLower();

        // Exact name match (highest priority)
        if (CompName.Equals(Needle))
        {
            return Comp;
        }

        // Exact path match
        if (CompPath.Equals(Needle))
        {
            return Comp;
        }

        // Path ends with .ComponentName
        if (CompPath.EndsWith(FString::Printf(TEXT(".%s"), *Needle)))
        {
            return Comp;
        }

        // Path ends with :ComponentName (subobject format)
        if (CompPath.EndsWith(FString::Printf(TEXT(":%s"), *Needle)))
        {
            return Comp;
        }

        // Partial match: ComponentName starts with needle
        if (CompName.StartsWith(Needle) && !StartsWithMatch)
        {
            StartsWithMatch = Comp;
        }

        // Path contains the component name
        if (!ExactMatch && CompPath.Contains(Needle))
        {
            ExactMatch = Comp;
        }
    }

    // Return in priority order
    if (StartsWithMatch)
    {
        return StartsWithMatch;
    }
    
    return ExactMatch;
}
#endif

// =============================================================================
// String Utilities
// =============================================================================

FString ToSafeAssetName(const FString& Input)
{
    if (Input.IsEmpty())
    {
        return TEXT("Asset");
    }

    FString Sanitized = Input.TrimStartAndEnd();

    // Replace SQL injection patterns
    Sanitized = Sanitized.Replace(TEXT(";"), TEXT("_"));
    Sanitized = Sanitized.Replace(TEXT("'"), TEXT("_"));
    Sanitized = Sanitized.Replace(TEXT("\""), TEXT("_"));
    Sanitized = Sanitized.Replace(TEXT("--"), TEXT("_"));
    Sanitized = Sanitized.Replace(TEXT("`"), TEXT("_"));

    // Replace invalid characters for Unreal asset names
    const TArray<TCHAR> InvalidChars = {
        TEXT('@'), TEXT('#'), TEXT('%'), TEXT('$'), TEXT('&'), TEXT('*'),
        TEXT('('), TEXT(')'), TEXT('+'), TEXT('='), TEXT('['), TEXT(']'),
        TEXT('{'), TEXT('}'), TEXT('<'), TEXT('>'), TEXT('?'), TEXT('|'),
        TEXT('\\'), TEXT(':'), TEXT('~'), TEXT('!'), TEXT(' ')
    };

    for (TCHAR C : InvalidChars)
    {
        TCHAR CharStr[2] = { C, TEXT('\0') };
        Sanitized = Sanitized.Replace(CharStr, TEXT("_"));
    }

    // Remove consecutive underscores
    while (Sanitized.Contains(TEXT("__")))
    {
        Sanitized = Sanitized.Replace(TEXT("__"), TEXT("_"));
    }

    // Remove leading/trailing underscores
    while (Sanitized.StartsWith(TEXT("_")))
    {
        Sanitized.RemoveAt(0);
    }
    while (Sanitized.EndsWith(TEXT("_")))
    {
        Sanitized.RemoveAt(Sanitized.Len() - 1);
    }

    // If empty after sanitization, use default
    if (Sanitized.IsEmpty())
    {
        return TEXT("Asset");
    }

    // Ensure name starts with letter or underscore
    if (!FChar::IsAlpha(Sanitized[0]) && Sanitized[0] != TEXT('_'))
    {
        Sanitized = TEXT("Asset_") + Sanitized;
    }

    // Truncate to reasonable length
    if (Sanitized.Len() > 64)
    {
        Sanitized = Sanitized.Left(64);
    }

    return Sanitized;
}

FString MakeUniqueAssetName(const FString& BaseName, const FString& PackagePath)
{
#if WITH_EDITOR
    FString TestName = ToSafeAssetName(BaseName);
    FString TestPath = PackagePath / TestName;
    
    // Check if the name is already unique
    if (!UEditorAssetLibrary::DoesAssetExist(TestPath))
    {
        return TestName;
    }

    // Append number suffix until we find a unique name
    int32 Suffix = 1;
    while (Suffix < 10000) // Safety limit
    {
        FString Candidate = FString::Printf(TEXT("%s_%d"), *TestName, Suffix);
        TestPath = PackagePath / Candidate;
        
        if (!UEditorAssetLibrary::DoesAssetExist(TestPath))
        {
            return Candidate;
        }
        Suffix++;
    }
#endif

    // Fallback
    return FString::Printf(TEXT("%s_%d"), *ToSafeAssetName(BaseName), FMath::Rand());
}

// =============================================================================
// Object and Property Resolution Implementation
// =============================================================================

#if WITH_EDITOR
UObject* ResolveObjectFromPath(const FString& ObjectPath, FString* OutResolvedPath)
{
    if (ObjectPath.IsEmpty())
    {
        return nullptr;
    }
    
    FString Path = ObjectPath;
    
    // Handle component paths in "ActorName.ComponentName" format
    if (Path.Contains(TEXT(".")) && !Path.StartsWith(TEXT("/")))
    {
        FString ActorName = Path.Left(Path.Find(TEXT(".")));
        FString ComponentName = Path.Right(Path.Len() - ActorName.Len() - 1);
        
        if (!ActorName.IsEmpty() && !ComponentName.IsEmpty())
        {
            if (AActor* Actor = FindActorByName(ActorName))
            {
                if (UActorComponent* Comp = FindActorComponentByName(Actor, ComponentName))
                {
                    if (OutResolvedPath)
                    {
                        *OutResolvedPath = Comp->GetPathName();
                    }
                    return Comp;
                }
            }
        }
    }
    
    // Try to find as actor by name
    if (AActor* FoundActor = FindActorByName(Path))
    {
        if (OutResolvedPath)
        {
            *OutResolvedPath = FoundActor->GetPathName();
        }
        return FoundActor;
    }
    
    // Try to find by actor label (display name) as fallback
    if (GEditor)
    {
        UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
        if (World)
        {
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                AActor* Actor = *It;
                if (Actor && (Actor->GetActorLabel().Equals(Path, ESearchCase::IgnoreCase) ||
                              Actor->GetName().Equals(Path, ESearchCase::IgnoreCase)))
                {
                    if (OutResolvedPath)
                    {
                        *OutResolvedPath = Actor->GetPathName();
                    }
                    return Actor;
                }
            }
        }
    }
    
    // Try to load as asset (supports both /Game/ and /Engine/ paths)
    if (Path.StartsWith(TEXT("/Game/")) || Path.StartsWith(TEXT("/Engine/")) || Path.StartsWith(TEXT("/Script/")))
    {
        FString PackagePath = Path;
        if (PackagePath.Contains(TEXT(".")))
        {
            PackagePath = PackagePath.Left(PackagePath.Find(TEXT(".")));
        }
        UPackage* LoadedPackage = LoadPackage(nullptr, *PackagePath, LOAD_None);
        if (LoadedPackage)
        {
            if (UObject* Found = FindObject<UObject>(LoadedPackage, *Path))
            {
                if (OutResolvedPath)
                {
                    *OutResolvedPath = Found->GetPathName();
                }
                return Found;
            }
            if (OutResolvedPath)
            {
                *OutResolvedPath = LoadedPackage->GetPathName();
            }
            return LoadedPackage;
        }
        
        // Try StaticFindObject for engine assets that may not need package loading
        if (UObject* Found = FindObject<UObject>(nullptr, *Path))
        {
            if (OutResolvedPath)
            {
                *OutResolvedPath = Found->GetPathName();
            }
            return Found;
        }
    }
    
    return nullptr;
}
#endif

FPropertyResolveResult ResolveProperty(UObject* Object, const FString& PropertyName)
{
    FPropertyResolveResult Result;
    
    if (!Object)
    {
        Result.Error = TEXT("Object is null");
        return Result;
    }
    
    if (PropertyName.IsEmpty())
    {
        Result.Error = TEXT("Property name is empty");
        return Result;
    }
    
    // Handle nested property paths
    if (PropertyName.Contains(TEXT(".")))
    {
        Result.Property = ResolveNestedPropertyPath(Object, PropertyName, Result.Container, Result.Error);
    }
    else
    {
        // Simple property name
        Result.Container = Object;
        Result.Property = Object->GetClass()->FindPropertyByName(*PropertyName);
        
        if (!Result.Property)
        {
            Result.Error = FString::Printf(TEXT("Property '%s' not found on object"), *PropertyName);
        }
    }
    
    return Result;
}

void AddVerification(TSharedPtr<FJsonObject>& Result, UObject* Object)
{
    if (!Result.IsValid() || !Object)
    {
        return;
    }
    
#if WITH_EDITOR
    if (AActor* AsActor = Cast<AActor>(Object))
    {
        AddActorVerification(Result, AsActor);
    }
    else
    {
        AddAssetVerification(Result, Object);
    }
#endif
}

} // namespace McpHandlerUtils

// =============================================================================
// Blueprint Graph Utilities Implementation
// =============================================================================
#if WITH_EDITOR && MCP_HAS_EDGRAPH_SCHEMA_K2

namespace McpBlueprintUtils
{

UEdGraphPin* FindExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
    if (!Node)
    {
        return nullptr;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->Direction == Direction)
        {
            return Pin;
        }
    }

    return nullptr;
}

UEdGraphPin* FindOutputPin(UEdGraphNode* Node, const FName& PinName)
{
    if (!Node)
    {
        return nullptr;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Output)
        {
            if (PinName.IsNone())
            {
                return Pin;
            }
            if (Pin->PinName == PinName)
            {
                return Pin;
            }
        }
    }

    return nullptr;
}

UEdGraphPin* FindInputPin(UEdGraphNode* Node, const FName& PinName)
{
    if (!Node)
    {
        return nullptr;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Input && Pin->PinName == PinName)
        {
            return Pin;
        }
    }

    return nullptr;
}

UEdGraphPin* FindDataPin(UEdGraphNode* Node, EEdGraphPinDirection Direction, const FName& PreferredName)
{
    if (!Node)
    {
        return nullptr;
    }

    UEdGraphPin* Fallback = nullptr;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (!Pin || Pin->Direction != Direction)
        {
            continue;
        }
        if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
        {
            continue;
        }
        if (!PreferredName.IsNone() && Pin->PinName == PreferredName)
        {
            return Pin;
        }
        if (!Fallback)
        {
            Fallback = Pin;
        }
    }

    return Fallback;
}

UEdGraphPin* FindPreferredEventExec(UEdGraph* Graph)
{
    if (!Graph)
    {
        return nullptr;
    }

    // Prefer custom events, fall back to first available event node
    UEdGraphPin* Fallback = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node)
        {
            continue;
        }

        if (UK2Node_CustomEvent* Custom = Cast<UK2Node_CustomEvent>(Node))
        {
            UEdGraphPin* ExecPin = FindExecPin(Custom, EGPD_Output);
            if (ExecPin && ExecPin->LinkedTo.Num() == 0)
            {
                return ExecPin;
            }
            if (!Fallback && ExecPin)
            {
                Fallback = ExecPin;
            }
        }
        else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
        {
            UEdGraphPin* ExecPin = FindExecPin(EventNode, EGPD_Output);
            if (ExecPin && ExecPin->LinkedTo.Num() == 0 && !Fallback)
            {
                Fallback = ExecPin;
            }
        }
    }

    return Fallback;
}

FEdGraphPinType MakePinType(const FString& InType)
{
    FEdGraphPinType PinType;
    const FString Lower = InType.ToLower();
    const FString CleanType = InType.TrimStartAndEnd();

    if (Lower == TEXT("float") || Lower == TEXT("double"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Float;
    }
    else if (Lower == TEXT("int") || Lower == TEXT("integer"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    }
    else if (Lower == TEXT("int64"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
    }
    else if (Lower == TEXT("bool") || Lower == TEXT("boolean"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    }
    else if (Lower == TEXT("string"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    }
    else if (Lower == TEXT("name"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
    }
    else if (Lower == TEXT("text"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
    }
    else if (Lower == TEXT("byte"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
    }
    else if (Lower == TEXT("vector"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    }
    else if (Lower == TEXT("rotator"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
    }
    else if (Lower == TEXT("transform"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
    }
    else if (Lower == TEXT("object"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        PinType.PinSubCategoryObject = UObject::StaticClass();
    }
    else if (Lower == TEXT("class"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Class;
        PinType.PinSubCategoryObject = UObject::StaticClass();
    }
    else
    {
        // Fallback: try to resolve as specific type
        if (UClass* ClassResolve = ResolveClassByName(CleanType))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
            PinType.PinSubCategoryObject = ClassResolve;
        }
        else if (UScriptStruct* StructResolve = FindObject<UScriptStruct>(nullptr, *CleanType))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            PinType.PinSubCategoryObject = StructResolve;
        }
        else if (UScriptStruct* LoadedStruct = LoadObject<UScriptStruct>(nullptr, *CleanType))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            PinType.PinSubCategoryObject = LoadedStruct;
        }
        else
        {
            // Try short name loop for structs
            bool bFoundStruct = false;
            if (!CleanType.Contains(TEXT("/")) && !CleanType.Contains(TEXT(".")))
            {
                for (TObjectIterator<UScriptStruct> It; It; ++It)
                {
                    if (It->GetName().Equals(CleanType, ESearchCase::IgnoreCase))
                    {
                        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
                        PinType.PinSubCategoryObject = *It;
                        bFoundStruct = true;
                        break;
                    }
                }
            }

            if (!bFoundStruct)
            {
                // Try Enum
                if (UEnum* EnumResolve = FindObject<UEnum>(nullptr, *CleanType))
                {
                    PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
                    PinType.PinSubCategoryObject = EnumResolve;
                }
                else if (UEnum* LoadedEnum = LoadObject<UEnum>(nullptr, *CleanType))
                {
                    PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
                    PinType.PinSubCategoryObject = LoadedEnum;
                }
                else
                {
                    // Default to wildcard
                    PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
                }
            }
        }
    }
    
    return PinType;
}

FString DescribePinType(const FEdGraphPinType& PinType)
{
    FString BaseType = PinType.PinCategory.ToString();

    if (PinType.PinSubCategoryObject.IsValid())
    {
        if (const UObject* SubObj = PinType.PinSubCategoryObject.Get())
        {
            BaseType = SubObj->GetName();
        }
    }
    else if (PinType.PinSubCategory != NAME_None)
    {
        BaseType = PinType.PinSubCategory.ToString();
    }

    FString ContainerWrappedType = BaseType;
    switch (PinType.ContainerType)
    {
    case EPinContainerType::Array:
        ContainerWrappedType = FString::Printf(TEXT("Array<%s>"), *BaseType);
        break;
    case EPinContainerType::Set:
        ContainerWrappedType = FString::Printf(TEXT("Set<%s>"), *BaseType);
        break;
    case EPinContainerType::Map:
    {
        FString ValueType = PinType.PinValueType.TerminalCategory.ToString();
        if (PinType.PinValueType.TerminalSubCategoryObject.IsValid())
        {
            if (const UObject* ValueObj = PinType.PinValueType.TerminalSubCategoryObject.Get())
            {
                ValueType = ValueObj->GetName();
            }
        }
        else if (PinType.PinValueType.TerminalSubCategory != NAME_None)
        {
            ValueType = PinType.PinValueType.TerminalSubCategory.ToString();
        }
        ContainerWrappedType = FString::Printf(TEXT("Map<%s,%s>"), *BaseType, *ValueType);
        break;
    }
    default:
        break;
    }

    return ContainerWrappedType;
}

UK2Node_VariableGet* CreateVariableGetter(UEdGraph* Graph, const FMemberReference& VarRef, float NodePosX, float NodePosY)
{
    if (!Graph)
    {
        return nullptr;
    }

    UK2Node_VariableGet* NewGet = NewObject<UK2Node_VariableGet>(Graph);
    if (!NewGet)
    {
        return nullptr;
    }

    Graph->Modify();
    NewGet->SetFlags(RF_Transactional);
    NewGet->VariableReference = VarRef;
    Graph->AddNode(NewGet, true, false);
    NewGet->CreateNewGuid();
    NewGet->NodePosX = NodePosX;
    NewGet->NodePosY = NodePosY;
    NewGet->AllocateDefaultPins();
    NewGet->Modify();
    
    return NewGet;
}

void LogConnectionFailure(const TCHAR* Context, UEdGraphPin* SourcePin, UEdGraphPin* TargetPin, const FPinConnectionResponse& Response)
{
    if (!SourcePin || !TargetPin)
    {
        UE_LOG(LogTemp, Verbose, TEXT("%s: connection skipped due to null pins (source=%p target=%p)"),
            Context, SourcePin, TargetPin);
        return;
    }

    FString SourceNodeName = SourcePin->GetOwningNode() ? SourcePin->GetOwningNode()->GetName() : TEXT("<null>");
    FString TargetNodeName = TargetPin->GetOwningNode() ? TargetPin->GetOwningNode()->GetName() : TEXT("<null>");

    UE_LOG(LogTemp, Verbose, TEXT("%s: schema rejected connection %s (%s) -> %s (%s) reason=%d"),
        Context, *SourceNodeName, *SourcePin->PinName.ToString(),
        *TargetNodeName, *TargetPin->PinName.ToString(),
        static_cast<int32>(Response.Response));
}

TArray<TSharedPtr<FJsonValue>> CollectBlueprintVariables(UBlueprint* Blueprint)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    if (!Blueprint)
    {
        return Out;
    }

    for (const FBPVariableDescription& Var : Blueprint->NewVariables)
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
        
        Out.Add(MakeShared<FJsonValueObject>(Obj));
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

} // namespace McpBlueprintUtils

#endif // WITH_EDITOR && MCP_HAS_EDGRAPH_SCHEMA_K2