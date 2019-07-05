#!/usr/bin/python3
# _*_ coding=utf-8 _*_

import argparse
import code
import readline
import signal
import sys
import subprocess
shell_result = subprocess.run(["llvm-config", "--src-root"], capture_output=True)
sys.path.insert(0, shell_result.stdout.decode("utf-8")[:-1] + "/bindings/python")
sys.path.insert(0, shell_result.stdout.decode("utf-8")[:-1] + "/tools/clang/bindings/python")
import llvm
import clang.cindex

def SigHandler_SIGINT(signum, frame):
    print()
    sys.exit(0)

class Argparser(object):
    def __init__(self):
        parser = argparse.ArgumentParser()
        parser.add_argument("--file", type=str, help="string")
        parser.add_argument("--symbol", type=str, help="string")
        parser.add_argument("--bool", action="store_true", help="bool", default=False)
        parser.add_argument("--dbg", action="store_true", help="debug", default=False)
        self.args = parser.parse_args()

def find_typerefs(node, typename):
    if node.kind.is_reference():
        #print(node.spelling)
        if node.spelling == typename:
            print("Found %s [Line=%s, Col=%s]"%(typename, node.location.line, node.location.column))
    for c in node.get_children():
        find_typerefs(c, typename)

# write code here
def premain(argparser):
    signal.signal(signal.SIGINT, SigHandler_SIGINT)
    #here
    index = clang.cindex.Index.create()
    tu = index.parse(argparser.args.file)
    print("TU:", tu.spelling)
    find_typerefs(tu.cursor, argparser.args.symbol)

def main():
    argparser = Argparser()
    if argparser.args.dbg:
        try:
            premain(argparser)
        except Exception as e:
            if hasattr(e, "__doc__"):
                print(e.__doc__)
            if hasattr(e, "message"):
                print(e.message)
            variables = globals().copy()
            variables.update(locals())
            shell = code.InteractiveConsole(variables)
            shell.interact(banner="DEBUG REPL")
    else:
        premain(argparser)

if __name__ == "__main__":
    main()
