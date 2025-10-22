#!/bin/bash
"/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe" /property:Configuration=Release
cp "x64/Release/BeastieballInfoWindow.dll" "$beastieball/mods/aurie/"
