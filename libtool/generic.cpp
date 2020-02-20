
/*first line intentionally left blank.*/
/***************************************************Project Mutator****************************************************/
//-*-c++-*-
// a simple clang tool.
/*Copyright (C) 2019 Farzad Sadeghi

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public 
License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later 
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the 
Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.*/
/*code structure inspired by Eli Bendersky's tutorial on Rewriters.*/
/**********************************************************************************************************************/
/*included modules*/
/*project headers*/
/*standard headers*/
#include <cassert>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
/*LLVM headers*/
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/LLVM.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
/*custom headers*/
#include "./cfe-extra/cfe_extra.h"
/**********************************************************************************************************************/
/*used namespaces*/
using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;
/**********************************************************************************************************************/
#if __clang_major__ <= 6
#define DEVI_GETLOCSTART getLocStart
#define DEVI_GETLOCEND getLocEnd
#elif __clang_major__ >= 8
#define DEVI_GETLOCSTART getBeginLoc
#define DEVI_GETLOCEND getEndLoc
#endif
/**********************************************************************************************************************/
namespace {
  static llvm::cl::OptionCategory lctcat("Obfuscator custom options");
  cl::opt<bool> CheckSystemHeader("SysHeader", cl::desc("match results in sys header also"), cl::init(false), cl::cat(lctcat), cl::ZeroOrMore);
  cl::opt<bool> MainFileOnly("MainOnly", cl::desc("only matches in the main file"), cl::init(true), cl::cat(lctcat), cl::ZeroOrMore);
  cl::opt<bool> LuaWrap("luawrap", cl::desc("enable lua wrapper functionality"), cl::init(false), cl::cat(lctcat), cl::ZeroOrMore);
  cl::opt<bool> FuncDump("funcdump", cl::desc("enable function dump functionality"), cl::init(false), cl::cat(lctcat), cl::ZeroOrMore);
  cl::opt<bool> CallCount("callcounter", cl::desc("enable the call counting funcitonality"), cl::init(false), cl::cat(lctcat), cl::ZeroOrMore);
  cl::opt<std::string> AutoGenOut("autogen", cl::desc("the complete path to the output file that will hold the Lua wrappers for your functions"), cl::init("./dummy.c"), cl::cat(lctcat), cl::ZeroOrMore);
  cl::list<std::string> FuncDumpList("funcdumplist", cl::desc("dumps functions that match the name"), cl::cat(lctcat), cl::ZeroOrMore, cl::CommaSeparated);
  cl::opt<std::string> FuncDumpPath("funcdumppath", cl::desc("path to dump functions to, for --funcdump"), cl::init("./dump.c"), cl::cat(lctcat), cl::ZeroOrMore);
  cl::opt<std::string> CallCountName("callcountname", cl::desc("the name of the function to count calls for"), cl::init("./dummy.c"), cl::cat(lctcat), cl::ZeroOrMore);
}
/**********************************************************************************************************************/
SourceLocation SourceLocationHasMacro(SourceLocation sl, Rewriter &rewrite) {
  if (sl.isMacroID()) {
    return rewrite.getSourceMgr().getSpellingLoc(sl);
  } else {
    return sl;
  }
}

SourceLocation getSLSpellingLoc(SourceLocation sl, Rewriter &rewrite) {
  if (sl.isMacroID()) return rewrite.getSourceMgr().getSpellingLoc(sl);
  else return sl;
}

std::string GetLuaRetPushExpr(const clang::Type* TP, const ASTContext &ASTC) {
  /* @trace 1.1.1.1 */
  if (TP->isIntegerType()) {
    return "lua_pushinteger";
  } else if (TP->isFloatingType()) {
    return "lua_pushnumber";
  } else if (TP->isRecordType()) {
  } else if (TP->isPointerType()) {
    return "lua_pushlightuserdata";
  } else if (TP->isVoidType()) {
  } else if (TP->isBooleanType()) {
  } else if (TP->isCharType()) {
    return "lua_pushstring";
  } else if (TP->isEnumeralType()) {
  } else {
  }
  return "";
}

