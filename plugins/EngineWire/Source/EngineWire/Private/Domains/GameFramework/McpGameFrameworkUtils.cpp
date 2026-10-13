#include "Domains/GameFramework/McpGameFrameworkContext.h"

namespace McpGameFrameworkHandlers
{
UClass* LoadClassFromPath(const FString& ClassPath)
{
    // A short name, a class path or a Blueprint path (with or without _C), resolved without logging a miss: appending
    // _C to a Blueprint asset path looked up '/Game/X/BP_Y_C', which never exists, and the reply carried "Failed to
    // find object" although the class then resolved.
    return ResolveClassByName(ClassPath);
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

    // A TSubclassOf only holds its meta class: writing an unrelated class (an Actor as the HUD)
    // is accepted by reflection and breaks the game mode at spawn time.
    UClass* MetaClass = nullptr;
    if (const FClassProperty* MetaProp = CastField<FClassProperty>(Prop)) MetaClass = MetaProp->MetaClass;
    if (const FSoftClassProperty* MetaSoftProp = CastField<FSoftClassProperty>(Prop)) MetaClass = MetaSoftProp->MetaClass;
    if (ClassToSet && MetaClass && !ClassToSet->IsChildOf(MetaClass))
    {
        OutError = FString::Printf(TEXT("%s is not a %s, which %s requires"),
            *ClassToSet->GetPathName(), *MetaClass->GetName(), *PropertyName.ToString());
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
