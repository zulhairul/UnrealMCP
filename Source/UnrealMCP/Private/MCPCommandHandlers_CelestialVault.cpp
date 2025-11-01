#include "MCPCommandHandlers_CelestialVault.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "JsonObjectConverter.h"
#include "MCPFileLogger.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Field.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Containers/ScriptArray.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Internationalization/Text.h"

namespace
{
    constexpr TCHAR DefaultCelestialVaultBlueprintPath[] = TEXT("/CelestialVault/Blueprints/BP_CelestialSky.BP_CelestialSky_C");

    bool JsonValueToVector(const TSharedPtr<FJsonValue>& Value, FVector& OutVector)
    {
        if (!Value.IsValid())
        {
            return false;
        }

        if (Value->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& JsonArray = Value->AsArray();
            if (JsonArray.Num() != 3)
            {
                return false;
            }

            OutVector.X = static_cast<float>(JsonArray[0]->AsNumber());
            OutVector.Y = static_cast<float>(JsonArray[1]->AsNumber());
            OutVector.Z = static_cast<float>(JsonArray[2]->AsNumber());
            return true;
        }

        if (Value->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> Obj = Value->AsObject();
            if (!Obj.IsValid())
            {
                return false;
            }

            double X = 0.0, Y = 0.0, Z = 0.0;
            if (!Obj->TryGetNumberField(TEXT("x"), X))
            {
                Obj->TryGetNumberField(TEXT("X"), X);
            }
            if (!Obj->TryGetNumberField(TEXT("y"), Y))
            {
                Obj->TryGetNumberField(TEXT("Y"), Y);
            }
            if (!Obj->TryGetNumberField(TEXT("z"), Z))
            {
                Obj->TryGetNumberField(TEXT("Z"), Z);
            }

            OutVector = FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
            return true;
        }

        if (Value->Type == EJson::Number)
        {
            double W = Value->AsNumber();
            OutVector = FVector(static_cast<float>(W));
            return true;
        }

        FString VectorString;
        if (Value->TryGetString(VectorString))
        {
            FVector Parsed;
            if (Parsed.InitFromString(VectorString))
            {
                OutVector = Parsed;
                return true;
            }
        }

        return false;
    }

    bool JsonValueToRotator(const TSharedPtr<FJsonValue>& Value, FRotator& OutRotator)
    {
        if (!Value.IsValid())
        {
            return false;
        }

        if (Value->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& JsonArray = Value->AsArray();
            if (JsonArray.Num() != 3)
            {
                return false;
            }

            OutRotator.Pitch = static_cast<float>(JsonArray[0]->AsNumber());
            OutRotator.Yaw = static_cast<float>(JsonArray[1]->AsNumber());
            OutRotator.Roll = static_cast<float>(JsonArray[2]->AsNumber());
            return true;
        }

        if (Value->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> Obj = Value->AsObject();
            if (!Obj.IsValid())
            {
                return false;
            }

            double Pitch = 0.0, Yaw = 0.0, Roll = 0.0;
            Obj->TryGetNumberField(TEXT("pitch"), Pitch);
            Obj->TryGetNumberField(TEXT("yaw"), Yaw);
            Obj->TryGetNumberField(TEXT("roll"), Roll);
            Obj->TryGetNumberField(TEXT("Pitch"), Pitch);
            Obj->TryGetNumberField(TEXT("Yaw"), Yaw);
            Obj->TryGetNumberField(TEXT("Roll"), Roll);

            OutRotator = FRotator(static_cast<float>(Pitch), static_cast<float>(Yaw), static_cast<float>(Roll));
            return true;
        }

        FString RotatorString;
        if (Value->TryGetString(RotatorString))
        {
            FRotator Parsed;
            if (Parsed.InitFromString(RotatorString))
            {
                OutRotator = Parsed;
                return true;
            }
        }

        return false;
    }

