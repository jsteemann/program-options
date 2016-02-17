# program-options

A C++11-based, header-only library for parsing command-line arguments and simple ini files.

The library was developed for use in ArangoDB so it uses the namespace `arangodb::options`.
Apart from the namespace, the library is general purpose and does not have any dependencies to 
ArangoDB or other components apart from standard C++11 headers.

A small example for setting up options is included in `example.cpp`. 
To compile and run it, use:

```bash
g++ -Wall -Wextra -std=c++11 example.cpp -o example 
./example --quiet -c config.ini foo bar baz
```

Compilation should work with g++5, clang++3.6, MSVS 2013 and MSVS 2015. Earlier compilers
are questionable if they don't support all of the C++11 features used.
