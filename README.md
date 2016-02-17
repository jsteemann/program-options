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
are questionable if they don't support all of the C++11 features used by the library.

To display a list of command-line options implemented by the example, run

```bash 
./example --help
```

The example should be pretty self-explanatory. Its *main* function defines a few typed
option parameters (booleans, integers, strings) that can be set via the command-line
(argc/argv) or via a configuration file. 

All options are validated. Using an unknown option or an out-of-bounds value for any
of the options will make the options processing fail and report an appropriate error.
 
Custom parameter types and vector options (specifying multiple values for an option) 
are possible, and examples for this are also included. The example also contains code
for handling common cases like `--help` and `--version`.