    bool JsonValueToLinearColor(const TSharedPtr<FJsonValue>& Value, FLinearColor& OutColor)
    {
        if (!Value.IsValid())
        {
            return false;
        }

        if (Value->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& JsonArray = Value->AsArray();
            if (JsonArray.Num() < 3)
            {
                return false;
            }

            OutColor.R = static_cast<float>(JsonArray[0]->AsNumber());
            OutColor.G = static_cast<float>(JsonArray[1]->AsNumber());
            OutColor.B = static_cast<float>(JsonArray[2]->AsNumber());
            OutColor.A = JsonArray.Num() > 3 ? static_cast<float>(JsonArray[3]->AsNumber()) : 1.0f;
            return true;
        }

        if (Value->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> Obj = Value->AsObject();
            if (!Obj.IsValid())
            {
                return false;
            }

            double R = 0.0, G = 0.0, B = 0.0, A = 1.0;
            Obj->TryGetNumberField(TEXT("r"), R);
            Obj->TryGetNumberField(TEXT("g"), G);
            Obj->TryGetNumberField(TEXT("b"), B);
            Obj->TryGetNumberField(TEXT("a"), A);
            Obj->TryGetNumberField(TEXT("R"), R);
            Obj->TryGetNumberField(TEXT("G"), G);
            Obj->TryGetNumberField(TEXT("B"), B);
            Obj->TryGetNumberField(TEXT("A"), A);

            OutColor = FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(A));
            return true;
        }

        FString ColorString;
        if (Value->TryGetString(ColorString))
        {
            FLinearColor Parsed;
            if (Parsed.InitFromString(ColorString))
            {
                OutColor = Parsed;
                return true;
            }
        }

        return false;
    }

    bool JsonValueToVector2D(const TSharedPtr<FJsonValue>& Value, FVector2D& OutVector)
    {
        if (!Value.IsValid())
        {
            return false;
        }

        if (Value->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& JsonArray = Value->AsArray();
            if (JsonArray.Num() != 2)
            {
                return false;
            }

            OutVector.X = static_cast<float>(JsonArray[0]->AsNumber());
            OutVector.Y = static_cast<float>(JsonArray[1]->AsNumber());
            return true;
        }

        if (Value->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> Obj = Value->AsObject();
            if (!Obj.IsValid())
            {
                return false;
            }

            double X = 0.0, Y = 0.0;
            if (!Obj->TryGetNumberField(TEXT("x"), X))
            {
                Obj->TryGetNumberField(TEXT("X"), X);
            }
            if (!Obj->TryGetNumberField(TEXT("y"), Y))
            {
                Obj->TryGetNumberField(TEXT("Y"), Y);
            }

            OutVector = FVector2D(static_cast<float>(X), static_cast<float>(Y));
            return true;
        }

        return false;
    }

    bool JsonValueToDateTime(const TSharedPtr<FJsonValue>& Value, FDateTime& OutDateTime)
    {
        if (!Value.IsValid())
        {
            return false;
        }

        FString DateString;
        if (!Value->TryGetString(DateString))
        {
            return false;
        }

        DateString.TrimStartAndEndInline();
        if (FDateTime::ParseIso8601(*DateString, OutDateTime))
        {
            return true;
        }

        if (FDateTime::Parse(DateString, OutDateTime))
        {
            return true;
        }

        return false;
    }
}

FMCPSetupCelestialVaultHandler::FMCPSetupCelestialVaultHandler()
    : FMCPCommandHandlerBase(TEXT("setup_celestial_vault"))
{
}

