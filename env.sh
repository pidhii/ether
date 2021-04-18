p=`realpath ${1:?installation path not specified}`

export PATH=$p/bin:$PATH
export ETHER_ROOT=$p
export ETHER_PATH=$p/lib/ether
export PKG_CONFIG_PATH=$p/lib/pkgconfig${PKG_CONFIG_PATH+:${PKG_CONFIG_PATH}}
