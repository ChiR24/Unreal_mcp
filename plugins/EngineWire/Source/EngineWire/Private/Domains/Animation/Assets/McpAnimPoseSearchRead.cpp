// get_pose_search_database and get_chooser_table: Pose Search databases and Chooser tables read by reflection only (no
// link to either plugin). The entry array is the PoseSearch.DatabaseEntries role, an array of instanced structs on some
// engines and of one plain struct on others (Resources/EngineRoles/PoseSearch.json, Chooser.json).

#include "Domains/Animation/McpAnimActionContext.h"

#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/Reflect/McpEngineRoles.h"
#include "Foundation/Reflection/McpInstancedStructView.h"
#include "Foundation/Reflection/McpReflectedInvoke.h"
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

// get_chooser_table: each column and result of a Chooser table by its struct and the asset it names, and with
// contextObject what the table chooses for that object through the plugin's own EvaluateChooser (roles: Chooser.json).
bool HandleGetChooserTable(FActionContext& Context, const TSharedPtr<FJsonObject>& Payload)
{
    const UClass* TableClass = Cast<UClass>(McpEngineRoles::TypeOf(TEXT("Chooser.Table")));
    const FArrayProperty* Columns = CastField<FArrayProperty>(McpEngineRoles::PropertyOf(TEXT("Chooser.Columns")));
    const FArrayProperty* Results = CastField<FArrayProperty>(McpEngineRoles::PropertyOf(TEXT("Chooser.Results")));
    if (!TableClass || !Columns || !Results)
    {
        Context.Fail(TEXT("FEATURE_UNAVAILABLE"), TEXT("Chooser tables are not available here: enable the Chooser plugin (system_control enable_plugin) and restart the editor."));
        return false;
    }
    const FString Path = GetJsonStringField(Payload, TEXT("chooserPath"));
    UObject* Table = Path.IsEmpty() ? nullptr : McpLoadAsset(Path);
    if (!Table || !Table->IsA(TableClass))
    {
        Context.Fail(TEXT("NOT_FOUND"), FString::Printf(TEXT("No Chooser table at '%s'."), *Path));
        return false;
    }
    auto Rows = [Table](const FArrayProperty* Array)
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        FScriptArrayHelper Helper(Array, Array->ContainerPtrToValuePtr<void>(Table));
        for (int32 Index = 0; Index < Helper.Num(); ++Index)
        {
            const FMcpInstancedStructView View(*reinterpret_cast<const FInstancedStruct*>(Helper.GetRawPtr(Index)));
            TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetNumberField(TEXT("index"), Index);
            Row->SetStringField(TEXT("kind"), View.Type ? View.Type->GetName() : FString(TEXT("None")));
            const FString Asset = View.IsValid() ? McpAnimPoseSearchDetail::EntryAsset(View.Type, View.Memory) : FString();
            if (!Asset.IsEmpty()) Row->SetStringField(TEXT("asset"), Asset);
            Out.Add(MakeShared<FJsonValueObject>(Row));
        }
        return Out;
    };
    const TArray<TSharedPtr<FJsonValue>> ColumnRows = Rows(Columns);
    const TArray<TSharedPtr<FJsonValue>> ResultRows = Rows(Results);
    Context.Resp->SetStringField(TEXT("chooserPath"), Table->GetPathName());
    Context.Resp->SetArrayField(TEXT("columns"), ColumnRows);
    Context.Resp->SetArrayField(TEXT("results"), ResultRows);
    const FString ContextPath = GetJsonStringField(Payload, TEXT("contextObject"));
    UFunction* Evaluate = McpEngineRoles::FunctionOf(TEXT("Chooser.Evaluate"));
    const UClass* Library = Cast<UClass>(McpEngineRoles::TypeOf(TEXT("Chooser.Library")));
    if (!ContextPath.IsEmpty() && Evaluate && Library)
    {
        TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
        Args->SetStringField(TEXT("ContextObject"), ContextPath);
        Args->SetStringField(TEXT("ChooserTable"), Table->GetPathName());
        Args->SetStringField(TEXT("ObjectClass"), GetJsonStringField(Payload, TEXT("objectClass"), TEXT("/Script/CoreUObject.Object")));
        TSharedPtr<FJsonObject> Outputs;
        TArray<TSharedPtr<FJsonValue>> Unset;
        FString Error;
        if (!McpInvokeWithArgs(Library->GetDefaultObject(), Evaluate, Args, Outputs, Unset, Error))
        {
            Context.Fail(TEXT("EVALUATE_FAILED"), Error);
            return false;
        }
        Context.Resp->SetObjectField(TEXT("chosen"), Outputs);
    }
    Context.bSuccess = true;
    Context.Message = FString::Printf(TEXT("%s: %d column(s), %d result(s)"), *Table->GetName(), ColumnRows.Num(), ResultRows.Num());
    return false;
}
} // namespace McpAnimationHandlers