TSharedPtr<FJsonObject> FMCPSetupCelestialVaultHandler::Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket)
{
    MCP_LOG_INFO(TEXT("Handling setup_celestial_vault command"));

    if (!GEditor)
    {
        MCP_LOG_ERROR(TEXT("GEditor is not available"));
        return CreateErrorResponse(TEXT("Editor context is not available"));
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        MCP_LOG_ERROR(TEXT("Unable to access the editor world"));
        return CreateErrorResponse(TEXT("Editor world is not available"));
    }

    // Make sure the Celestial Vault module is loaded if the plugin is available.
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("CelestialVault")))
    {
        if (!FModuleManager::Get().LoadModulePtr<IModuleInterface>(TEXT("CelestialVault")))
        {
            MCP_LOG_WARNING(TEXT("CelestialVault module could not be loaded; proceeding with dynamic class lookups"));
        }
    }

    AActor* SkyActor = nullptr;
    FString ErrorMessage;
    if (!ResolveOrSpawnSkyActor(World, Params, SkyActor, ErrorMessage))
    {
        MCP_LOG_ERROR(TEXT("Failed to resolve Celestial Vault actor: %s"), *ErrorMessage);
        return CreateErrorResponse(ErrorMessage);
    }

    if (!SkyActor)
    {
        MCP_LOG_ERROR(TEXT("ResolveOrSpawnSkyActor returned null actor without error"));
        return CreateErrorResponse(TEXT("Unable to find or create a Celestial Vault actor"));
    }

    if (!ApplyTransform(SkyActor, Params, ErrorMessage))
    {
        MCP_LOG_ERROR(TEXT("Failed to apply transform: %s"), *ErrorMessage);
        return CreateErrorResponse(ErrorMessage);
    }

    const TSharedPtr<FJsonObject>* SettingsObject = nullptr;
    if (Params->TryGetObjectField(TEXT("settings"), SettingsObject) && SettingsObject && SettingsObject->IsValid())
    {
        if (!ApplySettings(SkyActor, *SettingsObject, ErrorMessage))
        {
            MCP_LOG_ERROR(TEXT("Failed to apply settings: %s"), *ErrorMessage);
            return CreateErrorResponse(ErrorMessage);
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* ComponentsArray = nullptr;
    if (Params->TryGetArrayField(TEXT("components"), ComponentsArray) && ComponentsArray)
    {
        for (const TSharedPtr<FJsonValue>& ComponentValue : *ComponentsArray)
        {
            if (!ComponentValue.IsValid() || ComponentValue->Type != EJson::Object)
            {
                MCP_LOG_WARNING(TEXT("Skipping invalid component entry"));
                continue;
            }

            const TSharedPtr<FJsonObject> ComponentObject = ComponentValue->AsObject();
            if (!ComponentObject.IsValid())
            {
                continue;
            }

            FString PropertyName;
            if (!ComponentObject->TryGetStringField(TEXT("property"), PropertyName) || PropertyName.IsEmpty())
            {
                MCP_LOG_WARNING(TEXT("Component entry missing property field"));
                continue;
            }

            FProperty* Property = SkyActor->GetClass()->FindPropertyByName(*PropertyName);
            if (!Property)
            {
                MCP_LOG_WARNING(TEXT("Actor does not expose component property '%s'"), *PropertyName);
                continue;
            }

            FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property);
            if (!ObjectProperty)
            {
                MCP_LOG_WARNING(TEXT("Property '%s' is not an object property"), *PropertyName);
                continue;
            }

            UObject* ComponentObjectInstance = ObjectProperty->GetObjectPropertyValue(ObjectProperty->ContainerPtrToValuePtr<UObject*>(SkyActor));
            if (!ComponentObjectInstance)
            {
                MCP_LOG_WARNING(TEXT("Property '%s' on Celestial Vault actor is null"), *PropertyName);
                continue;
            }

            const TSharedPtr<FJsonObject>* ComponentSettings = nullptr;
            if (ComponentObject->TryGetObjectField(TEXT("settings"), ComponentSettings) && ComponentSettings && ComponentSettings->IsValid())
            {
                if (!ApplySettings(ComponentObjectInstance, *ComponentSettings, ErrorMessage))
                {
                    MCP_LOG_WARNING(TEXT("Failed to configure component '%s': %s"), *PropertyName, *ErrorMessage);
                }
            }
        }
    }

    // Build response payload
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actor_name"), SkyActor->GetName());
    Result->SetStringField(TEXT("actor_label"), SkyActor->GetActorLabel());
    Result->SetStringField(TEXT("actor_path"), SkyActor->GetPathName());

    FVector Location = SkyActor->GetActorLocation();
    FRotator Rotation = SkyActor->GetActorRotation();
    FVector Scale = SkyActor->GetActorScale3D();

    TSharedPtr<FJsonObject> TransformObject = MakeShared<FJsonObject>();
    TransformObject->SetArrayField(TEXT("location"), {
        MakeShared<FJsonValueNumber>(Location.X),
        MakeShared<FJsonValueNumber>(Location.Y),
        MakeShared<FJsonValueNumber>(Location.Z)
    });
    TransformObject->SetArrayField(TEXT("rotation"), {
        MakeShared<FJsonValueNumber>(Rotation.Pitch),
        MakeShared<FJsonValueNumber>(Rotation.Yaw),
        MakeShared<FJsonValueNumber>(Rotation.Roll)
    });
    TransformObject->SetArrayField(TEXT("scale"), {
        MakeShared<FJsonValueNumber>(Scale.X),
        MakeShared<FJsonValueNumber>(Scale.Y),
        MakeShared<FJsonValueNumber>(Scale.Z)
    });

    Result->SetObjectField(TEXT("transform"), TransformObject);

    MCP_LOG_INFO(TEXT("Celestial Vault configured successfully for actor '%s'"), *SkyActor->GetActorLabel());
    return CreateSuccessResponse(Result);
}