std::string GetLuaParamPushExpr(const clang::Type* TP, const ASTContext &ASTC) {
  /* @trace 2.2.2.2 - 3.3.3.3 */
  /* @trace 4.4.4.4 */
  if (TP->isIntegerType()) {
    return "lua_tointeger";
  } else if (TP->isAnyCharacterType()) {
    return "lua_tostring";
  } else if (TP->isFloatingType()) {
    return "lua_tonumber";
  } else if (TP->isRecordType()) {}
  if (TP->isPointerType()) {
    QualType QT = TP->getPointeeType();
    QT = QT.getCanonicalType();
    if (QT.getTypePtrOrNull()) {
      if (QT.getTypePtrOrNull()->isAnyCharacterType()) {
        return "lua_tostring";
      }
    }
    return "lua_touserdata";
  } else if (TP->isVoidType()) {
  } else if (TP->isBooleanType()) {
    return "lua_toboolean";
  } else if (TP->isEnumeralType()) {
  } else {
  }
  return "";
}
/**********************************************************************************************************************/
/**********************************************************************************************************************/
/* 3.4.2.1 */
class FuncDecl : public MatchFinder::MatchCallback {
public:
  FuncDecl (Rewriter &Rewrite) : Rewrite (Rewrite) {
    if ("" != FuncDumpPath) {
      funcdump_out.open(FuncDumpPath, std::ios::out);
    } else {
      funcdump_out.open("./dump.c", std::ios::out);
    }
  }

  ~FuncDecl() {
    funcdump_out.close();
  }

  virtual void run(const MatchFinder::MatchResult &MR) {
    const FunctionDecl* FD = MR.Nodes.getNodeAs<clang::FunctionDecl>("funcdecllua");
    SourceLocation SL = FD->getSourceRange().getBegin();
    if (Devi::IsTheMatchInSysHeader(CheckSystemHeader, MR, SL)) return void();
    if (!Devi::IsTheMatchInMainFile(MainFileOnly, MR, SL)) return void();
    std::string FuncName = FD->getNameAsString();
    
    if (LuaWrap) {
      const ArrayRef<ParmVarDecl*> PVD = FD->parameters();
      QualType RT = FD->getReturnType();
      RT = RT.getCanonicalType();
      const clang::Type* TPR = RT.getTypePtrOrNull();
      const FunctionDecl* FDef = FD->getDefinition();
      const ASTContext* ASTC = MR.Context;
      unsigned NumParam = FD->getNumParams();
      std::ofstream fs;
      fs.open(AutoGenOut, std::ios_base::app);
      fs << "int " << FuncName << "_lct(lua_State* ls);"  << "\n";
      fs << "int " << FuncName << "_lct(lua_State* ls) {"  << "\n";
      fs << "if (" << NumParam << " != lua_gettop(ls)) {\nprintf (\"wrong number of arguments\\n\");\nreturn 0;\n}\n";
      if (FDef) {
        int back_counter = -NumParam;
        // pass 1
        for (auto &iter : PVD) {
          QualType QT = iter->getType();
          QT = QT.getCanonicalType();
          const clang::Type* TPP = QT.getTypePtrOrNull();
          const NamedDecl* ND = iter->getUnderlyingDecl();
          fs << QT.getAsString() << " ";
          fs << iter->getNameAsString() << " = ";
          fs << GetLuaParamPushExpr(TPP, *ASTC) << "(ls," << back_counter << ");\n";
          back_counter++;
        }
        // pass 2
        fs << RT.getAsString() << " result_lct" << " = " << FuncName << "(";
        for (auto &iter : PVD) {
          fs << iter->getNameAsString();
          if (NumParam > 1) {
            if (iter != PVD.back()) {
              fs << ",";
            }
          }
        }
      }
      fs << ");\n";
      if (TPR) {
        fs << GetLuaRetPushExpr(TPR, *ASTC) << "(ls, result_lct);\n";
        fs << "return 1;\n";
        fs << "}\n";
      } else {
        fs << "return 0;\n";
      }
      fs << "\n";
      fs.close();
    }

    if (FuncDump) {
      for(int i = 0; i < FuncDumpList.size(); ++i)
      {
        if (FuncName == FuncDumpList[i]) {
          SourceLocation SLE = FD->getSourceRange().getEnd();
          funcdump_out << Rewrite.getRewrittenText(SourceRange(SL, SLE)) << "\n";
        }
      }
    }
  }

private:
  Rewriter &Rewrite;
  std::ofstream funcdump_out;
};
/**********************************************************************************************************************/
class CallCounter : public MatchFinder::MatchCallback {
public:
  CallCounter (Rewriter &Rewrite) : Rewrite (Rewrite) {
    Call_Count = 0;
  }

  ~CallCounter() {
    if (CallCount) {
      std::cout << "fincal call count is " << Call_Count << ".\n";
    }
  }

