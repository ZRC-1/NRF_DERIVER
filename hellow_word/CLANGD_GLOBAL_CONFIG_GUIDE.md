# Clangd 全局配置指南

## ? 已完成的配置

### 1. 全局 .clangd 文件
已创建：`C:\Users\Admin\.clangd`

此文件会自动应用到所有项目（除非项目有自己的 .clangd）。

## ? 需要手动完成的配置

### 2. VSCode 用户设置

打开 VSCode 用户设置：
1. 按 `Ctrl+,` 打开设置
2. 点击右上角的 "打开设置(JSON)" 图标
3. 在 JSON 文件中添加以下配置（在最后一个属性后添加逗号，然后添加）：

```json
"clangd.arguments": [
    "--background-index",
    "--clang-tidy",
    "--header-insertion=never",
    "--compile-commands-dir=${workspaceFolder}/build"
]
```

### 完整示例

你的 settings.json 应该类似这样：

```json
{
    "KeilAssistant.MDK.Uv4Path": "D:\\Keil_v5\\UV4\\UV4.exe",
    "editor.fontSize": 16,
    
    // 添加这一行（注意前面要有逗号）
    "clangd.arguments": [
        "--background-index",
        "--clang-tidy",
        "--header-insertion=never",
        "--compile-commands-dir=${workspaceFolder}/build"
    ]
}
```

## ? 配置说明

### 全局 .clangd 配置的作用：
- 自动移除 ARM GCC 特定的编译器标志
- 添加 Zephyr/nRF 常用宏定义
- 配置诊断行为
- 适用于所有 nRF Connect SDK 项目

### VSCode clangd.arguments 的作用：
- `--background-index`: 后台索引，提升性能
- `--clang-tidy`: 启用代码检查
- `--header-insertion=never`: 禁止自动插入头文件
- `--compile-commands-dir`: 指定编译数据库位置

## ? 应用配置

完成手动配置后：
1. 重启 VSCode
2. 或按 `Ctrl+Shift+P` → "clangd: Restart language server"

## ? 项目级配置（可选）

如果某个项目需要特殊配置，可以在项目根目录创建 `.clangd` 文件，它会覆盖全局配置。

当前项目已有项目级配置：`.clangd`

## ? 验证配置

打开任何 nRF Connect SDK 项目的 C 文件：
- 不应该有红色波浪线
- 头文件应该能正确跳转
- 代码补全应该正常工作

## ? 故障排查

如果配置不生效：
1. 检查 VSCode 是否安装了 clangd 扩展
2. 确保项目已构建（存在 build/*/compile_commands.json）
3. 查看 VSCode 输出面板 → clangd 日志
4. 重启 clangd 语言服务器

## ? 备份信息

你的原始 settings.json 已备份到：
`C:\Users\Admin\AppData\Roaming\Code\User\settings.json.backup`
