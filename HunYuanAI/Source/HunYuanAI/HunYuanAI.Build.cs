// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class HunYuanAI : ModuleRules
{
    public HunYuanAI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // 定义第三方库路径
        string ThirdPartyPath = Path.Combine(ModuleDirectory, "ThirdParty");
        string TencentCloudPath = Path.Combine(ThirdPartyPath, "tencentcloud-sdk-cpp");
        string VcpkgDepsPath = Path.Combine(ThirdPartyPath, "vcpkg-deps");

        // 添加所有可能的包含路径
        PublicIncludePaths.AddRange(
            new string[] {
                Path.Combine(TencentCloudPath, "include"),
                Path.Combine(VcpkgDepsPath, "include"),
            }
        );

        // 私有包含路径
        PrivateIncludePaths.AddRange(
            new string[] {
                Path.Combine(TencentCloudPath, "include"),

            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
         );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                
                // ===== 原有模块依赖 =====
                "InputCore",
                "UnrealEd",           // 编辑器功能
                "AssetTools",         // 资产工具
                "AssetRegistry",      // 资产注册
                "ContentBrowser",     // 内容浏览器
                "MainFrame",          // 主框架
                "PropertyEditor",     // 属性编辑器
                "Projects",           // 项目设置
                "EditorStyle",
                "LevelEditor",
                "MeshDescription",    // 网格描述
                "StaticMeshDescription", // 静态网格描述
                "MeshUtilities",      // 网格工具
                "RawMesh",            // 原始网格
                
                // ===== HTTP 相关 =====
                "HTTP",               // HTTP请求
                "Json",               // JSON序列化
                "JsonUtilities",      // JSON工具
                
                // ===== 图片处理 =====
                "ImageWrapper",       // 图片处理
                
                // ===== UI 相关 =====
                "DesktopPlatform",    // 桌面平台对话框
                "ToolMenus",          // 工具栏菜单
                "InputCore",
                
                // ===== 新增：历史记录管理需要的模块 =====
                "Json",               // JSON序列化（已有，确保存在）
                "JsonUtilities",      // JSON工具（已有）
                "Projects",           // 路径管理（已有）
                
                // 如果需要加密历史记录，可以添加：
                // "Crypto",           // 加密支持（可选）
                
                // 如果需要数据库支持（高级功能）：
                // "DatabaseSupport",  // 数据库支持（可选）
            }
        );

        // 添加库文件
        PublicAdditionalLibraries.AddRange(
            new string[]
            {
                Path.Combine(TencentCloudPath, "lib", "tencentcloud-sdk-cpp-core.lib"),
                //Path.Combine(TencentCloudPath, "lib", "tencentcloud-sdk-cpp-ai3d.lib"),
                Path.Combine(VcpkgDepsPath, "lib", "libcurl.lib"),
                Path.Combine(VcpkgDepsPath, "lib", "libcrypto.lib"),
                Path.Combine(VcpkgDepsPath, "lib", "libssl.lib"),
                Path.Combine(VcpkgDepsPath, "lib", "zlib.lib"),
            }
        );

        // 添加预处理器宏
        PublicDefinitions.Add("TENCENTCLOUD_USE_COMMON_CLIENT=1");
        PublicDefinitions.Add("LOG_HUNYUANAI=1");


        // 运行时依赖配置
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // 检查实际DLL存放位置
            string[] PossibleDllPaths = new[]
            {
                Path.Combine(TencentCloudPath, "bin"),           // bin目录
                Path.Combine(TencentCloudPath, "bin", "Win64"),  // bin/Win64目录
                Path.Combine(TencentCloudPath, "lib"),           // lib目录
                Path.Combine(TencentCloudPath, "lib", "Win64"),  // lib/Win64目录
                Path.Combine(VcpkgDepsPath, "bin"),              // vcpkg bin目录
            };

            // 腾讯云DLL
            string[] TencentDlls = { "tencentcloud-sdk-cpp-core.dll" };
            foreach (string Dll in TencentDlls)
            {
                foreach (string DllPath in PossibleDllPaths)
                {
                    string SourcePath = Path.Combine(DllPath, Dll);
                    if (File.Exists(SourcePath))
                    {
                        // 复制到插件的Binaries目录
                        RuntimeDependencies.Add(Path.Combine("$(PluginDir)/Binaries/Win64", Dll), SourcePath);
                        break;
                    }
                }
            }

            // vcpkg依赖DLL
            string[] VcpkgDlls = { "libcurl.dll", "libcrypto-3-x64.dll", "libssl-3-x64.dll", "zlib1.dll" };
            foreach (string Dll in VcpkgDlls)
            {
                foreach (string DllPath in PossibleDllPaths)
                {
                    string SourcePath = Path.Combine(DllPath, Dll);
                    if (File.Exists(SourcePath))
                    {
                        RuntimeDependencies.Add(Path.Combine("$(PluginDir)/Binaries/Win64", Dll), SourcePath);
                        break;
                    }
                }
            }
        }
    }
}