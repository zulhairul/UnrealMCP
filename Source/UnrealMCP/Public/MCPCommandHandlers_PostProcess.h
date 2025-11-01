#pragma once

#include "CoreMinimal.h"
#include "MCPCommandHandlers.h"

class APostProcessVolume;

/**
 * Handler that applies high-level color grading adjustments to a post process volume.
 */
class FMCPApplyColorGradingHandler : public FMCPCommandHandlerBase
{
public:
    FMCPApplyColorGradingHandler()
        : FMCPCommandHandlerBase(TEXT("apply_color_grading"))
    {
    }

    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket) override;

private:
    /**
     * Locate an existing post process volume or optionally create a new one.
     */
    APostProcessVolume* FindOrCreateTargetVolume(UWorld* World, const FString& RequestedNameOrLabel, bool bCreateIfMissing, FString& OutStatusMessage);

    /**
     * Apply supported color grading settings to the provided volume and track which overrides changed.
     */
    bool ApplySettings(APostProcessVolume* Volume, const TSharedPtr<FJsonObject>& SettingsObject, TSharedPtr<FJsonObject>& OutAppliedFields);

    /** Helper that extracts a 4D vector from the JSON payload. */
    bool TryExtractVector4(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, FVector4& OutVector) const;
};

