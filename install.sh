#!/bin/bash

SOURCE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

#mozjpeg
git clone https://github.com/mozilla/mozjpeg
cd mozjpeg/
mkdir build && cd build
cmake -G"Unix Makefiles" ..
make && sudo make install

cd ${SOURCE}

#zopflipng
git clone https://github.com/google/zopfli.git
cd zopfli
make libzopflipng
if [[ "$OSTYPE" == "darwin"* ]]; then
    sudo cp libzopflipng.so.1.0.2 /usr/local/lib
    sudo ln -s libzopflipng.so.1.0.2 /usr/local/lib/libzopflipng.so
    sudo ln -s libzopflipng.so.1.0.2 /usr/local/lib/libzopflipng.so.1
    sudo mkdir /usr/local/include/zopflipng
    sudo cp src/zopflipng/zopflipng_lib.h /usr/local/include/zopflipng
else
    sudo cp libzopflipng.so.1.0.2 /usr/lib
    sudo ln -s libzopflipng.so.1.0.2 /usr/lib/libzopflipng.so
    sudo ln -s libzopflipng.so.1.0.2 /usr/lib/libzopflipng.so.1
    sudo mkdir /usr/include/zopflipng
    sudo cp src/zopflipng/zopflipng_lib.h /usr/include/zopflipng
fi

cd ${SOURCE}
