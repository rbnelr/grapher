
set CXX=cl

rem meson build_vsdbg  --backend=vs --buildtype=debug
meson build_vsval  --backend=vs --buildtype=debugoptimized -Db_lto=true

set CXX=g++

meson build_gccval  --buildtype=debugoptimized -Db_lto=true
meson build_gccprof --buildtype=debugoptimized -Db_lto=true -Dtracy=true
meson build_gccrel  --buildtype=release        -Db_lto=true