bool FMCPSetupCelestialVaultHandler::ResolveOrSpawnSkyActor(UWorld* World, const TSharedPtr<FJsonObject>& Params, AActor*& OutActor, FString& OutError) const
{
    OutActor = nullptr;

    if (!World)
    {
        OutError = TEXT("World is null");
        return false;
    }

    FString TargetActorLabel;
    Params->TryGetStringField(TEXT("actor_label"), TargetActorLabel);

    FString TargetActorName;
    Params->TryGetStringField(TEXT("actor_name"), TargetActorName);

    if (!TargetActorLabel.IsEmpty() || !TargetActorName.IsEmpty())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (!Actor)
            {
                continue;
            }

            if (!TargetActorLabel.IsEmpty() && Actor->GetActorLabel() == TargetActorLabel)
            {
                OutActor = Actor;
                break;
            }

            if (!TargetActorName.IsEmpty() && Actor->GetName() == TargetActorName)
            {
                OutActor = Actor;
                break;
            }
        }
    }

    if (OutActor)
    {
        MCP_LOG_INFO(TEXT("Found existing Celestial Vault actor '%s'"), *OutActor->GetActorLabel());
        return true;
    }

    FString BlueprintPath;
    Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath);
    if (BlueprintPath.IsEmpty())
    {
        BlueprintPath = DefaultCelestialVaultBlueprintPath;
    }

    UClass* ActorClass = LoadObject<UClass>(nullptr, *BlueprintPath);
    if (!ActorClass)
    {
        OutError = FString::Printf(TEXT("Failed to load Celestial Vault blueprint '%s'"), *BlueprintPath);
        return false;
    }

    const FScopedTransaction Transaction(NSLOCTEXT("UnrealMCP", "SetupCelestialVault", "Setup Celestial Vault"));
    World->Modify();

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Name = NAME_None;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParameters.OverrideLevel = World->GetCurrentLevel();

    const FVector SpawnLocation(0.0f, 0.0f, 0.0f);
    const FRotator SpawnRotation(0.0f, 0.0f, 0.0f);

    OutActor = World->SpawnActor<AActor>(ActorClass, SpawnLocation, SpawnRotation, SpawnParameters);
    if (!OutActor)
    {
        OutError = TEXT("Failed to spawn Celestial Vault actor");
        return false;
    }

    FString DesiredLabel = TargetActorLabel;
    if (DesiredLabel.IsEmpty())
    {
        DesiredLabel = TEXT("Celestial Vault");
    }

    OutActor->SetActorLabel(DesiredLabel);
    MCP_LOG_INFO(TEXT("Spawned new Celestial Vault actor '%s' using '%s'"), *DesiredLabel, *BlueprintPath);
    return true;
}

