// create_pose_search_database and add_pose_search_entry (item 17's writes), by reflection only: no link to PoseSearch.
// The database is made directly: the plugin's factory (5.5-5.9) makes one only after its modal schema picker set a
// schema, and then does only NewObject plus that schema. An entry is one element of the
// PoseSearch.DatabaseEntries role: a plain struct holding the animation (5.7 and later) or an instanced struct of the
// entry kind for the animation's class (earlier engines that have one in CoreUObject).

#include "Domains/Animation/McpAnimActionContext.h"

#include "Animation/AnimationAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetCreation.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/BridgeHelpers/Security/McpProjectPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/Reflect/McpEngineRoles.h"
#include "Modules/ModuleManager.h"
#include "Safety/McpSafeOperations.h"
#include "UObject/UnrealType.h"
#if MCP_HAS_CORE_INSTANCED_STRUCT
#include "StructUtils/InstancedStruct.h"
#endif

namespace McpAnimPoseSearchWriteDetail
{
// The first object property of Struct that can hold Asset, set in Memory; false when none can.
bool SetAsset(const UStruct* Struct, void* Memory, UObject* Asset)
{
    for (TFieldIterator<FObjectPropertyBase> It(Struct); It; ++It)
    {
        if (It->PropertyClass && Asset->IsA(It->PropertyClass))
        {
            It->SetObjectPropertyValue_InContainer(Memory, Asset);
            return true;
        }
    }
    return false;
}
} // namespace McpAnimPoseSearchWriteDetail

