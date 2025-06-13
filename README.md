# GF

一个功能丰富的 DeepSeek API 命令行聊天客户端，支持配置文件管理和历史记录功能。  
~~todo:实现恋爱模式~~
## 编译

```bash
xmake build
```

## 使用方法

### 基本使用

```bash
# 设置API密钥环境变量
export DEEPSEEK_API_KEY="your_api_key_here"

# 运行程序
./build/linux/x86_64/release/gf
```

### 命令行参数

```bash
# 显示帮助信息
./gf --help

# 显示版本信息
./gf --version

# 指定配置文件
./gf --config /path/to/config.json

# 控制流式输出
./gf --stream on   # 启用流式输出
./gf --stream off  # 禁用流式输出

# 历史记录管理
./gf --history show              # 显示最近10条历史记录
./gf --history show --history-count 20  # 显示最近20条历史记录
./gf --history clear             # 清除所有历史记录
./gf --history search            # 搜索历史记录
./gf --history sessions          # 显示所有会话

# 多轮对话会话管理
./gf --session new               # 开始新会话
./gf --session session_20250613_123456_789  # 继续特定会话
./gf --load-context session_id  # 加载会话上下文
./gf --max-context 5             # 限制加载的上下文轮次

# 禁用历史记录（本次会话）
./gf --no-history
```

## 配置文件

配置文件默认位置：`~/.config/gf/config.json`

### 配置选项

```json
{
  "default_system_prompt": "You are a helpful AI assistant.",
  "stream_enabled": true,
  "max_history_entries": 1000,
  "default_model": "deepseek-chat",
  "auto_save_history": true,
  "temperature": 0.7
}
```

### 配置说明

- `default_system_prompt`: 默认系统提示词
- `stream_enabled`: 是否默认启用流式输出
- `max_history_entries`: 历史记录最大保存条数
- `default_model`: 默认使用的模型名称
- `auto_save_history`: 是否自动保存历史记录
- `temperature`: 模型温度参数

## 历史记录

历史记录默认保存在：`~/.config/gf/history.json`

### 历史记录功能

1. **自动保存**: 每次对话都会自动保存到历史记录文件
2. **查看历史**: 使用 `--history show` 查看最近的对话记录
3. **搜索功能**: 使用 `--history search` 搜索包含特定关键词的对话
4. **清除历史**: 使用 `--history clear` 清除所有历史记录
5. **限制条数**: 自动维护历史记录条数，超过限制时删除最旧的记录

### 历史记录格式

每条历史记录包含：
- 时间戳
- 用户消息  
- 助手回复
- 系统提示（如果有）
- 使用的模型名称

## 多轮对话功能

### 会话管理

每次启动程序都会自动创建新的会话，所有对话都按会话组织：

```bash
# 查看所有会话
./gf --history sessions

# 开始新会话
./gf --session new

# 继续特定会话
./gf --session session_20250613_123456_789
```

### 上下文加载

可以从历史会话中加载对话上下文，让AI了解之前的对话内容：

```bash
# 加载会话上下文（默认最近10轮对话）
./gf --load-context session_20250613_123456_789

# 限制加载的对话轮次
./gf --load-context session_20250613_123456_789 --max-context 5
```

### 聊天中的特殊命令

在聊天过程中，您可以使用以下特殊命令：

- `/help` - 显示帮助信息
- `/new` - 开始新会话
- `/session` - 显示当前会话信息
- `/sessions` - 列出所有会话
- `/load <session_id>` - 加载指定会话的上下文
- `/clear` - 清除当前对话上下文
- `/exit` - 退出程序


### 会话ID格式

会话ID格式：`session_YYYYMMDD_HHMMSS_mmm`
- 包含创建时间，便于识别和排序
- 例如：`session_20250613_143022_456`

## 使用示例

### 1. 首次运行

```bash
export DEEPSEEK_API_KEY="your_api_key_here"
./gf
```

程序会：
- 创建默认配置文件 `~/.config/gf/config.json`
- 创建历史记录文件 `~/.config/gf/history.json`
- 提示输入系统提示词（可使用默认值）

### 2. 查看历史记录

```bash
# 查看最近10条记录
./gf --history show

# 查看最近20条记录，显示详细信息
./gf --history show --history-count 20
```

### 3. 搜索历史记录

```bash
./gf --history search
# 然后输入搜索关键词，例如："Python"
```

### 4. 使用自定义配置

```bash
# 复制示例配置文件
cp config.example.json my_config.json

# 编辑配置文件
vim my_config.json

# 使用自定义配置运行
./gf --config my_config.json
```

## 文件结构

```
~/.config/gf/
├── config.json    # 配置文件
└── history.json   # 历史记录文件
```

## 依赖库
- jsoncpp: JSON 处理
- libcurl: HTTP 请求
- readline: 命令行输入

## 注意事项

1. 确保设置了 `DEEPSEEK_API_KEY` 环境变量
2. 首次运行时会自动创建配置目录和默认配置文件
3. 历史记录会占用磁盘空间，建议定期清理或调整 `max_history_entries`
4. 命令行参数的优先级高于配置文件设置
5. 多轮对话会话会自动创建，每个会话包含完整的对话历史
6. 使用上下文加载功能时，建议限制加载的轮次以避免token超限
7. 程序支持Ctrl+C优雅退出，会自动保存数据，无需担心数据丢失