# 飞花令 & 成语接龙 (终端版)

基于 [chinese-poetry](https://github.com/chinese-poetry/chinese-poetry) 和 [chinese-xinhua](https://github.com/pwxcoo/chinese-xinhua) 数据库的简体中文终端游戏。

## 功能

- **飞花令**: 玩家选择一个固定关键字，所有诗句必须包含此字
- **古诗接龙**: 下一句必须包含上一句最后一个字（目标字滚动传递）
- **成语接龙**: 下一个成语必须以上一个成语的最后一个字开头
- **用过粒度可选** (诗词模式): 半句不可用 / 整句不可用 / 整首不可用
- **输入校验**: 玩家输入的诗词/成语必须在库中，防止乱输入
- **多玩家**: 支持多名人类玩家同局
- **多机器人**: 可配置多个 AI 对手
- **机器人记忆配置**: 记住全部 / 随机 N 条 / 百分比
- **存档系统**: 随时保存/读取，HMAC-SHA256 防篡改
- **纯终端**: 无 GUI，Windows / Linux 均可运行

## 快速开始

### 克隆仓库（含 submodule）

```bash
git clone --recursive https://github.com/yourname/feihualing.git
cd feihualing
```

如果已经克隆但没有 submodule：

```bash
git submodule update --init --recursive
```

### Windows (Visual Studio 2022)

直接打开 `FeihuaLing.sln`，选择 Release|x64，生成即可。

或使用命令行：

```bash
msbuild FeihuaLing.sln /p:Configuration=Release /p:Platform=x64
```

运行时确保工作目录包含 `third_party/` 目录（即项目根目录）。

### Linux / CMake

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cd build && ./FeihuaLing
```

## 游戏操作

- 主菜单选择新游戏或读取存档
- 飞花令模式选择「用过」粒度
- 添加人类玩家和机器人（可配置机器人记忆量）
- 回合中输入诗句/成语
- 命令:
  - `/save` 或 `/s` — 存档
  - `/hint` 或 `/h` — 显示可用选项提示
  - `/quit` 或 `/q` — 退出当前对局

## 项目结构

```
feihualing/
├── FeihuaLing.sln              # Visual Studio 解决方案
├── FeihuaLing.vcxproj          # MSVC 工程文件
├── CMakeLists.txt              # CMake 跨平台构建
├── .gitmodules                 # Git submodule 配置
├── .github/workflows/build.yml # GitHub Actions CI
├── include/
│   ├── game.hpp                # 游戏核心逻辑
│   ├── player.hpp              # 玩家与机器人
│   ├── poetry_db.hpp           # 诗词数据库
│   ├── idiom_db.hpp            # 成语数据库
│   ├── save_system.hpp         # 存档与防篡改
│   ├── utils.hpp               # UTF-8 / 字符串工具
│   ├── sha256.hpp              # SHA256 / HMAC
│   ├── t2s.hpp                 # 繁简转换 (OpenCC)
│   └── nlohmann/json.hpp       # JSON 解析
├── src/
│   ├── main.cpp                # 终端入口与菜单
│   ├── game.cpp
│   ├── player.cpp
│   ├── poetry_db.cpp
│   ├── idiom_db.cpp
│   └── save_system.cpp
├── third_party/
│   ├── chinese-poetry/         # 诗词数据 (submodule)
│   └── chinese-xinhua/         # 成语数据 (submodule)
└── saves/                      # 存档目录
```

## 数据来源

- 诗词: [chinese-poetry/chinese-poetry](https://github.com/chinese-poetry/chinese-poetry) — 全唐诗、宋诗、宋词
- 成语: [pwxcoo/chinese-xinhua](https://github.com/pwxcoo/chinese-xinhua) — 30000+ 成语
- 繁简转换: [OpenCC](https://github.com/BYVoid/OpenCC) — Apache-2.0

## 许可证

MIT