namespace McpAnimationHandlers
{
bool HandleCreatePoseSearchDatabase(FActionContext& Context, const TSharedPtr<FJsonObject>& Payload)
{
    FModuleManager::Get().LoadModule(TEXT("PoseSearch"));
    UClass* DatabaseClass = Cast<UClass>(McpEngineRoles::TypeOf(TEXT("PoseSearch.Database")));
    if (!DatabaseClass)
    {
        Context.Fail(TEXT("FEATURE_UNAVAILABLE"), TEXT("Pose Search is not available here: enable the PoseSearch plugin (system_control enable_plugin) and restart the editor."));
        return false;
    }
    const FString Name = GetJsonStringField(Payload, TEXT("name"));
    const FString Folder = SanitizeProjectRelativePath(GetJsonStringField(Payload, TEXT("path"), TEXT("/Game")));
    FString Error, Code;
    UPackage* Package = Name.IsEmpty() || Folder.IsEmpty() ? nullptr : McpCreatePackageForNewAsset(Folder / Name, Error, Code);
    if (!Package)
    {
        Context.Fail(Code.IsEmpty() ? TEXT("INVALID_ARGUMENT") : *Code, Error.IsEmpty() ? FString(TEXT("name and a /Game path are required.")) : Error);
        return false;
    }
    UObject* Database = NewObject<UObject>(Package, DatabaseClass, FName(*Name), RF_Public | RF_Standalone | RF_Transactional);
    const FString SchemaPath = GetJsonStringField(Payload, TEXT("schemaPath"));
    FObjectPropertyBase* Schema = CastField<FObjectPropertyBase>(McpEngineRoles::PropertyOf(TEXT("PoseSearch.DatabaseSchema")));
    UObject* SchemaAsset = SchemaPath.IsEmpty() ? nullptr : McpLoadAsset(SchemaPath);
    if (Database && Schema && SchemaAsset && SchemaAsset->IsA(Schema->PropertyClass))
    {
        Schema->SetObjectPropertyValue_InContainer(Database, SchemaAsset);
    }
    if (!Database)
    {
        Context.Fail(TEXT("CREATE_FAILED"), TEXT("The Pose Search database could not be made."));
        return false;
    }
    FAssetRegistryModule::AssetCreated(Database);
    McpSafeAssetSave(Database);
    Context.Resp->SetStringField(TEXT("databasePath"), Database->GetPathName());
    Context.Resp->SetBoolField(TEXT("schemaSet"), SchemaAsset != nullptr && Schema && Schema->GetObjectPropertyValue_InContainer(Database) == SchemaAsset);
    Context.bSuccess = true;
    Context.Message = FString::Printf(TEXT("Created Pose Search database %s"), *Database->GetName());
    return false;
}

bool HandleAddPoseSearchEntry(FActionContext& Context, const TSharedPtr<FJsonObject>& Payload)
{
    using namespace McpAnimPoseSearchWriteDetail;
    const UClass* DatabaseClass = Cast<UClass>(McpEngineRoles::TypeOf(TEXT("PoseSearch.Database")));
    const FArrayProperty* Entries = CastField<FArrayProperty>(McpEngineRoles::PropertyOf(TEXT("PoseSearch.DatabaseEntries")));
    const FStructProperty* Inner = Entries ? CastField<FStructProperty>(Entries->Inner) : nullptr;
    if (!DatabaseClass || !Inner)
    {
        Context.Fail(TEXT("FEATURE_UNAVAILABLE"), TEXT("Pose Search is not available here: enable the PoseSearch plugin (system_control enable_plugin) and restart the editor."));
        return false;
    }
    UObject* Database = McpLoadAsset(GetJsonStringField(Payload, TEXT("databasePath")));
    UObject* Animation = McpLoadAsset(GetJsonStringField(Payload, TEXT("animationPath")));
    if (!Database || !Database->IsA(DatabaseClass) || !Cast<UAnimationAsset>(Animation))
    {
        Context.Fail(TEXT("NOT_FOUND"), TEXT("databasePath must name a Pose Search database and animationPath an animation sequence, blend space, montage or composite."));
        return false;
    }
    Database->Modify();
    FScriptArrayHelper Helper(Entries, Entries->ContainerPtrToValuePtr<void>(Database));
    const int32 Index = Helper.AddValue();
    bool bSet = false;
    if (Inner->Struct->GetName() != TEXT("InstancedStruct"))
    {
        bSet = SetAsset(Inner->Struct, Helper.GetRawPtr(Index), Animation);
    }
#if MCP_HAS_CORE_INSTANCED_STRUCT
    else
    {
        // The entry kind for the animation's class: PoseSearchDatabaseSequence, ...BlendSpace, ...AnimMontage, ...AnimComposite.
        const FString Kind = Animation->GetClass()->GetName().Replace(TEXT("Anim"), TEXT(""), ESearchCase::CaseSensitive);
        const UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *(TEXT("/Script/PoseSearch.PoseSearchDatabase") + Kind));
        Struct = Struct ? Struct : FindObject<UScriptStruct>(nullptr, *(TEXT("/Script/PoseSearch.PoseSearchDatabase") + Animation->GetClass()->GetName()));
        FInstancedStruct& Entry = *reinterpret_cast<FInstancedStruct*>(Helper.GetRawPtr(Index));
        if (Struct)
        {
            Entry.InitializeAs(Struct);
            bSet = SetAsset(Struct, Entry.GetMutableMemory(), Animation);
        }
    }
#endif
    if (!bSet)
    {
        Helper.RemoveValues(Index, 1);
        Context.Fail(TEXT("NOT_SUPPORTED"), FString::Printf(TEXT("A %s cannot be an entry of this engine's Pose Search database."), *Animation->GetClass()->GetName()));
        return false;
    }
    FPropertyChangedEvent Changed(const_cast<FArrayProperty*>(Entries), EPropertyChangeType::ArrayAdd);
    Database->PostEditChangeProperty(Changed);
    McpSafeAssetSave(Database);
    Context.Resp->SetStringField(TEXT("databasePath"), Database->GetPathName());
    Context.Resp->SetNumberField(TEXT("index"), Index);
    Context.Resp->SetNumberField(TEXT("entryCount"), Helper.Num());
    Context.bSuccess = true;
    Context.Message = FString::Printf(TEXT("Added %s to %s as entry %d"), *Animation->GetName(), *Database->GetName(), Index);
    return false;
}
} // namespace McpAnimationHandlers
