@echo off
REM Build script for creating DungeonCrawler.exe on Windows
REM Run this on a Windows machine with Python 3.8+ installed

echo Installing PyInstaller...
pip install pyinstaller

echo Building DungeonCrawler.exe...
pyinstaller --onefile --windowed --name "DungeonCrawler" ^
    --add-data "dungeon_crawler.py;." ^
    dungeon_gui.py

echo.
echo Build complete! Find DungeonCrawler.exe in the dist folder.
pause
