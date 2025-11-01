// Copyright Epic Games, Inc.

#pragma once

#include "MCPCommandHandlers.h"

class AActor;

/**
 * Handler for configuring the Celestial Vault sky system.
 */
class FMCPSetupCelestialVaultHandler : public FMCPCommandHandlerBase
{
public:
    FMCPSetupCelestialVaultHandler();

    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket) override;

private:
    bool ResolveOrSpawnSkyActor(UWorld* World, const TSharedPtr<FJsonObject>& Params, AActor*& OutActor, FString& OutError) const;
    bool ApplyTransform(AActor* Actor, const TSharedPtr<FJsonObject>& Params, FString& OutError) const;
    bool ApplySettings(UObject* Target, const TSharedPtr<FJsonObject>& Settings, FString& OutError) const;
    bool ApplyPropertyValue(UObject* Target, FProperty* Property, const TSharedPtr<FJsonValue>& Value, FString& OutError) const;
    bool AssignValue(FProperty* Property, void* Address, const TSharedPtr<FJsonValue>& Value, FString& OutError) const;
    bool ApplyStructValue(void* DataPtr, FStructProperty* StructProperty, const TSharedPtr<FJsonValue>& Value, FString& OutError) const;
    bool ApplyArrayValue(void* DataPtr, FArrayProperty* ArrayProperty, const TSharedPtr<FJsonValue>& Value, FString& OutError) const;
};

