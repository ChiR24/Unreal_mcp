#include "McpAutomationBridge_GameFrameworkHandlersContext.h"

namespace McpGameFrameworkHandlers
{
#if WITH_EDITOR
UClass* LoadClassFromPath(const FString& ClassPath)
{
    if (ClassPath.IsEmpty()) return nullptr;

    if (UClass* NativeClass = FindObject<UClass>(nullptr, *ClassPath))
    {
        return NativeClass;
    }

    FString BPPath = ClassPath;
    if (!BPPath.EndsWith(TEXT("_C")))
    {
        BPPath += TEXT("_C");
    }

    if (UClass* BPClass = LoadClass<UObject>(nullptr, *BPPath))
    {
        return BPClass;
    }

    UBlueprint* Blueprint = LoadBlueprintFromPath(ClassPath);
    return Blueprint ? Blueprint->GeneratedClass : nullptr;
}

bool SetClassProperty(UBlueprint* Blueprint, const FName& PropertyName, UClass* ClassToSet, FString& OutError)
{
    if (!Blueprint || !Blueprint->GeneratedClass)
    {
        OutError = TEXT("Invalid blueprint or generated class");
        return false;
    }

    UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
    if (!CDO)
    {
        OutError = TEXT("Failed to get CDO");
        return false;
    }

    FProperty* Prop = Blueprint->GeneratedClass->FindPropertyByName(PropertyName);
    if (!Prop && Blueprint->ParentClass)
    {
        Prop = Blueprint->ParentClass->FindPropertyByName(PropertyName);
    }
    if (!Prop)
    {
        OutError = FString::Printf(TEXT("Property '%s' not found"), *PropertyName.ToString());
        return false;
    }

    if (FClassProperty* ClassProp = CastField<FClassProperty>(Prop))
    {
        ClassProp->SetPropertyValue_InContainer(CDO, ClassToSet);
        CDO->MarkPackageDirty();
        return true;
    }
    if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Prop))
    {
        FSoftObjectPtr SoftPtr(ClassToSet);
        SoftClassProp->SetPropertyValue_InContainer(CDO, SoftPtr);
        CDO->MarkPackageDirty();
        return true;
    }

    OutError = FString::Printf(TEXT("Property '%s' is not a class property"), *PropertyName.ToString());
    return false;
}

bool AddBlueprintVariable(UBlueprint* Blueprint, const FString& VarName, const FEdGraphPinType& PinType, const FString& Category)
{
    if (!Blueprint) return false;

    const bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VarName), PinType);
    if (bSuccess && !Category.IsEmpty())
    {
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*VarName), nullptr, FText::FromString(Category));
    }
    return bSuccess;
}

int32 AddVariable(UBlueprint* Blueprint, const FString& VarName, const FEdGraphPinType& PinType, const FString& Category)
{
    return AddBlueprintVariable(Blueprint, VarName, PinType, Category) ? 1 : 0;
}

int32 AddVariableWithDefault(
    UBlueprint* Blueprint,
    const FString& VarName,
    const FEdGraphPinType& PinType,
    const FString& Category,
    const FString& DefaultValue)
{
    if (!AddBlueprintVariable(Blueprint, VarName, PinType, Category)) return 0;
    SetVariableDefaultValue(Blueprint, VarName, DefaultValue);
    return 1;
}

void FinishBlueprintMutation(UBlueprint* Blueprint, bool bSave)
{
    McpSafeCompileBlueprint(Blueprint);
    Blueprint->MarkPackageDirty();
    if (bSave)
    {
        McpSafeAssetSave(Blueprint);
    }
}

TSharedPtr<FJsonObject> MakeBlueprintResponse(const FString& Message, UBlueprint* Blueprint)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), Message);
    Response->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    return Response;
}

static FEdGraphPinType MakePinType(const FName& Category, const FName& SubCategory = NAME_None)
{
    FEdGraphPinType PinType;
    PinType.PinCategory = Category;
    PinType.PinSubCategory = SubCategory;
    return PinType;
}

FEdGraphPinType MakeIntPinType() { return MakePinType(UEdGraphSchema_K2::PC_Int); }
FEdGraphPinType MakeFloatPinType() { return MakePinType(UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float); }
FEdGraphPinType MakeBoolPinType() { return MakePinType(UEdGraphSchema_K2::PC_Boolean); }
FEdGraphPinType MakeNamePinType() { return MakePinType(UEdGraphSchema_K2::PC_Name); }
FEdGraphPinType MakeBytePinType() { return MakePinType(UEdGraphSchema_K2::PC_Byte); }
#endif
}
