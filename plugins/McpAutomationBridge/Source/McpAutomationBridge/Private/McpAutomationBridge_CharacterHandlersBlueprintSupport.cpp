#include "McpAutomationBridge_CharacterHandlers.h"

#if WITH_EDITOR
namespace McpCharacterHandlers
{
UBlueprint* CreateCharacterBlueprintAsset(const FString& Path, const FString& Name, FString& OutError)
{
    const FString FullPath = Path / Name;
    if (!IsValidAssetPath(FullPath))
    {
        OutError = FString::Printf(TEXT("Invalid asset path: '%s'. Path must start with '/', cannot contain '..' or '//'."), *FullPath);
        return nullptr;
    }
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        OutError = FString::Printf(TEXT("Asset already exists at path: %s"), *FullPath);
        return nullptr;
    }

    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = ACharacter::StaticClass();
    UBlueprint* Blueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(
        UBlueprint::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn));
    if (!Blueprint)
    {
        OutError = TEXT("Failed to create character blueprint");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Blueprint);
    Blueprint->MarkPackageDirty();
    return Blueprint;
}

UBlueprint* LoadCharacterBlueprint(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& BlueprintPath, FCharacterSocket Socket)
{
    if (BlueprintPath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
        return nullptr;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
    }
    return Blueprint;
}

void SetBPVarDefaultValue(UBlueprint* Blueprint, FName VarName, const FString& DefaultValue)
{
    if (!Blueprint)
    {
        return;
    }

    bool bUpdatedVariableDescription = false;
    for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
    {
        if (VarDesc.VarName == VarName)
        {
            VarDesc.DefaultValue = DefaultValue;
            bUpdatedVariableDescription = true;
            break;
        }
    }

    bool bAppliedToCDO = false;
    McpSafeCompileBlueprint(Blueprint);
    if (Blueprint->GeneratedClass)
    {
        if (UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject())
        {
            if (FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName))
            {
                void* ValuePtr = Property->ContainerPtrToValuePtr<void>(CDO);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                Property->ImportText_Direct(*DefaultValue, ValuePtr, CDO, 0);
#else
                Property->ImportText(*DefaultValue, ValuePtr, PPF_None, CDO);
#endif
                bAppliedToCDO = true;
            }
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    Blueprint->MarkPackageDirty();
    if (!bUpdatedVariableDescription && !bAppliedToCDO)
    {
        UE_LOG(LogMcpCharacterHandlers, Warning,
               TEXT("Variable '%s' default value could not be applied; variable was not found on Blueprint '%s'."),
               *VarName.ToString(), *Blueprint->GetName());
    }
}

bool AddBlueprintVariable(UBlueprint* Blueprint, const FString& VarName, const FEdGraphPinType& PinType, const FString& Category)
{
    if (!Blueprint)
    {
        return false;
    }

    const bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VarName), PinType);
    if (bSuccess && !Category.IsEmpty())
    {
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*VarName), nullptr, FText::FromString(Category));
    }
    return bSuccess;
}

FEdGraphPinType BoolPinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    return PinType;
}

FEdGraphPinType FloatPinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
    PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    return PinType;
}

FEdGraphPinType IntPinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    return PinType;
}

FEdGraphPinType NamePinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
    return PinType;
}

FEdGraphPinType VectorPinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
    PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    return PinType;
}

FVector VectorFromJson(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid())
    {
        return FVector::ZeroVector;
    }
    return FVector(
        GetJsonNumberField(Obj, TEXT("x"), 0.0),
        GetJsonNumberField(Obj, TEXT("y"), 0.0),
        GetJsonNumberField(Obj, TEXT("z"), 0.0));
}

FRotator RotatorFromJson(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid())
    {
        return FRotator::ZeroRotator;
    }
    return FRotator(
        GetJsonNumberField(Obj, TEXT("pitch"), 0.0),
        GetJsonNumberField(Obj, TEXT("yaw"), 0.0),
        GetJsonNumberField(Obj, TEXT("roll"), 0.0));
}
}
#endif
