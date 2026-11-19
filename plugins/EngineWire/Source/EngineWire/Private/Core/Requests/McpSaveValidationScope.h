// Copyright (c) 2025 EngineWire Contributors

#pragma once

#include "Editor.h"
#include "EditorSubsystem.h"
#include "UObject/UnrealType.h"

// The engine's save-time Data Validation runs after each save and opens the Message Log for every warning or error a
// validator reports: on a minimized editor that is a window of its own, taking focus on the user's screen, at every
// MCP write that saves an asset with a lint finding. An MCP request's reply runs the same validators itself and
// carries what they find in warnings, so saves inside a request skip the engine's pass; the user's own saves keep it.
// The switch is found by reflection: UDataValidationSettings::bValidateOnSave, else the validator subsystem's own
// flag on engines without that class. The engine reads it when a saved package is queued for validation, so holding
// it for the request covers saves the request makes on later ticks too.
struct FMcpSaveValidationScope
{
	FMcpSaveValidationScope()
	{
		if (UClass* Settings = FindObject<UClass>(nullptr, TEXT("/Script/DataValidation.DataValidationSettings")))
		{
			Owner = Settings->GetDefaultObject();
		}
		else if (UClass* Subsystem = FindObject<UClass>(nullptr, TEXT("/Script/DataValidation.EditorValidatorSubsystem")))
		{
			Owner = GEditor && Subsystem->IsChildOf(UEditorSubsystem::StaticClass()) ? GEditor->GetEditorSubsystemBase(Subsystem) : nullptr;
		}
		Flag = Owner ? FindFProperty<FBoolProperty>(Owner->GetClass(), TEXT("bValidateOnSave")) : nullptr;
		if (Flag)
		{
			bWas = Flag->GetPropertyValue_InContainer(Owner);
			Flag->SetPropertyValue_InContainer(Owner, false);
		}
	}

	~FMcpSaveValidationScope()
	{
		if (Flag && IsValid(Owner))
		{
			Flag->SetPropertyValue_InContainer(Owner, bWas);
		}
	}

	FMcpSaveValidationScope(const FMcpSaveValidationScope&) = delete;
	FMcpSaveValidationScope& operator=(const FMcpSaveValidationScope&) = delete;

private:
	UObject* Owner = nullptr;
	FBoolProperty* Flag = nullptr;
	bool bWas = true;
};
