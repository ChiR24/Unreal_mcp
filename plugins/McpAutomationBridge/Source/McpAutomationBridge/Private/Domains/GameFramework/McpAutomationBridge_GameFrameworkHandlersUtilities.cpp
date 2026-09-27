#include "Domains/GameFramework/McpAutomationBridge_GameFrameworkHandlersContext.h"

namespace McpGameFrameworkHandlers
{
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

}
