#!/bin/bash
set -e

rm -rf tools/gdb
rm -rf tools/djgpp
rm -rf tools/dosbox-x

base_url="https://marioslab.io/dump/dos"
gdb_url=""
djgpp_url=""
dosbox_url=""
os=$OSTYPE

if [[ "$os" == "linux-gnu"* ]]; then
    gdb_url="https://github.com/badlogic/gdb-7.1a-djgpp/releases/download/gdb-7.1a-djgpp/gdb-7.1a-djgpp-linux.zip";
    djgpp_url="https://github.com/andrewwutw/build-djgpp/releases/download/v3.3/djgpp-linux64-gcc1210.tar.bz2";
    dosbox_url="https://github.com/badlogic/dosbox-x/releases/download/dosbox-x-gdb-v0.84.5/dosbox-x-0.84.5-linux.zip";
elif [[ "$os" == "darwin"* ]]; then
    gdb_url="https://github.com/badlogic/gdb-7.1a-djgpp/releases/download/gdb-7.1a-djgpp/gdb-7.1a-djgpp-macos-x86_64.zip";
    djgpp_url="https://github.com/andrewwutw/build-djgpp/releases/download/v3.3/djgpp-osx-gcc1210.tar.bz2";
    dosbox_url="https://github.com/badlogic/dosbox-x/releases/download/dosbox-x-gdb-v0.84.5/dosbox-x-macosx-x86_64-20221223232510.zip";
elif [[ "$os" == "cygwin" ]] || [[ "$os" == "msys" ]] || [[ $(uname -r) =~ WSL ]]; then
    gdb_url="https://github.com/badlogic/gdb-7.1a-djgpp/releases/download/gdb-7.1a-djgpp/gdb-7.1a-djgpp-windows.zip";
    djgpp_url="https://github.com/andrewwutw/build-djgpp/releases/download/v3.3/djgpp-mingw-gcc1210-standalone.zip";
    dosbox_url="https://github.com/badlogic/dosbox-x/releases/download/dosbox-x-gdb-v0.84.5/dosbox-x-mingw-win64-20221223232734.zip";
else
    echo "Sorry, this template doesn't support $os"
    exit
fi

pushd tools &> /dev/null

echo "Installing GDB"
echo "$gdb_url"
mkdir -p gdb
pushd gdb &> /dev/null
curl -L $gdb_url --output gdb.zip &> /dev/null
unzip -o gdb.zip > /dev/null
rm gdb.zip > /dev/null
popd &> /dev/null

echo "Installing DJGPP"
echo "$djgpp_url"
if [[ "$os" == "cygwin" ]] || [[ "$os" == "msys" ]] || [[ $(uname -r) =~ WSL ]]; then
    curl -L $djgpp_url --output djgpp.zip &> /dev/null
    unzip djgpp.zip &> /dev/null
    rm djgpp.zip
else
    curl -L $djgpp_url --output djgpp.tar.bz2 &> /dev/null
    tar xf djgpp.tar.bz2
    rm djgpp.tar.bz2
fi

echo "Installing DOSBox-x"
echo "$dosbox_url"
curl -L $dosbox_url --output dosbox.zip &> /dev/null
unzip -o dosbox.zip &> /dev/null
rm dosbox.zip

echo "Installing VS Code extensions"
if [[ $(code --version) ]]; then
    code --install-extension llvm-vs-code-extensions.vscode-clangd --install-extension ms-vscode.cmake-tools --install-extension ms-vscode.cpptools --install-extension webfreak.debug
else
    echo "WARNING: could not find 'code' on path. Could not install VS Code extensions!"
    echo "         Ensure 'code' is on your PATH and re-run 'download-tools.sh' to install"
    echo "         the VS Code extensions."
fi

if [[ "$os" == "linux-gnu"* ]]; then
    chmod a+x gdb/gdb > /dev/null
    chmod a+x djgpp/bin/*
    chmod a+x djgpp/i586-pc-msdosdjgpp/bin/*
    chmod a+x dosbox-x/dosbox-x-sdl1
    ln -s $(pwd)/dosbox-x/dosbox-x-sdl1 dosbox-x/dosbox-x
    echo
    echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    echo
    echo " Please install the following packages using your Linux distribution's "
    echo " package manager:"
    echo
    echo " libncurses5 libfl-dev libslirp-dev libfluidsynth-dev"
    echo
    echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
elif [[ "$os" == "darwin"* ]]; then
    chmod a+x gdb/gdb > /dev/null
    chmod a+x djgpp/bin/*
    chmod a+x djgpp/i586-pc-msdosdjgpp/bin/*
    chmod a+x dosbox-x/dosbox-x.app/Contents/MacOS/dosbox-x
    ln -s $(pwd)/dosbox-x/dosbox-x.app/Contents/MacOS/dosbox-x dosbox-x/dosbox-x
elif [[ "$os" == "cygwin" ]] || [[ "$os" == "msys" ]] || [[ $(uname -r) =~ WSL ]]; then
    rm -rf "COPYING"
    mv mingw-build/mingw dosbox-x
    rm -rf mingw-build
fi

popd &> /dev/null