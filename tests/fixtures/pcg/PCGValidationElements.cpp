#include "PCGValidationElements.h"
#include "PCGContext.h"
#include "PCGComponent.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "PCGModule.h"
#include "Curves/CurveFloat.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
struct FValidationDelayContext : FPCGContext
{
    double Started = -1.0;
    TStrongObjectPtr<UObject> Scratch;
};

class FValidationDelayElement : public IPCGElementWithCustomContext<FValidationDelayContext>
{
public:
    virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
    virtual bool IsCacheable(const UPCGSettings* Settings) const override { return false; }

protected:
    virtual bool ExecuteInternal(FPCGContext* InContext) const override
    {
        FValidationDelayContext* Context = static_cast<FValidationDelayContext*>(InContext);
        const UPCGValidationDelaySettings* Settings = Context->GetInputSettings<UPCGValidationDelaySettings>();
        if (!Settings) return true;
        if (Context->Started < 0.0)
        {
            Context->Started = FPlatformTime::Seconds();
            if (Settings->bCreateTransientChild)
            {
                Context->Scratch.Reset(NewObject<UCurveFloat>(const_cast<UPCGValidationDelaySettings*>(Settings), NAME_None, RF_Transient));
            }
        }
        if (FPlatformTime::Seconds() - Context->Started >= Settings->DelaySeconds)
        {
            Context->OutputData = Context->InputData;
            for (FPCGTaggedData& Tagged : Context->OutputData.TaggedData) Tagged.Pin = PCGPinConstants::DefaultOutputLabel;
            return true;
        }
        Context->bIsPaused = true;
        FPCGModule::GetPCGModuleChecked().ExecuteNextTick([Handle = Context->GetOrCreateHandle()]()
        {
            FPCGContext::FSharedContext<FValidationDelayContext> Shared(Handle);
            if (FValidationDelayContext* State = Shared.Get()) State->bIsPaused = false;
        });
        return false;
    }
};
}

int64 UPCGValidationDelaySettings::ReplaceInFlightGeneration(UPCGComponent* Component)
{
    if (!IsInGameThread() || !IsValid(Component) || !Component->GetWorld() ||
        Component->GetWorld()->WorldType != EWorldType::Editor ||
        FCString::Strcmp(FApp::GetProjectName(), TEXT("PCGValidation")) != 0 ||
        !Component->GetPathName().StartsWith(TEXT("/Game/MCP_PCG_Validation/Lifecycle_")) ||
        Component->IsPartitioned() || Component->IsManagedByRuntimeGenSystem() ||
        !Component->IsGenerating() || Component->IsCleaningUp()) return -1;

    // Replace real native work in one game-thread turn; deferred Refresh may only cancel it.
    Component->CancelGeneration();
    if (Component->IsGenerating() || Component->IsCleaningUp()) return -1;
    const FPCGTaskId Replacement = Component->GenerateLocalGetTaskId(true);
    return Replacement == InvalidPCGTaskId ? -1 : static_cast<int64>(Replacement);
}

TArray<FPCGPinProperties> UPCGValidationDelaySettings::InputPinProperties() const
{
    return { FPCGPinProperties(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point) };
}

TArray<FPCGPinProperties> UPCGValidationDelaySettings::OutputPinProperties() const
{
    return { FPCGPinProperties(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point) };
}

FPCGElementPtr UPCGValidationDelaySettings::CreateElement() const
{
    return MakeShared<FValidationDelayElement>();
}

UPCGValidationBlueprintElement::UPCGValidationBlueprintElement()
{
    bHasDefaultInPin = false;
    bHasDefaultOutPin = false;
    CustomInputPins.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
    CustomOutputPins.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
}

void UPCGValidationBlueprintElement::Execute_Implementation(const FPCGDataCollection& Input, FPCGDataCollection& Output)
{
    Output = Input;
    for (FPCGTaggedData& Tagged : Output.TaggedData)
    {
        Tagged.Pin = PCGPinConstants::DefaultOutputLabel;
        Tagged.Tags.Add(OutputTag);
    }
}
