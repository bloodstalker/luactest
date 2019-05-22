
# luactest
The idea is to be able to write and run tests for C functions from Lua. inspired by me being lazy and my being too short for writing tests in C.<br/>
The workflow is the clang libtooling tool under `libtool` will find the funcitons and add their lua wrapper into source code and register it with lua. `luatablegen` is a python script that takes an XML script describing your C structs and turns them into lua tables since almost every non-toy C code uses structs anyways and adding C structs as Lua tables is non-trivial. after that you just build the tool(its not a typo, you actually build your tool) which is a Lua 5.3 interpreter. you then write your test scripts in Lua and run them. `linenoise` is there to give you some basic cli functionality if you choose to run the tool in interactive mode.<br/>

```sh
git clone https://github.com/bloodstalker/luactest
git submodule init
git submodule update
make
```
