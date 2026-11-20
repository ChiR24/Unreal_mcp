// get_pose_search_database: a Pose Search database read by reflection only (no link to the PoseSearch plugin): its
// schema and each animation entry, its kind and asset. The entry array is the PoseSearch.DatabaseEntries role, an array
// of instanced structs on some engines and of one plain struct on others (Resources/EngineRoles/PoseSearch.json).

#include "Domains/Animation/McpAnimActionContext.h"

#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/Reflect/McpEngineRoles.h"
#include "Foundation/Reflection/McpInstancedStructView.h"
#include "UObject/UnrealType.h"

namespace McpAnimPoseSearchDetail
{
// The first object an entry struct holds (its sequence, blend space or montage), by path.
FString EntryAsset(const UStruct* Type, const void* Memory)
{
    for (TFieldIterator<FProperty> It(Type); It; ++It)
    {
        if (const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(*It))
        {
            const FString Path = Soft->GetPropertyValue_InContainer(Memory).ToString();
            if (!Path.IsEmpty()) return Path;
        }
        else if (const FObjectPropertyBase* Hard = CastField<FObjectPropertyBase>(*It))
        {
            if (const UObject* Object = Hard->GetObjectPropertyValue_InContainer(Memory)) return Object->GetPathName();
        }
    }
    return FString();
}
} // namespace McpAnimPoseSearchDetail

namespace McpAnimationHandlers
{
bool HandleGetPoseSearchDatabase(FActionContext& Context, const TSharedPtr<FJsonObject>& Payload)
{
    using namespace McpAnimPoseSearchDetail;
    const UClass* DatabaseClass = Cast<UClass>(McpEngineRoles::TypeOf(TEXT("PoseSearch.Database")));
    const FArrayProperty* Entries = CastField<FArrayProperty>(McpEngineRoles::PropertyOf(TEXT("PoseSearch.DatabaseEntries")));
    if (!DatabaseClass || !Entries)
    {
        Context.Fail(TEXT("FEATURE_UNAVAILABLE"), TEXT("Pose Search is not available here: enable the PoseSearch plugin (system_control enable_plugin) and restart the editor."));
        return false;
    }
    const FString Path = GetJsonStringField(Payload, TEXT("databasePath"));
    UObject* Database = Path.IsEmpty() ? nullptr : McpLoadAsset(Path);
    if (!Database || !Database->IsA(DatabaseClass))
    {
        Context.Fail(TEXT("NOT_FOUND"), FString::Printf(TEXT("No Pose Search database at '%s'."), *Path));
        return false;
    }
    if (const FObjectPropertyBase* Schema = CastField<FObjectPropertyBase>(McpEngineRoles::PropertyOf(TEXT("PoseSearch.DatabaseSchema"))))
    {
        const UObject* Value = Schema->GetObjectPropertyValue_InContainer(Database);
        Context.Resp->SetStringField(TEXT("schema"), Value ? Value->GetPathName() : FString());
    }
    const FStructProperty* Inner = CastField<FStructProperty>(Entries->Inner);
    const bool bInstanced = Inner && Inner->Struct->GetName() == TEXT("InstancedStruct");
    FScriptArrayHelper Helper(Entries, Entries->ContainerPtrToValuePtr<void>(Database));
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 Index = 0; Inner && Index < Helper.Num(); ++Index)
    {
        const UStruct* Type = Inner->Struct;
        const void* Memory = Helper.GetRawPtr(Index);
        if (bInstanced)
        {
            const FMcpInstancedStructView View(*reinterpret_cast<const FInstancedStruct*>(Memory));
            Type = View.Type;
            Memory = View.Memory;
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("index"), Index);
        Row->SetStringField(TEXT("kind"), Type ? Type->GetName() : FString(TEXT("None")));
        Row->SetStringField(TEXT("asset"), Type && Memory ? EntryAsset(Type, Memory) : FString());
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Context.Resp->SetStringField(TEXT("databasePath"), Database->GetPathName());
    Context.Resp->SetStringField(TEXT("entriesProperty"), Entries->GetName());
    Context.Resp->SetArrayField(TEXT("entries"), Rows);
    Context.Resp->SetNumberField(TEXT("entryCount"), Rows.Num());
    Context.bSuccess = true;
    Context.Message = FString::Printf(TEXT("%s: %d animation entr%s"), *Database->GetName(), Rows.Num(), Rows.Num() == 1 ? TEXT("y") : TEXT("ies"));
    return false;
}
} // namespace McpAnimationHandlers
