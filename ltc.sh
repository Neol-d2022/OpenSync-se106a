wget https://github.com/libtom/libtomcrypt/releases/download/v1.18.0/crypt-1.18.0.tar.xz && tar -xvf crypt-1.18.0.tar.xz && cd libtomcrypt-1.18.0/ && make CFLAGS="-DTFM_DESC" && make CFLAGS="-DTFM_DESC" install && cd ../ && rm -rf libtomcrypt-1.18.0/ crypt-1.18.0.tar.xz