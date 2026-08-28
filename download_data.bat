@echo off
echo ========================================
echo   飞花令 & 成语接龙 - 数据下载脚本 (Windows)
echo ========================================
echo.

if not exist "third_party" mkdir third_party

echo [1/2] 下载 chinese-poetry (诗词库)...
if exist "third_party\chinese-poetry\.git" (
    echo   已存在，跳过
) else (
    git clone --depth 1 https://github.com/chinese-poetry/chinese-poetry.git third_party\chinese-poetry
)

echo.
echo [2/2] 下载 chinese-xinhua (成语库)...
if exist "third_party\chinese-xinhua\.git" (
    echo   已存在，跳过
) else (
    git clone --depth 1 https://github.com/pwxcoo/chinese-xinhua.git third_party\chinese-xinhua
)

echo.
echo ========================================
echo   数据下载完成！
echo   现在可以用 Visual Studio 打开 FeihuaLing.sln 构建
echo ========================================
pause
