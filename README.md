# JIT a HSACO

## install llvm-14 libs/headers
follow https://apt.llvm.org/. The API may change during different LLVM release, so this example is only written based on llvm-14.

`sudo ./llvm.sh 14 all`

`sh build.sh`

`./hsaco_jit.exe # will result in _tmp.o object`


## run this hsaco
unfortunately, we still need to link this `_tmp.o` into ELF in order to use current HIP API(rocm-4.x~5.x, Jun/2022) to launch the code object

`/usr//lib/llvm-14/bin/ld.lld   _tmp.o  -shared -o memcpy_x4_kernel_gfx1030.hsaco`

then we need download https://github.com/carlushuang/gcnasm.git, the `memcpy_example_gfx1030` example, replace the above generated hsaco with that built from this example. Then you can run it.