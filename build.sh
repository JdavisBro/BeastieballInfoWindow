#!/bin/bash
"/c/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" /property:Configuration=Release
cp "x64/Release/BeastieballGacha.dll" "$beastieball/mods/aurie/"