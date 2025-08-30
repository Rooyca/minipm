
build:
    [ -f minipm ] && [ main.c -ot minipm ] || cc main.c -lX11 -o minipm

install: build
    mv ./minipm /usr/bin

uninstall:
    rm /usr/bin/minipm