  virtual void run(const MatchFinder::MatchResult &MR) {
    const CallExpr* CE = MR.Nodes.getNodeAs<clang::CallExpr>("callcounter");
    SourceLocation SL = CE->getSourceRange().getBegin();
    if (Devi::IsTheMatchInSysHeader(CheckSystemHeader, MR, SL)) return void();
    if (!Devi::IsTheMatchInMainFile(MainFileOnly, MR, SL)) return void();

    auto Callee = CE->getDirectCallee();
    if (!Callee) return void();

    std::string FuncName = Callee->getNameAsString();
    SourceLocation SLE = CE->getSourceRange().getBegin();
    auto SM = MR.SourceManager;

    if (FuncName == CallCountName) {
      Call_Count++;
      std::cout << SL.printToString(*SM) << ":" << SLE.printToString(*SM) << ":" << Call_Count << "\n";
    }
  }

private:
  Rewriter &Rewrite;
  uint32_t Call_Count;
};
/**********************************************************************************************************************/
class PPInclusion : public PPCallbacks {
public:
  explicit PPInclusion (SourceManager *SM, Rewriter *Rewrite) : SM(*SM), Rewrite(*Rewrite) {}

  virtual void MacroDefined(const Token &MacroNameTok, const MacroDirective *MD) {}

  virtual void MacroExpands (const Token &MacroNameTok, const MacroDefinition &MD, SourceRange Range, const MacroArgs *Args) {}

  virtual void  InclusionDirective (SourceLocation HashLoc, const Token &IncludeTok, 
      StringRef FileName, bool IsAngled, CharSourceRange FilenameRange, const FileEntry *File, 
      StringRef SearchPath, StringRef RelativePath, const clang::Module *Imported) {}

private:
  const SourceManager &SM;
  Rewriter &Rewrite;
};
/**********************************************************************************************************************/
class BlankDiagConsumer : public clang::DiagnosticConsumer {
  public:
    BlankDiagConsumer() = default;
    virtual ~BlankDiagConsumer() {}
    virtual void HandleDiagnostic(DiagnosticsEngine::Level DiagLevel, const Diagnostic &Info) override {}
};
/**********************************************************************************************************************/
class LCTASTConsumer : public ASTConsumer {
public:
  LCTASTConsumer(Rewriter &R) : funcDeclHandler(R), callCounterHandler(R)  {
    if (LuaWrap || FuncDump) {
      Matcher.addMatcher(functionDecl().bind("funcdecllua"), &funcDeclHandler);
    }

    if (CallCount) {
      Matcher.addMatcher(callExpr().bind("callcounter"), &callCounterHandler);
    }
  }

  void HandleTranslationUnit(ASTContext &Context) override {
    Matcher.matchAST(Context);
  }

private:
  FuncDecl funcDeclHandler;
  CallCounter callCounterHandler;
  MatchFinder Matcher;
};
/**********************************************************************************************************************/
class LCTFrontendAction : public ASTFrontendAction {
public:
  LCTFrontendAction() {}
  ~LCTFrontendAction() {
    delete BDCProto;
  }

  bool BeginInvocation(CompilerInstance &CI) override {
    return true;
  }

  void EndSourceFileAction() override {
  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
#if __clang_major__ > 9
    CI.getPreprocessor().addPPCallbacks(std::make_unique<PPInclusion>(&CI.getSourceManager(), &TheRewriter));
#elif
    CI.getPreprocessor().addPPCallbacks(llvm::make_unique<PPInclusion>(&CI.getSourceManager(), &TheRewriter));
#endif
    DiagnosticsEngine &DE = CI.getPreprocessor().getDiagnostics();
    DE.setClient(BDCProto, false);
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
#if __clang_major__ > 9
    return std::make_unique<LCTASTConsumer>(TheRewriter);
#elif
    return llvm::make_unique<LCTASTConsumer>(TheRewriter);
#endif
  }

private:
  BlankDiagConsumer* BDCProto = new BlankDiagConsumer;
  Rewriter TheRewriter;
};
/**********************************************************************************************************************/
/**********************************************************************************************************************/
/*Main*/
int main(int argc, const char **argv) {
  std::ofstream fs;
  fs.open(AutoGenOut);
  fs.close();
  CommonOptionsParser op(argc, argv, lctcat);
  const std::vector<std::string> &SourcePathList = op.getSourcePathList();
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());
  return Tool.run(newFrontendActionFactory<LCTFrontendAction>().get());
}
/**********************************************************************************************************************/
/*last line intentionally left blank.*/

