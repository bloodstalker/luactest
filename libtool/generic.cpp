
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
  std::string TEMP_FILE="/tmp/generic_temp";
  cl::opt<std::string> AutoGenOut("autogen", cl::desc("the complete path to the output file that will hold the Lua wrappers for your functions"), cl::init("./dummy.c"), cl::cat(lctcat), cl::ZeroOrMore);
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
  if (TP->isIntegerType()) {
    return "lua_pushinteger";
  } // integer
  if (TP->isFloatingType()) {
    return "lua_pushnumber";
  } // float/number
  if (TP->isRecordType()) {} // struct or union
  if (TP->isPointerType()) {} // pointer
  if (TP->isVoidType()) {} // do nothing
  if (TP->isBooleanType()) {} // boolean
  if (TP->isCharType()) {
    return "lua_pushstring";
  } // string
  if (TP->isEnumeralType()) {} // enumeration
  return "";
}

std::string GetLuaParamPushExpr(const clang::Type* TP, const ASTContext &ASTC) {
  if (TP->isIntegerType()) {
    return "lua_tointeger";
  } // integer
  if (TP->isAnyCharacterType()) {
    return "lua_tostring";
  } // string
  if (TP->isFloatingType()) {
    return "lua_tonumber";
  } // float/number
  if (TP->isRecordType()) {} // struct or union
  if (TP->isPointerType()) {
    QualType QT = TP->getPointeeType();
    QT = QT.getCanonicalType();
    if (QT.getTypePtrOrNull()) {
      if (QT.getTypePtrOrNull()->isAnyCharacterType()) {
        return "lua_tostring";
      }
    }
    return "lua_touserdata";
  } // pointer
  if (TP->isVoidType()) {} // do nothing
  if (TP->isBooleanType()) {
    return "lua_toboolean";
  } // boolean
  if (TP->isEnumeralType()) {} // enumeration
  return "";
}
/**********************************************************************************************************************/
/**********************************************************************************************************************/
class FuncDecl : public MatchFinder::MatchCallback
{
public:
  FuncDecl (Rewriter &Rewrite) : Rewrite (Rewrite) {}

  virtual void run(const MatchFinder::MatchResult &MR) {
    const FunctionDecl* FD = MR.Nodes.getNodeAs<clang::FunctionDecl>("funcdecl");
    SourceLocation SL = FD->getSourceRange().getBegin();
    if (Devi::IsTheMatchInSysHeader(CheckSystemHeader, MR, SL)) return void();
    if (!Devi::IsTheMatchInMainFile(MainFileOnly, MR, SL)) return void();
    
    const ArrayRef<ParmVarDecl*> PVD = FD->parameters();
    QualType RT = FD->getReturnType();
    RT = RT.getCanonicalType();
    const clang::Type* TPR = RT.getTypePtrOrNull();
    const FunctionDecl* FDef = FD->getDefinition();
    const ASTContext* ASTC = MR.Context;
    unsigned NumParam = FD->getNumParams();
    std::string FuncName = FD->getNameAsString();
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

private:
  Rewriter &Rewrite;
};
/**********************************************************************************************************************/
class PPInclusion : public PPCallbacks
{
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
class BlankDiagConsumer : public clang::DiagnosticConsumer
{
  public:
    BlankDiagConsumer() = default;
    virtual ~BlankDiagConsumer() {}
    virtual void HandleDiagnostic(DiagnosticsEngine::Level DiagLevel, const Diagnostic &Info) override {}
};
/**********************************************************************************************************************/
class LCTASTConsumer : public ASTConsumer {
public:
  LCTASTConsumer(Rewriter &R) : funcDeclHandler(R) {
    Matcher.addMatcher(functionDecl().bind("funcdecl"), &funcDeclHandler);
  }

  void HandleTranslationUnit(ASTContext &Context) override {
    Matcher.matchAST(Context);
  }

private:
  FuncDecl funcDeclHandler;
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
    CI.getPreprocessor().addPPCallbacks(llvm::make_unique<PPInclusion>(&CI.getSourceManager(), &TheRewriter));
    DiagnosticsEngine &DE = CI.getPreprocessor().getDiagnostics();
    DE.setClient(BDCProto, false);
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return llvm::make_unique<LCTASTConsumer>(TheRewriter);
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

