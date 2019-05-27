
/*first line intentionally left blank.*/
/***************************************************Project Mutator****************************************************/
//-*-c++-*-
// a clang tool blueprint
/*Copyright (C) 2018 Farzad Sadeghi

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.*/
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
  static llvm::cl::OptionCategory ctbcat("Obfuscator custom options");
  cl::opt<bool> CheckSystemHeader("SysHeader", cl::desc("mutator-lvl0 will run through System Headers"), cl::init(false), cl::cat(ctbcat), cl::ZeroOrMore);
  cl::opt<bool> MainFileOnly("MainOnly", cl::desc("mutator-lvl0 will only report the results that reside in the main file"), cl::init(false), cl::cat(ctbcat), cl::ZeroOrMore);
  std::string TEMP_FILE="/tmp/generic_temp";
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
/**********************************************************************************************************************/
class CalledFunc : public MatchFinder::MatchCallback {
  public:
    CalledFunc(Rewriter &Rewrite) : Rewrite(Rewrite) {}

    virtual void run(const MatchFinder::MatchResult &MR) {}

  private:
    Rewriter &Rewrite;
};
/**********************************************************************************************************************/
class CalledVar : public MatchFinder::MatchCallback {
  public:
    CalledVar (Rewriter &Rewrite) : Rewrite(Rewrite) {}

    virtual void run(const MatchFinder::MatchResult &MR) {
      const VarDecl* VD = MR.Nodes.getNodeAs<clang::VarDecl>("vardecl");
    }

  private:
    Rewriter &Rewrite;
};
/**********************************************************************************************************************/
class FuncDecl : public MatchFinder::MatchCallback
{
public:
  FuncDecl (Rewriter &Rewrite) : Rewrite (Rewrite) {}

  virtual void run(const MatchFinder::MatchResult &MR) {
    const FunctionDecl* FD = MR.Nodes.getNodeAs<clang::FunctionDecl>("funcdecl");
    const ArrayRef<ParmVarDecl*> PVD = FD->parameters();
    const QualType RT = FD->getReturnType();
  }

private:
  Rewriter &Rewrite;
};
/**********************************************************************************************************************/
class VDecl : public MatchFinder::MatchCallback
{
public:
  VDecl (Rewriter &Rewrite) : Rewrite (Rewrite) {}

  virtual void run(const MatchFinder::MatchResult &MR) {}

private:
  Rewriter &Rewrite;
};
/**********************************************************************************************************************/
class ClassDecl : public MatchFinder::MatchCallback {
  public:
    ClassDecl (Rewriter &Rewrite) : Rewrite(Rewrite) {}

    virtual void run(const MatchFinder::MatchResult &MR) {}

  private:
    Rewriter &Rewrite;
};
/**********************************************************************************************************************/
class SFCPPARR02SUB : public MatchFinder::MatchCallback
{
  public:
    SFCPPARR02SUB (Rewriter &Rewrite) : Rewrite(Rewrite) {}

    virtual void run(const MatchFinder::MatchResult &MR)
    {
      if (MR.Nodes.getNodeAs<clang::DeclRefExpr>("sfcpp02sub") != nullptr)
      {
        const DeclRefExpr* DRE = MR.Nodes.getNodeAs<clang::DeclRefExpr>("sfcpp02sub");
        SourceManager *const SM = MR.SourceManager;
        SourceLocation SL = DRE->DEVI_GETLOCSTART();
        CheckSLValidity(SL);
        SL = SM->getSpellingLoc(SL);
        if (Devi::IsTheMatchInSysHeader(CheckSystemHeader, MR, SL)) {return void();}
        if (!Devi::IsTheMatchInMainFile(MainFileOnly, MR, SL)) {return void();}
        const NamedDecl* ND = DRE->getFoundDecl();
        SourceLocation OriginSL = ND->DEVI_GETLOCSTART();
        CheckSLValidity(OriginSL);
        OriginSL = SM->getSpellingLoc(OriginSL);
        StringRef OriginFileName [[maybe_unused]] = SM->getFilename(OriginSL);

        if (OriginSL == ExtOriginSL && OriginFileName == ExtOriginFileName) {
          std::cout << "SaferCPP01" << ":" << "Native Array used - pointer points to an array:" << SL.printToString(*MR.SourceManager) << ":" << DRE->getFoundDecl()->getName().str() << "\n";
        }

      }
    }

    void setOriginSourceLocation(SourceLocation inSL)
    {
    ExtOriginSL = inSL;
    }

    void setOriginFileName(StringRef inStrRef)
    {
      ExtOriginFileName = inStrRef;
    }

  private:
    Rewriter &Rewrite;
    SourceLocation ExtOriginSL;
    StringRef ExtOriginFileName;
};
/**********************************************************************************************************************/
/**
 * @brief MatchCallback for safercpp matching of pointers pointing to arrays.
 */
class SFCPPARR02 : public MatchFinder::MatchCallback
{
  public:
    SFCPPARR02 (Rewriter &Rewrite) : Rewrite(Rewrite), SubHandler(Rewrite) {}

