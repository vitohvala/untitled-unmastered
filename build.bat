@echo off

mkdir build
pushd build

cl -FC -Zi ../src/untitled.c ../src/sound.cpp user32.lib gdi32.lib

popd
