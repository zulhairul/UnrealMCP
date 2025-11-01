#include "MCPCommandHandlers_PostProcess.h"

#include "Editor.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "MCPFileLogger.h"

namespace
{
    /** Utility that tries to load a texture asset for LUT support. */
    UTexture* LoadTextureOptional(const FString& AssetPath)
    {
        if (AssetPath.IsEmpty())
        {
            return nullptr;
        }

        return Cast<UTexture>(StaticLoadObject(UTexture::StaticClass(), nullptr, *AssetPath));
    }

    FString SafeActorName(APostProcessVolume* Volume)
    {
        if (!Volume)
        {
            return TEXT("<null>");
        }

        return FString::Printf(TEXT("%s (%s)"), *Volume->GetActorLabel(), *Volume->GetName());
    }
}

TSharedPtr<FJsonObject> FMCPApplyColorGradingHandler::Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket)
{
    MCP_LOG_INFO("Handling apply_color_grading command");

    if (!Params.IsValid())
    {
        return CreateErrorResponse(TEXT("Missing parameters for apply_color_grading"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        MCP_LOG_ERROR("Editor world is not available when attempting to apply color grading");
        return CreateErrorResponse(TEXT("Editor world is not available"));
    }

    FString RequestedVolumeName;
    Params->TryGetStringField(FStringView(TEXT("volume_name")), RequestedVolumeName);

    bool bCreateIfMissing = true;
    Params->TryGetBoolField(FStringView(TEXT("create_if_missing")), bCreateIfMissing);

    const TSharedPtr<FJsonObject>* SettingsObject = nullptr;
    if (!Params->TryGetObjectField(FStringView(TEXT("settings")), SettingsObject) || !SettingsObject)
    {
        MCP_LOG_WARNING("apply_color_grading command missing 'settings' object");
        return CreateErrorResponse(TEXT("Missing 'settings' object"));
    }

    FString StatusMessage;
    APostProcessVolume* TargetVolume = FindOrCreateTargetVolume(World, RequestedVolumeName, bCreateIfMissing, StatusMessage);
    if (!TargetVolume)
    {
        MCP_LOG_ERROR("Failed to resolve target post process volume for apply_color_grading: %s", *StatusMessage);
        return CreateErrorResponse(StatusMessage.IsEmpty() ? TEXT("Failed to resolve post process volume") : StatusMessage);
    }

    TargetVolume->Modify();

    TSharedPtr<FJsonObject> AppliedSettings = MakeShared<FJsonObject>();
    const bool bAppliedAnySetting = ApplySettings(TargetVolume, *SettingsObject, AppliedSettings);
    if (!bAppliedAnySetting)
    {
        MCP_LOG_WARNING("apply_color_grading called but no supported settings were provided");
        return CreateErrorResponse(TEXT("No supported color grading settings supplied"));
    }

    TargetVolume->PostEditChange();
    GEditor->RedrawAllViewports(false);

    TSharedPtr<FJsonObject> ResultObject = MakeShared<FJsonObject>();
    ResultObject->SetStringField(TEXT("volume_name"), TargetVolume->GetName());
    ResultObject->SetStringField(TEXT("volume_label"), TargetVolume->GetActorLabel());
    ResultObject->SetBoolField(TEXT("unbound"), TargetVolume->bUnbound);
    ResultObject->SetNumberField(TEXT("blend_weight"), TargetVolume->BlendWeight);
    ResultObject->SetNumberField(TEXT("priority"), TargetVolume->Priority);
    if (!StatusMessage.IsEmpty())
    {
        ResultObject->SetStringField(TEXT("message"), StatusMessage);
    }
    ResultObject->SetObjectField(TEXT("applied_overrides"), AppliedSettings);

    MCP_LOG_INFO("Successfully applied color grading overrides to %s", *SafeActorName(TargetVolume));
    return CreateSuccessResponse(ResultObject);
}

APostProcessVolume* FMCPApplyColorGradingHandler::FindOrCreateTargetVolume(UWorld* World, const FString& RequestedNameOrLabel, bool bCreateIfMissing, FString& OutStatusMessage)
{
    if (!World)
    {
        OutStatusMessage = TEXT("World is invalid");
        return nullptr;
    }

    APostProcessVolume* TargetVolume = nullptr;

    // First attempt: match explicit name or label if provided.
    if (!RequestedNameOrLabel.IsEmpty())
    {
        for (TActorIterator<APostProcessVolume> It(World); It; ++It)
        {
            if (It->GetName().Equals(RequestedNameOrLabel, ESearchCase::IgnoreCase) ||
                It->GetActorLabel().Equals(RequestedNameOrLabel, ESearchCase::IgnoreCase))
            {
                TargetVolume = *It;
                break;
            }
        }

        if (!TargetVolume)
        {
            MCP_LOG_WARNING("Requested post process volume '%s' was not found", *RequestedNameOrLabel);
        }
    }

    // Second attempt: prefer an existing unbound volume for global adjustments.
    if (!TargetVolume)
    {
        for (TActorIterator<APostProcessVolume> It(World); It; ++It)
        {
            if (It->bUnbound)
            {
                TargetVolume = *It;
                break;
            }
        }
    }

    // Third attempt: grab the first available post process volume.
    if (!TargetVolume)
    {
        for (TActorIterator<APostProcessVolume> It(World); It; ++It)
        {
            TargetVolume = *It;
            break;
        }
    }

    if (TargetVolume)
    {
        OutStatusMessage = FString::Printf(TEXT("Using post process volume %s"), *SafeActorName(TargetVolume));
        return TargetVolume;
    }

    if (!bCreateIfMissing)
    {
        OutStatusMessage = RequestedNameOrLabel.IsEmpty() ? TEXT("No post process volumes exist in the scene") :
            FString::Printf(TEXT("Post process volume '%s' not found"), *RequestedNameOrLabel);
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    if (!RequestedNameOrLabel.IsEmpty())
    {
        SpawnParams.Name = *RequestedNameOrLabel;
    }

    APostProcessVolume* NewVolume = World->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    if (!NewVolume)
    {
        OutStatusMessage = TEXT("Failed to create a new post process volume");
        return nullptr;
    }

    NewVolume->bUnbound = true; // Default to affecting the whole scene.
    if (!RequestedNameOrLabel.IsEmpty())
    {
        NewVolume->SetActorLabel(RequestedNameOrLabel);
    }
    else
    {
        NewVolume->SetActorLabel(TEXT("MCP_ColorGrading"));
    }

    OutStatusMessage = FString::Printf(TEXT("Created new unbound post process volume %s"), *SafeActorName(NewVolume));
    return NewVolume;
}

bool FMCPApplyColorGradingHandler::ApplySettings(APostProcessVolume* Volume, const TSharedPtr<FJsonObject>& SettingsObject, TSharedPtr<FJsonObject>& OutAppliedFields)
{
    if (!Volume || !SettingsObject.IsValid())
    {
        return false;
    }

    bool bAppliedAny = false;
    FPostProcessSettings& Settings = Volume->Settings;

    auto CaptureVectorField = [&](const FString& FieldName, FVector4& InOutValue, bool& bOverrideFlag, const FString& OutputName)
    {
        FVector4 ParsedVector;
        if (TryExtractVector4(SettingsObject, FieldName, ParsedVector))
        {
            bOverrideFlag = true;
            InOutValue = ParsedVector;
            TArray<TSharedPtr<FJsonValue>> VectorJson;
            VectorJson.Add(MakeShared<FJsonValueNumber>(ParsedVector.X));
            VectorJson.Add(MakeShared<FJsonValueNumber>(ParsedVector.Y));
            VectorJson.Add(MakeShared<FJsonValueNumber>(ParsedVector.Z));
            VectorJson.Add(MakeShared<FJsonValueNumber>(ParsedVector.W));
            OutAppliedFields->SetArrayField(OutputName, VectorJson);
            bAppliedAny = true;
        }
    };

    CaptureVectorField(TEXT("color_saturation"), Settings.ColorSaturation, Settings.bOverride_ColorSaturation, TEXT("color_saturation"));
    CaptureVectorField(TEXT("color_contrast"), Settings.ColorContrast, Settings.bOverride_ColorContrast, TEXT("color_contrast"));
    CaptureVectorField(TEXT("color_gamma"), Settings.ColorGamma, Settings.bOverride_ColorGamma, TEXT("color_gamma"));
    CaptureVectorField(TEXT("color_gain"), Settings.ColorGain, Settings.bOverride_ColorGain, TEXT("color_gain"));
    CaptureVectorField(TEXT("color_offset"), Settings.ColorOffset, Settings.bOverride_ColorOffset, TEXT("color_offset"));

    double ScalarValue = 0.0;
    if (SettingsObject->TryGetNumberField(FStringView(TEXT("temperature")), ScalarValue))
    {
        Settings.bOverride_ColorTemperature = true;
        Settings.WhiteTemp = ScalarValue;
        OutAppliedFields->SetNumberField(TEXT("temperature"), ScalarValue);
        bAppliedAny = true;
    }

    if (SettingsObject->TryGetNumberField(FStringView(TEXT("tint")), ScalarValue))
    {
        Settings.bOverride_ColorTint = true;
        Settings.WhiteTint = ScalarValue;
        OutAppliedFields->SetNumberField(TEXT("tint"), ScalarValue);
        bAppliedAny = true;
    }

    if (SettingsObject->TryGetNumberField(FStringView(TEXT("film_slope")), ScalarValue))
    {
        Settings.bOverride_FilmSlope = true;
        Settings.FilmSlope = ScalarValue;
        OutAppliedFields->SetNumberField(TEXT("film_slope"), ScalarValue);
        bAppliedAny = true;
    }

    if (SettingsObject->TryGetNumberField(FStringView(TEXT("film_toe")), ScalarValue))
    {
        Settings.bOverride_FilmToe = true;
        Settings.FilmToe = ScalarValue;
        OutAppliedFields->SetNumberField(TEXT("film_toe"), ScalarValue);
        bAppliedAny = true;
    }

    if (SettingsObject->TryGetNumberField(FStringView(TEXT("film_shoulder")), ScalarValue))
    {
        Settings.bOverride_FilmShoulder = true;
        Settings.FilmShoulder = ScalarValue;
        OutAppliedFields->SetNumberField(TEXT("film_shoulder"), ScalarValue);
        bAppliedAny = true;
    }

    if (SettingsObject->TryGetNumberField(FStringView(TEXT("film_black_clip")), ScalarValue))
    {
        Settings.bOverride_FilmBlackClip = true;
        Settings.FilmBlackClip = ScalarValue;
        OutAppliedFields->SetNumberField(TEXT("film_black_clip"), ScalarValue);
        bAppliedAny = true;
    }

    if (SettingsObject->TryGetNumberField(FStringView(TEXT("film_white_clip")), ScalarValue))
    {
        Settings.bOverride_FilmWhiteClip = true;
        Settings.FilmWhiteClip = ScalarValue;
        OutAppliedFields->SetNumberField(TEXT("film_white_clip"), ScalarValue);
        bAppliedAny = true;
    }

    FString LutPath;
    if (SettingsObject->TryGetStringField(FStringView(TEXT("look_up_texture")), LutPath) ||
        SettingsObject->TryGetStringField(FStringView(TEXT("lut")), LutPath))
    {
        if (!LutPath.IsEmpty())
        {
            if (UTexture* LutTexture = LoadTextureOptional(LutPath))
            {
                Settings.bOverride_ColorGradingLUT = true;
                Settings.ColorGradingLUT = LutTexture;
                OutAppliedFields->SetStringField(TEXT("lut"), LutTexture->GetPathName());
                bAppliedAny = true;
            }
            else
            {
                MCP_LOG_WARNING("Failed to load LUT texture at path %s", *LutPath);
            }
        }
    }

    if (SettingsObject->TryGetNumberField(FStringView(TEXT("lut_intensity")), ScalarValue) ||
        SettingsObject->TryGetNumberField(FStringView(TEXT("color_grading_intensity")), ScalarValue))
    {
        Settings.bOverride_ColorGradingIntensity = true;
        Settings.ColorGradingIntensity = ScalarValue;
        OutAppliedFields->SetNumberField(TEXT("color_grading_intensity"), ScalarValue);
        bAppliedAny = true;
    }

    if (SettingsObject->TryGetNumberField(FStringView(TEXT("blend_weight")), ScalarValue))
    {
        Volume->BlendWeight = ScalarValue;
        OutAppliedFields->SetNumberField(TEXT("blend_weight"), ScalarValue);
        bAppliedAny = true;
    }

    if (SettingsObject->TryGetNumberField(FStringView(TEXT("priority")), ScalarValue))
    {
        Volume->Priority = ScalarValue;
        OutAppliedFields->SetNumberField(TEXT("priority"), ScalarValue);
        bAppliedAny = true;
    }

    bool bUnboundValue = Volume->bUnbound;
    if (SettingsObject->TryGetBoolField(FStringView(TEXT("unbound")), bUnboundValue))
    {
        Volume->bUnbound = bUnboundValue;
        OutAppliedFields->SetBoolField(TEXT("unbound"), bUnboundValue);
        bAppliedAny = true;
    }

    return bAppliedAny;
}

bool FMCPApplyColorGradingHandler::TryExtractVector4(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, FVector4& OutVector) const
{
    if (!JsonObject.IsValid())
    {
        return false;
    }

    // Try array format first.
    const TArray<TSharedPtr<FJsonValue>>* ArrayValues = nullptr;
    if (JsonObject->TryGetArrayField(FieldName, ArrayValues) && ArrayValues)
    {
        if (ArrayValues->Num() == 0)
        {
            return false;
        }

        double Components[4] = {1.0, 1.0, 1.0, 1.0};
        const int32 Count = FMath::Min(ArrayValues->Num(), 4);
        for (int32 Index = 0; Index < Count; ++Index)
        {
            Components[Index] = (*ArrayValues)[Index]->AsNumber();
        }

        // If the array was only RGB, keep alpha at 1.0.
        OutVector = FVector4(Components[0], Components[1], Components[2], Components[3]);
        return true;
    }

    // Support scalar shorthand that applies uniformly across RGB with alpha 1.0.
    double ScalarValue = 0.0;
    if (JsonObject->TryGetNumberField(FieldName, ScalarValue))
    {
        OutVector = FVector4(ScalarValue, ScalarValue, ScalarValue, ScalarValue);
        return true;
    }

    return false;
}

