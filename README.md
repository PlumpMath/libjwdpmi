# libjwdpmi
Because I can't come up with any better names.  
This library aims to be a complete development framework for DPMI (32-bit DOS) applications, written in C++14.  
It's still in the experimental stage. Anything may change at any time.

## Features
Current features include:
* C++ interfaces to access many DPMI services.
* Interrupt handling, including dynamic IRQ assignment, IRQ sharing, and nested interrupts.
* CPU exception handling, also nested and re-entrant.
* Cooperative multi-threading and coroutines.

## Installing
* Build and install DJGPP (the DOS port of gcc)  
A build script can be found here: https://github.com/andrewwutw/build-djgpp

* Set your `PATH` and `GCC_EXEC_PREFIX` accordingly:  
```
    $ export PATH=/usr/local/djgpp/i586-pc-msdosdjgpp/bin:$PATH  
    $ export GCC_EXEC_PREFIX=/usr/local/djgpp/lib/gcc/  
```
* Add this repository as a submodule in your own project  
```
    $ git submodule add https://github.com/jwt27/libjwdpmi.git ./lib/libjwdpmi  
    $ git submodule update --init
```
* In the root directory, copy the sample configuration file to `jwdpmi_config.h` and adjust options as necessary.  
```
    $ cp jwdpmi_config_default.h jwdpmi_config.h  
```
* In your makefile, export your `CXX` and `CXXFLAGS`, and add a rule to build `libjwdpmi`:  
```
    export CXX CXXFLAGS  
    libjwdpmi:  
        $(MAKE) -C lib/libjwdpmi/  
```
* Add the `include` directory to your global include path (`-I`) and link your program with `libjwdpmi.a`, found in the `bin/` directory.  

## Using
There's basically no documentation at this point. Read the header files in `include/`, there are some comments scattered around. The `detail` namespaces contain implementation details, you shouldn't need to use anything in it (file a feature request if you do).

## License
It's GPLv3, for now.
