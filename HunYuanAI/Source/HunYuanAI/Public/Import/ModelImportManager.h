#pragma once

#include "CoreMinimal.h"
#include "UI/ModelInfoWidget.h"
#include "AssetImportTask.h"
#include "UObject/NoExportTypes.h"

// 模型文件组信息
struct FModelFileGroup
{
    FString FolderPath;
    FString ObjFilePath;
    FString MtlFilePath;
    FString TextureFilePath;
    FString ModelName;

    bool IsValid() const
    {
        return !ObjFilePath.IsEmpty() && FPaths::FileExists(ObjFilePath);
    }

    bool HasTexture() const
    {
        return !TextureFilePath.IsEmpty() && FPaths::FileExists(TextureFilePath);
    }

    bool HasMaterial() const
    {
        return !MtlFilePath.IsEmpty() && FPaths::FileExists(MtlFilePath);
    }
};

// 导入结果枚举
enum class EModelImportResult : uint8
{
    Success,
    FileNotFound,
    UnsupportedFormat,
    DirectoryCreationFailed,
    ImportFailed,
    EditorNotAvailable,
    ObjFileNotFound,
    MaterialBindFailed,
    TextureCopyFailed
};

// 导入进度委托
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnModelImportProgress, int32 /* Current */, int32 /* Total */, const FString& /* CurrentModelName */);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnModelImported, bool /* bSuccess */, const FString& /* AssetPath */);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnImportCompleted, int32 /* SuccessCount */);

class HUNYUANAI_API FModelImportManager : public TSharedFromThis<FModelImportManager>
{
public:
    static TSharedPtr<FModelImportManager> Get();
    static void Shutdown();

    ~FModelImportManager();

    // 从解压后的文件夹导入模型（包含obj、mtl、png）
    bool ImportModelFromFolder(const FString& FolderPath, const FString& DestinationPath = TEXT("/Game/HunyuanImports/"));

    // 批量导入多个文件夹
    int32 ImportModelsFromFolders(const TArray<FString>& FolderPaths, const FString& DestinationPath = TEXT("/Game/HunyuanImports/"));

    // 异步导入多个文件夹
    void ImportModelsAsync(const TArray<FString>& FolderPaths, const FString& DestinationPath = TEXT("/Game/HunyuanImports/"));

    // 扫描文件夹中的模型文件
    static FModelFileGroup ScanFolderForModelFiles(const FString& FolderPath);

    // 检查文件是否为支持的3D格式
    static bool IsSupportedFormat(const FString& FilePath);

    // 获取支持的格式列表
    static TArray<FString> GetSupportedFormats();

    // 设置默认导入路径
    void SetDefaultImportPath(const FString& NewPath);

    // 获取默认导入路径
    FString GetDefaultImportPath() const;

    // 取消当前导入
    void CancelImport();

    // 是否正在导入
    bool IsImporting() const { return bIsImporting; }

    // 获取材质绑定状态
    bool IsMaterialBound(const FString& ModelPath) const;

    // 导入进度委托
    FOnModelImportProgress OnModelImportProgress;
    FOnModelImported OnModelImported;
    FOnImportCompleted OnImportCompleted;

private:
    FModelImportManager();

    // 准备导入文件（复制文件到项目结构）
    bool PrepareImportFiles(const FModelFileGroup& FileGroup, const FString& DestinationPath,
        FString& OutObjDestPath, FString& OutMtlDestPath, FString& OutTextureDestPath);

    // 修改MTL文件中的纹理路径
    bool UpdateMtlTexturePath(const FString& MtlFilePath, const FString& TextureFileName, const FString& OutputMtlPath);

    // 内部导入实现
    EModelImportResult ImportModelInternal(const FString& FilePath, const FString& DestinationPath, TArray<UObject*>& OutImportedAssets);

    // 在内容浏览器中选中资源
    void SelectInContentBrowser(const TArray<UObject*>& Assets);

    // 清理包名
    FString SanitizePackageName(const FString& InPackageName);

    // 确保目录存在
    bool EnsureDirectoryExists(const FString& PackagePath);

    // 异步导入的工作线程函数
    void AsyncImportWorker(TArray<FString> FolderPaths, FString DestinationPath);

private:
    static TSharedPtr<FModelImportManager> Instance;
    static FCriticalSection InstanceCriticalSection;

    FString DefaultImportPath;
    mutable FCriticalSection ImportCriticalSection;

    // 异步导入控制
    bool bIsImporting;
    bool bCancelRequested;
    FCriticalSection AsyncCriticalSection;
    TSharedPtr<FRunnableThread> ImportThread;

    // 记录已导入的模型
    TSet<FString> ImportedModels;
};