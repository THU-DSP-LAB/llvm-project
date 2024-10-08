//===--- Analysis.cpp -----------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang-include-cleaner/Analysis.h"
#include "AnalysisInternal.h"
#include "clang-include-cleaner/Types.h"
#include "clang/AST/ASTContext.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Format/Format.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Tooling/Core/Replacement.h"
#include "clang/Tooling/Inclusions/HeaderIncludes.h"
#include "clang/Tooling/Inclusions/StandardLibrary.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"

namespace clang::include_cleaner {

void walkUsed(llvm::ArrayRef<Decl *> ASTRoots,
              llvm::ArrayRef<SymbolReference> MacroRefs,
              const PragmaIncludes *PI, const SourceManager &SM,
              UsedSymbolCB CB) {
  // This is duplicated in writeHTMLReport, changes should be mirrored there.
  tooling::stdlib::Recognizer Recognizer;
  for (auto *Root : ASTRoots) {
    auto &SM = Root->getASTContext().getSourceManager();
    walkAST(*Root, [&](SourceLocation Loc, NamedDecl &ND, RefType RT) {
      SymbolReference SymRef{Loc, ND, RT};
      if (auto SS = Recognizer(&ND)) {
        // FIXME: Also report forward decls from main-file, so that the caller
        // can decide to insert/ignore a header.
        return CB(SymRef, findHeaders(*SS, SM, PI));
      }
      // FIXME: Extract locations from redecls.
      return CB(SymRef, findHeaders(ND.getLocation(), SM, PI));
    });
  }
  for (const SymbolReference &MacroRef : MacroRefs) {
    assert(MacroRef.Target.kind() == Symbol::Macro);
    CB(MacroRef, findHeaders(MacroRef.Target.macro().Definition, SM, PI));
  }
}

static std::string spellHeader(const Header &H, HeaderSearch &HS,
                               const FileEntry *Main) {
  switch (H.kind()) {
  case Header::Physical: {
    bool IsSystem = false;
    std::string Path = HS.suggestPathToFileForDiagnostics(
        H.physical(), Main->tryGetRealPathName(), &IsSystem);
    return IsSystem ? "<" + Path + ">" : "\"" + Path + "\"";
  }
  case Header::Standard:
    return H.standard().name().str();
  case Header::Verbatim:
    return H.verbatim().str();
  }
  llvm_unreachable("Unknown Header kind");
}

AnalysisResults analyze(llvm::ArrayRef<Decl *> ASTRoots,
                        llvm::ArrayRef<SymbolReference> MacroRefs,
                        const Includes &Inc, const PragmaIncludes *PI,
                        const SourceManager &SM, HeaderSearch &HS) {
  const FileEntry *MainFile = SM.getFileEntryForID(SM.getMainFileID());
  llvm::DenseSet<const Include *> Used;
  llvm::StringSet<> Missing;
  walkUsed(ASTRoots, MacroRefs, PI, SM,
           [&](const SymbolReference &Ref, llvm::ArrayRef<Header> Providers) {
             bool Satisfied = false;
             for (const Header &H : Providers) {
               if (H.kind() == Header::Physical && H.physical() == MainFile)
                 Satisfied = true;
               for (const Include *I : Inc.match(H)) {
                 Used.insert(I);
                 Satisfied = true;
               }
             }
             if (!Satisfied && !Providers.empty() &&
                 Ref.RT == RefType::Explicit)
               Missing.insert(spellHeader(Providers.front(), HS, MainFile));
           });

  AnalysisResults Results;
  for (const Include &I : Inc.all())
    if (!Used.contains(&I))
      Results.Unused.push_back(&I);
  for (llvm::StringRef S : Missing.keys())
    Results.Missing.push_back(S.str());
  llvm::sort(Results.Missing);
  return Results;
}

std::string fixIncludes(const AnalysisResults &Results, llvm::StringRef Code,
                        const format::FormatStyle &Style) {
  assert(Style.isCpp() && "Only C++ style supports include insertions!");
  tooling::Replacements R;
  // Encode insertions/deletions in the magic way clang-format understands.
  for (const Include *I : Results.Unused)
    cantFail(R.add(tooling::Replacement("input", UINT_MAX, 1, I->quote())));
  for (llvm::StringRef Spelled : Results.Missing)
    cantFail(R.add(tooling::Replacement("input", UINT_MAX, 0,
                                        ("#include " + Spelled).str())));
  // "cleanup" actually turns the UINT_MAX replacements into concrete edits.
  auto Positioned = cantFail(format::cleanupAroundReplacements(Code, R, Style));
  return cantFail(tooling::applyAllReplacements(Code, Positioned));
}

} // namespace clang::include_cleaner
