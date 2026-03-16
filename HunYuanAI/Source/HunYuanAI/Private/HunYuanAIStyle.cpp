#include "HunYuanAIStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FHunYuanAIStyle::StyleInstance = nullptr;

void FHunYuanAIStyle::Initialize()
{
    if (!StyleInstance.IsValid())
    {
        StyleInstance = Create();
        FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
    }
}

void FHunYuanAIStyle::Shutdown()
{
    FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
    ensure(StyleInstance.IsUnique());
    StyleInstance.Reset();
}

FName FHunYuanAIStyle::GetStyleSetName()
{
    static FName StyleSetName(TEXT("HunYuanAIStyle"));
    return StyleSetName;
}

TSharedRef<FSlateStyleSet> FHunYuanAIStyle::Create()
{
    TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet("HunYuanAIStyle"));
    Style->SetContentRoot(IPluginManager::Get().FindPlugin("HunYuanAI")->GetBaseDir() / TEXT("Resources"));

    const FVector2D Icon20x20(20.0f, 20.0f);
    Style->Set("HunYuanAI.OpenPluginWindow", new IMAGE_BRUSH_SVG(TEXT("PlaceholderButtonIcon"), Icon20x20));

    return Style;
}

void FHunYuanAIStyle::ReloadTextures()
{
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
    }
}

const ISlateStyle& FHunYuanAIStyle::Get()
{
    return *StyleInstance;
}