    virtual void run(const MatchFinder::MatchResult &MR)
    {
      if (MR.Nodes.getNodeAs<clang::DeclRefExpr>("sfcpparrdeep") != nullptr)
      {
        const DeclRefExpr* DRE = MR.Nodes.getNodeAs<clang::DeclRefExpr>("sfcpparrdeep");
        ASTContext *const ASTC = MR.Context;
        SourceManager *const SM = MR.SourceManager;
        SourceLocation SL = DRE->DEVI_GETLOCSTART();
        CheckSLValidity(SL);
        SL = SM->getSpellingLoc(SL);
        const NamedDecl* ND = DRE->getFoundDecl();
        StringRef NDName = ND->getName();
        SubHandler.setOriginSourceLocation(SM->getSpellingLoc(ND->DEVI_GETLOCSTART()));
        SubHandler.setOriginFileName(SM->getFilename(SM->getSpellingLoc(ND->DEVI_GETLOCSTART())));
        Matcher.addMatcher(declRefExpr(to(varDecl(hasName(NDName.str())))).bind("sfcpp02sub"), &SubHandler);
        Matcher.matchAST(*ASTC);
      }
    }

  private:
    Rewriter &Rewrite;
    MatchFinder Matcher;
    SFCPPARR02SUB SubHandler;
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
/**
 * @brief A Clang Diagnostic Consumer that does nothing.
 */
class BlankDiagConsumer : public clang::DiagnosticConsumer
{
  public:
    BlankDiagConsumer() = default;
    virtual ~BlankDiagConsumer() {}
    virtual void HandleDiagnostic(DiagnosticsEngine::Level DiagLevel, const Diagnostic &Info) override {}
};
/**********************************************************************************************************************/
class MyASTConsumer : public ASTConsumer {
public:
  MyASTConsumer(Rewriter &R) : funcDeclHandler(R), HandlerForVar(R), HandlerForClass(R), HandlerForCalledFunc(R), HandlerForCalledVar(R), HandlerForSFCPPARR02(R) {
#if 1
    Matcher.addMatcher(functionDecl().bind("funcdecl"), &funcDeclHandler);
    Matcher.addMatcher(varDecl(anyOf(unless(hasDescendant(expr(anything()))), hasDescendant(expr(anything()).bind("expr")))).bind("vardecl"), &HandlerForVar);
    Matcher.addMatcher(recordDecl(isClass()).bind("classdecl"), &HandlerForClass);
    Matcher.addMatcher(declRefExpr().bind("calledvar"), &HandlerForCalledVar);
    Matcher.addMatcher(varDecl(hasType(arrayType())).bind("sfcpparrdecl"), &HandlerForSFCPPARR02);
    Matcher.addMatcher(fieldDecl(hasType(arrayType())).bind("sfcpparrfield"), &HandlerForSFCPPARR02);
    Matcher.addMatcher(implicitCastExpr(hasCastKind(CK_ArrayToPointerDecay)).bind("sfcpparrcastexpr"), &HandlerForSFCPPARR02);
    Matcher.addMatcher(cStyleCastExpr(hasCastKind(CK_ArrayToPointerDecay)).bind("sfcpparrcastexpr"), &HandlerForSFCPPARR02);
    Matcher.addMatcher(declRefExpr(hasAncestor(binaryOperator(allOf(hasLHS(declRefExpr().bind("sfcpparrdeep")), hasRHS(hasDescendant(implicitCastExpr(hasCastKind(CK_ArrayToPointerDecay)))), hasOperatorName("="))))), &HandlerForSFCPPARR02);
#endif
  }

  void HandleTranslationUnit(ASTContext &Context) override {
    Matcher.matchAST(Context);
  }

private:
  FuncDecl funcDeclHandler;
  VDecl HandlerForVar;
  ClassDecl HandlerForClass;
  CalledFunc HandlerForCalledFunc;
  CalledVar HandlerForCalledVar;
  SFCPPARR02 HandlerForSFCPPARR02;
  MatchFinder Matcher;
};
/**********************************************************************************************************************/
class ObfFrontendAction : public ASTFrontendAction {
public:
  ObfFrontendAction() {}
  ~ObfFrontendAction() {
    delete BDCProto;
    delete tee;
  }

  void EndSourceFileAction() override {
    std::error_code EC;
    std::string OutputFilename = TEMP_FILE;
    TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID()).write(llvm::outs());
    tee = new raw_fd_ostream(StringRef(OutputFilename), EC, sys::fs::F_None);
    TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID()).write(*tee);
  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
    CI.getPreprocessor().addPPCallbacks(llvm::make_unique<PPInclusion>(&CI.getSourceManager(), &TheRewriter));
    DiagnosticsEngine &DE = CI.getPreprocessor().getDiagnostics();
    DE.setClient(BDCProto, false);
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return llvm::make_unique<MyASTConsumer>(TheRewriter);
  }

private:
  BlankDiagConsumer* BDCProto = new BlankDiagConsumer;
  Rewriter TheRewriter;
  raw_ostream *tee = &llvm::outs();
};
/**********************************************************************************************************************/
/**********************************************************************************************************************/
/*Main*/
int main(int argc, const char **argv) {
  CommonOptionsParser op(argc, argv, ctbcat);
  const std::vector<std::string> &SourcePathList = op.getSourcePathList();
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());
  return Tool.run(newFrontendActionFactory<ObfFrontendAction>().get());
}
/**********************************************************************************************************************/
/*last line intentionally left blank.*/