bool FMCPSetupCelestialVaultHandler::ApplyTransform(AActor* Actor, const TSharedPtr<FJsonObject>& Params, FString& OutError) const
{
    if (!Actor)
    {
        OutError = TEXT("Actor is null");
        return false;
    }

    bool bModified = false;

    const TArray<TSharedPtr<FJsonValue>>* LocationArray = nullptr;
    if (Params->TryGetArrayField(TEXT("location"), LocationArray) && LocationArray)
    {
        if (LocationArray->Num() == 3)
        {
            FVector Location;
            Location.X = static_cast<float>((*LocationArray)[0]->AsNumber());
            Location.Y = static_cast<float>((*LocationArray)[1]->AsNumber());
            Location.Z = static_cast<float>((*LocationArray)[2]->AsNumber());
            Actor->Modify();
            Actor->SetActorLocation(Location);
            bModified = true;
        }
        else
        {
            OutError = TEXT("Location must contain exactly three numeric values");
            return false;
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* RotationArray = nullptr;
    if (Params->TryGetArrayField(TEXT("rotation"), RotationArray) && RotationArray)
    {
        if (RotationArray->Num() == 3)
        {
            FRotator Rotation;
            Rotation.Pitch = static_cast<float>((*RotationArray)[0]->AsNumber());
            Rotation.Yaw = static_cast<float>((*RotationArray)[1]->AsNumber());
            Rotation.Roll = static_cast<float>((*RotationArray)[2]->AsNumber());
            Actor->Modify();
            Actor->SetActorRotation(Rotation);
            bModified = true;
        }
        else
        {
            OutError = TEXT("Rotation must contain exactly three numeric values");
            return false;
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* ScaleArray = nullptr;
    if (Params->TryGetArrayField(TEXT("scale"), ScaleArray) && ScaleArray)
    {
        if (ScaleArray->Num() == 3)
        {
            FVector Scale;
            Scale.X = static_cast<float>((*ScaleArray)[0]->AsNumber());
            Scale.Y = static_cast<float>((*ScaleArray)[1]->AsNumber());
            Scale.Z = static_cast<float>((*ScaleArray)[2]->AsNumber());
            Actor->Modify();
            Actor->SetActorScale3D(Scale);
            bModified = true;
        }
        else
        {
            OutError = TEXT("Scale must contain exactly three numeric values");
            return false;
        }
    }

    if (bModified)
    {
        Actor->InvalidateLightingCache();
        Actor->PostEditChange();
        Actor->MarkPackageDirty();
    }

    return true;
}

bool FMCPSetupCelestialVaultHandler::ApplySettings(UObject* Target, const TSharedPtr<FJsonObject>& Settings, FString& OutError) const
{
    if (!Target)
    {
        OutError = TEXT("Target object is null");
        return false;
    }

    if (!Settings.IsValid())
    {
        return true;
    }

    Target->Modify();

    for (const auto& Pair : Settings->Values)
    {
        const FString& PropertyName = Pair.Key;
        FProperty* Property = Target->GetClass()->FindPropertyByName(*PropertyName);
        if (!Property)
        {
            MCP_LOG_WARNING(TEXT("Property '%s' not found on %s"), *PropertyName, *Target->GetClass()->GetName());
            continue;
        }

        if (!ApplyPropertyValue(Target, Property, Pair.Value, OutError))
        {
            MCP_LOG_WARNING(TEXT("Failed to assign property '%s': %s"), *PropertyName, *OutError);
            return false;
        }
    }

    if (AActor* Actor = Cast<AActor>(Target))
    {
        Actor->PostEditChange();
        Actor->MarkPackageDirty();
    }
    else
    {
        Target->PostEditChange();
        if (UPackage* Package = Target->GetOutermost())
        {
            Package->SetDirtyFlag(true);
        }
    }

    return true;
}

bool FMCPSetupCelestialVaultHandler::ApplyPropertyValue(UObject* Target, FProperty* Property, const TSharedPtr<FJsonValue>& Value, FString& OutError) const
{
    if (!Target || !Property)
    {
        OutError = TEXT("Invalid target or property");
        return false;
    }

    void* Address = Property->ContainerPtrToValuePtr<void>(Target);
    return AssignValue(Property, Address, Value, OutError);
}

bool FMCPSetupCelestialVaultHandler::AssignValue(FProperty* Property, void* Address, const TSharedPtr<FJsonValue>& Value, FString& OutError) const
{
    if (!Property || !Address)
    {
        OutError = TEXT("Invalid property address");
        return false;
    }

    if (!Value.IsValid())
    {
        OutError = TEXT("Value is invalid");
        return false;
    }

    if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
    {
        if (!Value->IsNumeric())
        {
            OutError = FString::Printf(TEXT("Expected numeric value for property '%s'"), *Property->GetName());
            return false;
        }

        if (NumericProperty->IsInteger())
        {
            const int64 IntValue = static_cast<int64>(Value->AsNumber());
            NumericProperty->SetIntPropertyValue(Address, IntValue);
        }
        else
        {
            const double FloatValue = Value->AsNumber();
            NumericProperty->SetFloatingPointPropertyValue(Address, FloatValue);
        }
        return true;
    }

    if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
    {
        if (Value->Type == EJson::Boolean)
        {
            BoolProperty->SetPropertyValue(Address, Value->AsBool());
            return true;
        }

        if (Value->Type == EJson::String)
        {
            FString BoolString = Value->AsString();
            BoolProperty->SetPropertyValue(Address, BoolString.ToBool());
            return true;
        }

        OutError = FString::Printf(TEXT("Expected boolean value for property '%s'"), *Property->GetName());
        return false;
    }

    if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
    {
        FString StringValue;
        if (!Value->TryGetString(StringValue))
        {
            OutError = FString::Printf(TEXT("Expected string value for property '%s'"), *Property->GetName());
            return false;
        }
        StringProperty->SetPropertyValue(Address, StringValue);
        return true;
    }

    if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
    {
        FString NameString;
        if (!Value->TryGetString(NameString))
        {
            OutError = FString::Printf(TEXT("Expected string for name property '%s'"), *Property->GetName());
            return false;
        }
        NameProperty->SetPropertyValue(Address, FName(*NameString));
        return true;
    }

    if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
    {
        FString TextString;
        if (!Value->TryGetString(TextString))
        {
            OutError = FString::Printf(TEXT("Expected string for text property '%s'"), *Property->GetName());
            return false;
        }
        TextProperty->SetPropertyValue(Address, FText::FromString(TextString));
        return true;
    }

    if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
    {
        return ApplyStructValue(Address, StructProperty, Value, OutError);
    }

    if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
    {
        return ApplyArrayValue(Address, ArrayProperty, Value, OutError);
    }

    if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
    {
        if (Value->Type == EJson::String)
        {
            FString ObjectPath = Value->AsString();
            UObject* LoadedObject = LoadObject<UObject>(nullptr, *ObjectPath);
            if (!LoadedObject && !ObjectPath.IsEmpty())
            {
                OutError = FString::Printf(TEXT("Failed to load object '%s' for property '%s'"), *ObjectPath, *Property->GetName());
                return false;
            }
            ObjectProperty->SetObjectPropertyValue(Address, LoadedObject);
            return true;
        }

        if (Value->Type == EJson::Object)
        {
            UObject* ObjectValue = ObjectProperty->GetObjectPropertyValue(Address);
            if (!ObjectValue)
            {
                OutError = FString::Printf(TEXT("Property '%s' is null; cannot apply nested settings"), *Property->GetName());
                return false;
            }

            const TSharedPtr<FJsonObject> NestedObject = Value->AsObject();
            if (!ApplySettings(ObjectValue, NestedObject, OutError))
            {
                return false;
            }
            return true;
        }

        OutError = FString::Printf(TEXT("Unsupported JSON type for object property '%s'"), *Property->GetName());
        return false;
    }

    if (FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
    {
        FString ObjectPath;
        if (!Value->TryGetString(ObjectPath))
        {
            OutError = FString::Printf(TEXT("Expected string asset path for property '%s'"), *Property->GetName());
            return false;
        }

        FSoftObjectPath SoftPath(ObjectPath);
        SoftObjectProperty->SetPropertyValue(Address, SoftPath);
        return true;
    }

    if (FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(Property))
    {
        FString ClassPath;
        if (!Value->TryGetString(ClassPath))
        {
            OutError = FString::Printf(TEXT("Expected string class path for property '%s'"), *Property->GetName());
            return false;
        }

        FSoftObjectPath SoftPath(ClassPath);
        SoftClassProperty->SetPropertyValue(Address, SoftPath);
        return true;
    }

    if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
    {
        const UEnum* Enum = EnumProperty->GetEnum();
        if (!Enum)
        {
            OutError = FString::Printf(TEXT("Enum property '%s' does not have an enum"), *Property->GetName());
            return false;
        }

        if (Value->Type == EJson::String)
        {
            const FString EnumName = Value->AsString();
            int64 EnumValue = Enum->GetValueByNameString(EnumName);
            if (EnumValue == INDEX_NONE)
            {
                OutError = FString::Printf(TEXT("Enum value '%s' not found for property '%s'"), *EnumName, *Property->GetName());
                return false;
            }

            EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(Address, EnumValue);
            return true;
        }

        if (Value->IsNumeric())
        {
            const int64 EnumValue = static_cast<int64>(Value->AsNumber());
            EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(Address, EnumValue);
            return true;
        }

        OutError = FString::Printf(TEXT("Unsupported JSON type for enum property '%s'"), *Property->GetName());
        return false;
    }

    OutError = FString::Printf(TEXT("Property '%s' is of unsupported type"), *Property->GetName());
    return false;
}

bool FMCPSetupCelestialVaultHandler::ApplyStructValue(void* DataPtr, FStructProperty* StructProperty, const TSharedPtr<FJsonValue>& Value, FString& OutError) const
{
    if (!StructProperty || !DataPtr)
    {
        OutError = TEXT("Invalid struct assignment");
        return false;
    }

    UScriptStruct* Struct = StructProperty->Struct;
    if (!Struct)
    {
        OutError = TEXT("Struct property missing struct definition");
        return false;
    }

    if (Struct == TBaseStructure<FVector>::Get())
    {
        FVector VectorValue;
        if (!JsonValueToVector(Value, VectorValue))
        {
            OutError = TEXT("Failed to parse FVector from JSON");
            return false;
        }
        *static_cast<FVector*>(DataPtr) = VectorValue;
        return true;
    }

    if (Struct == TBaseStructure<FRotator>::Get())
    {
        FRotator RotatorValue;
        if (!JsonValueToRotator(Value, RotatorValue))
        {
            OutError = TEXT("Failed to parse FRotator from JSON");
            return false;
        }
        *static_cast<FRotator*>(DataPtr) = RotatorValue;
        return true;
    }

    if (Struct == TBaseStructure<FLinearColor>::Get())
    {
        FLinearColor ColorValue;
        if (!JsonValueToLinearColor(Value, ColorValue))
        {
            OutError = TEXT("Failed to parse FLinearColor from JSON");
            return false;
        }
        *static_cast<FLinearColor*>(DataPtr) = ColorValue;
        return true;
    }

    if (Struct == TBaseStructure<FVector2D>::Get())
    {
        FVector2D VectorValue;
        if (!JsonValueToVector2D(Value, VectorValue))
        {
            OutError = TEXT("Failed to parse FVector2D from JSON");
            return false;
        }
        *static_cast<FVector2D*>(DataPtr) = VectorValue;
        return true;
    }

    if (Struct == TBaseStructure<FDateTime>::Get())
    {
        FDateTime DateValue;
        if (!JsonValueToDateTime(Value, DateValue))
        {
            OutError = TEXT("Failed to parse FDateTime from JSON");
            return false;
        }
        *static_cast<FDateTime*>(DataPtr) = DateValue;
        return true;
    }

    const TSharedPtr<FJsonObject> StructObject = Value->AsObject();
    if (!StructObject.IsValid())
    {
        OutError = FString::Printf(TEXT("Struct property '%s' expects an object"), *StructProperty->GetName());
        return false;
    }

    for (TFieldIterator<FProperty> It(Struct); It; ++It)
    {
        FProperty* InnerProperty = *It;
        if (!InnerProperty)
        {
            continue;
        }

        const TSharedPtr<FJsonValue>* FieldValuePtr = StructObject->Values.Find(InnerProperty->GetName());
        if (!FieldValuePtr)
        {
            continue;
        }

        void* FieldAddress = InnerProperty->ContainerPtrToValuePtr<void>(DataPtr);
        if (!AssignValue(InnerProperty, FieldAddress, *FieldValuePtr, OutError))
        {
            return false;
        }
    }

    return true;
}

bool FMCPSetupCelestialVaultHandler::ApplyArrayValue(void* DataPtr, FArrayProperty* ArrayProperty, const TSharedPtr<FJsonValue>& Value, FString& OutError) const
{
    if (!ArrayProperty || !DataPtr)
    {
        OutError = TEXT("Invalid array assignment");
        return false;
    }

    if (Value->Type != EJson::Array)
    {
        OutError = FString::Printf(TEXT("Property '%s' expects an array"), *ArrayProperty->GetName());
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>& JsonArray = Value->AsArray();

    FScriptArrayHelper ArrayHelper(ArrayProperty, DataPtr);
    ArrayHelper.Resize(0);

    for (const TSharedPtr<FJsonValue>& ElementValue : JsonArray)
    {
        int32 NewIndex = ArrayHelper.AddValue();
        void* ElementPtr = ArrayHelper.GetRawPtr(NewIndex);
        if (!AssignValue(ArrayProperty->Inner, ElementPtr, ElementValue, OutError))
        {
            return false;
        }
    }

    return true;
}

