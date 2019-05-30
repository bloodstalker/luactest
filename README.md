
[![Build Status](https://travis-ci.org/bloodstalker/luactest.svg?branch=master)](https://travis-ci.org/bloodstalker/luactest)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/bloodstalker/luactest.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/bloodstalker/luactest/alerts/)
<a href="https://scan.coverity.com/projects/bloodstalker-luactest">
  <img alt="Coverity Scan Build Status"
       src="https://img.shields.io/coverity/scan/18518.svg"/>
</a>


# luactest
The idea is to be able to write and run tests for C functions from Lua. inspired by me being lazy and my being too short for writing tests in C.<br/>
The workflow is the clang libtooling tool under `libtool` will find the funcitons and add their lua wrapper into source code and register it with lua. `luatablegen` is a python script that takes an XML script describing your C structs and turns them into lua tables since almost every non-toy C code uses structs anyways and adding C structs as Lua tables is non-trivial. after that you just build the tool(its not a typo, you actually build your tool) which is a Lua 5.3 interpreter. you then write your test scripts in Lua and run them. `linenoise` is there to give you some basic cli functionality if you choose to run the tool in interactive mode. the `default.lua` file will add your `luarocks` libraries to our custom Lua instance for some added quality of life.<br/>

## Demo
you can run the makefile in the root directory. then run the executable, type in `dofile("./test.lua")`and see the result. it runs our little test function from Lua which converts a string into an integer. <br\>

## TODO
* write the libtooling tool.<br/>
* need to add in `linenoise`.<br/>
* need to improve the lua REPL. right now it sucks.<br/>
* add cygwin support because i have to use windows at work...<br/>


## Notes
The `luarocks` feature work for Lua 5.3. I haven't looked at the Lua 5.4 changelog yet.<br/>
```sh
git clone https://github.com/bloodstalker/luactest
git submodule init
git submodule update
make
```
