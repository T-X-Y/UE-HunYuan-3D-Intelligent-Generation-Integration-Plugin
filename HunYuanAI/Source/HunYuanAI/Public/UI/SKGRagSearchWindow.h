// SKGRagSearchWindow.h
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor/EditorEngine.h"

class SKnowledgeGraphSearchWidget;

class SKGRagSearchWindow : public SWindow
{
public:
    // 打开窗口
    static void OpenWindow();

    // 关闭窗口
    static void CloseWindow();

    // 获取窗口实例
    static TSharedPtr<SWindow> GetWindow();

    void Construct(const FArguments& InArgs);

private:
    static TWeakPtr<SWindow> Instance;
    TSharedPtr<SKnowledgeGraphSearchWidget> SearchWidget;
};