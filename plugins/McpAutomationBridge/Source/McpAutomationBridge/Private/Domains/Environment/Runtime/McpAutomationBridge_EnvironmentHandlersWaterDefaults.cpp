#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

// A water body spawned outside the editor's own factory has no surface material and no waves, so it
// renders nothing and lies flat. These fill what it lacks from the same settings that factory reads
// (UWaterBodyActorFactory), reached by reflection so the bridge needs no link to the Water modules.
namespace McpEnvironmentHandlers {

namespace {
// The UWaterEditorSettings member holding this kind of body's defaults: WaterBodyOceanDefaults and so on.
FName McpWaterDefaultsName(const AActor *WaterActor)
{
    for (const UClass *Class = WaterActor->GetClass(); Class; Class = Class->GetSuperClass())
    {
        const FString Name = Class->GetName();
        if (Name == TEXT("WaterBodyOcean") || Name == TEXT("WaterBodyLake") || Name == TEXT("WaterBodyRiver") || Name == TEXT("WaterBodyCustom"))
        {
            return FName(*(Name + TEXT("Defaults")));
        }
    }
    return NAME_None;
}

UObject *McpLoadSoftMember(const UStruct *Struct, const void *Container, const TCHAR *Name)
{
    const FSoftObjectProperty *Property = CastField<FSoftObjectProperty>(Struct->FindPropertyByName(FName(Name)));
    return Property ? Property->GetPropertyValue_InContainer(Container).LoadSynchronous() : nullptr;
}

// Sets one material the body's component lacks, with the change notice the details panel sends so the
// water mesh is rebuilt with it.
bool McpFillWaterMaterial(UObject *Component, const TCHAR *Name, UObject *Material)
{
    FObjectProperty *Property = CastField<FObjectProperty>(Component->GetClass()->FindPropertyByName(FName(Name)));
    if (!Property || !Material || !Material->IsA(Property->PropertyClass) || Property->GetObjectPropertyValue_InContainer(Component))
    {
        return false;
    }
    Component->Modify();
    Property->SetObjectPropertyValue_InContainer(Component, Material);
    FPropertyChangedEvent Event(Property);
    Component->PostEditChangeProperty(Event);
    return true;
}
} // namespace

bool McpAssignWaterWaves(AActor *WaterActor, UObject *Waves)
{
    WaterActor->Modify();
    if (!McpInvokeObjectSetter(WaterActor, FName(TEXT("SetWaterWaves")), Waves))
    {
        McpSetObjectPropertyValue(WaterActor, TEXT("WaterWaves"), Waves);
    }
    return Waves && McpGetObjectPropertyValue(WaterActor, TEXT("WaterWaves")) == Waves;
}

void McpApplyWaterBodyDefaults(AActor *WaterActor, TSharedPtr<FJsonObject> Resp)
{
    UClass *SettingsClass = FindObject<UClass>(nullptr, TEXT("/Script/WaterEditor.WaterEditorSettings"));
    UObject *Settings = SettingsClass ? SettingsClass->GetDefaultObject() : nullptr;
    const FName DefaultsName = McpWaterDefaultsName(WaterActor);
    const FStructProperty *DefaultsProperty = Settings && !DefaultsName.IsNone()
        ? CastField<FStructProperty>(SettingsClass->FindPropertyByName(DefaultsName)) : nullptr;
    UObject *Component = McpGetObjectPropertyValue(WaterActor, TEXT("WaterBodyComponent"));
    if (!DefaultsProperty || !Component)
    {
        return;
    }
    const void *Defaults = DefaultsProperty->ContainerPtrToValuePtr<void>(Settings);
    TArray<FString> Filled;
    for (const TCHAR *Name : {TEXT("WaterMaterial"), TEXT("WaterStaticMeshMaterial"), TEXT("WaterHLODMaterial"), TEXT("UnderwaterPostProcessMaterial")})
    {
        if (McpFillWaterMaterial(Component, Name, McpLoadSoftMember(DefaultsProperty->Struct, Defaults, Name)))
        {
            Filled.Add(Name);
        }
    }
    UClass *RuntimeClass = FindObject<UClass>(nullptr, TEXT("/Script/Water.WaterRuntimeSettings"));
    if (RuntimeClass && McpFillWaterMaterial(Component, TEXT("WaterInfoMaterial"),
            McpLoadSoftMember(RuntimeClass, RuntimeClass->GetDefaultObject(), TEXT("DefaultWaterInfoMaterial"))))
    {
        Filled.Add(TEXT("WaterInfoMaterial"));
    }
    // An ocean or lake takes its own copy of the default waves, as the factory gives it.
    const FObjectProperty *WavesProperty = CastField<FObjectProperty>(DefaultsProperty->Struct->FindPropertyByName(TEXT("WaterWaves")));
    UObject *DefaultWaves = WavesProperty ? WavesProperty->GetObjectPropertyValue_InContainer(Defaults) : nullptr;
    if (DefaultWaves && !McpGetObjectPropertyValue(WaterActor, TEXT("WaterWaves")))
    {
        UObject *Waves = DuplicateObject(DefaultWaves, WaterActor, MakeUniqueObjectName(WaterActor, DefaultWaves->GetClass(), TEXT("WaterWaves")));
        if (McpAssignWaterWaves(WaterActor, Waves))
        {
            Filled.Add(TEXT("WaterWaves"));
        }
    }
    if (Filled.Num() > 0)
    {
        McpAddStringArrayField(Resp, TEXT("defaultsApplied"), Filled);
    }
}

} // namespace McpEnvironmentHandlers
