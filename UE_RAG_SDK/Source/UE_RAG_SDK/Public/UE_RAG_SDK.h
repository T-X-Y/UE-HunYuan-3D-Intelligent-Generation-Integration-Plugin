// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class UE_RAG_SDK_API FUE_RAG_SDKModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static inline FUE_RAG_SDKModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FUE_RAG_SDKModule>("UE_RAG_SDK");
    }

    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("UE_RAG_SDK");
    }
};