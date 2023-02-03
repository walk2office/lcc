/***********************************
 * File:     Parser.cc
 *
 * Author:   蔡鹏
 *
 * Email:    iiicp@outlook.com
 *
 * Date:     2022/10/17
 ***********************************/

#include "Parser.h"
#include <algorithm>
#include <unordered_set>
#include "Utilities.h"

namespace lcc {
namespace set {
template <typename Set>
static inline Set set_union(const Set &lhs, const Set &rhs) {
  Set res{lhs};
  res.insert(rhs.begin(), rhs.end());
  return std::move(res);
}
template <typename Set, typename Key = typename Set::value_type>
static inline Set set_intersection(const Set &lhs, const Set &rhs) {
  if (lhs.size() <= rhs.size()) {
    Set res;
    for (const Key &key : lhs) {
      if (rhs.count(key) > 0) {
        res.insert(key);
      }
    }
    return std::move(res);
  } else {
    return set_intersection(rhs, lhs);
  }
}
} // namespace set

Parser::Parser(std::vector<Token> && tokens)
    : mTokens(std::move(tokens)), mTokCursor(mTokens.cbegin()),
      mTokEnd(mTokens.cend()) {
  mFirstPostFixSet = {tok::arrow,   tok::period,    tok::l_square,
                      tok::l_paren, tok::plus_plus, tok::minus_minus};
  mAssignmentSet = {tok::equal,       tok::plus_equal,    tok::minus_equal,
                    tok::slash_equal, tok::star_equal,    tok::percent_equal,
                    tok::less_equal,  tok::greater_equal, tok::amp_equal,
                    tok::pipe_equal,  tok::caret_equal};
  mFirstSpecifierQualifierSet = {
      tok::kw_void,   tok::kw_char,     tok::kw_short,    tok::kw_int,
      tok::kw_long,   tok::kw_float,    tok::kw_double,   tok::kw__Bool,
      tok::kw_signed, tok::kw_unsigned, tok::kw_enum,     tok::kw_struct,
      tok::kw_union,  tok::kw_const,    tok::kw_restrict, tok::kw_volatile,
      tok::kw_inline, tok::identifier};
  mFirstDeclarationSpecifierSet = {
      tok::kw_typedef,  tok::kw_extern, tok::kw_static,   tok::kw_auto,
      tok::kw_register, tok::kw_void,   tok::kw_char,     tok::kw_short,
      tok::kw_int,      tok::kw_long,   tok::kw_float,    tok::kw_double,
      tok::kw__Bool,    tok::kw_signed, tok::kw_unsigned, tok::kw_enum,
      tok::kw_struct,   tok::kw_union,  tok::kw_const,    tok::kw_restrict,
      tok::kw_volatile, tok::kw_inline, tok::identifier};
  mFirstPointerSet = {tok::star};
  mFirstParameterListSet = mFirstDeclarationSpecifierSet;
  mFirstDirectAbstractDeclaratorSet = {tok::l_paren, tok::l_square};
  mFirstAbstractDeclaratorSet =
      set::set_union(mFirstPointerSet, mFirstDirectAbstractDeclaratorSet);
  mFirstParameterTypeListSet = mFirstParameterListSet;
  mFirstDirectDeclaratorSet = {tok::identifier, tok::l_paren};
  mFirstDeclaratorSet =
      set::set_union(mFirstPointerSet, mFirstDirectDeclaratorSet);
  mFirstDeclarationSet = mFirstDeclarationSpecifierSet;
  mFirstExpressionSet = {
      tok::l_paren,       tok::identifier,     tok::numeric_constant,
      tok::char_constant, tok::string_literal, tok::plus_plus,
      tok::minus_minus,   tok::minus,          tok::plus,
      tok::amp,           tok::tilde,          tok::exclaim,
      tok::kw_sizeof};
  std::set<tok::TokenKind> st1{tok::l_brace};
  mFirstInitializerSet = set::set_union(mFirstExpressionSet, st1);
  std::set<tok::TokenKind> st2{tok::l_square, tok::period};
  mFirstInitializerListSet = set::set_union(mFirstInitializerSet, st2);
  std::set<tok::TokenKind> st3{
      tok::kw_if,       tok::kw_for,   tok::l_brace,  tok::kw_switch,
      tok::kw_continue, tok::kw_break, tok::kw_case,  tok::kw_default,
      tok::identifier,  tok::kw_do,    tok::kw_while, tok::kw_return,
      tok::kw_goto,     tok::semi};
  mFirstStatementSet = set::set_union(st3, mFirstExpressionSet);
  mFirstBlockItem = set::set_union(mFirstDeclarationSet, mFirstStatementSet);
  mFirstFunctionDefinitionSet = mFirstDeclarationSpecifierSet;
  mFirstExternalDeclarationSet =
      set::set_union(mFirstDeclarationSet, mFirstFunctionDefinitionSet);
}

Syntax::TranslationUnit Parser::ParseTranslationUnit() {
  std::vector<Syntax::ExternalDeclaration> decls;
  while (mTokCursor != mTokEnd) {
    auto result = ParseExternalDeclaration();
    if (result) {
      decls.push_back(std::move(*result));
    }
  }
  return Syntax::TranslationUnit(std::move(decls));
}

Syntax::DeclarationSpecifiers Parser::ParseDeclarationSpecifiers() {
  Syntax::DeclarationSpecifiers declarationSpecifiers;
  bool seeTy = false;
next_specifier:
  switch (mTokCursor->getTokenKind()) {
  case tok::kw_auto: {
    declarationSpecifiers.addStorageClassSpecifier(
        Syntax::StorageClassSpecifier(Syntax::StorageClassSpecifier::Auto));
    Consume(tok::kw_auto);
    break;
  }
  case tok::kw_register: {
    declarationSpecifiers.addStorageClassSpecifier(
        Syntax::StorageClassSpecifier(Syntax::StorageClassSpecifier::Register));
    Consume(tok::kw_register);
    break;
  }
  case tok::kw_static: {
    declarationSpecifiers.addStorageClassSpecifier(
        Syntax::StorageClassSpecifier(Syntax::StorageClassSpecifier::Static));
    Consume(tok::kw_static);
    break;
  }
  case tok::kw_extern: {
    declarationSpecifiers.addStorageClassSpecifier(
        Syntax::StorageClassSpecifier(Syntax::StorageClassSpecifier::Extern));
    Consume(tok::kw_extern);
    break;
  }
  case tok::kw_typedef: {
    declarationSpecifiers.addStorageClassSpecifier(
        Syntax::StorageClassSpecifier(Syntax::StorageClassSpecifier::Typedef));
    Consume(tok::kw_typedef);
    break;
  }
  case tok::kw_volatile: {
    declarationSpecifiers.addTypeQualifier(Syntax::TypeQualifier(Syntax::TypeQualifier::Volatile));
    Consume(tok::kw_volatile);
    break;
  }
  case tok::kw_const: {
    declarationSpecifiers.addTypeQualifier(Syntax::TypeQualifier(Syntax::TypeQualifier::Const));
    Consume(tok::kw_const);
    break;
  }
  case tok::kw_restrict: {
    declarationSpecifiers.addTypeQualifier(Syntax::TypeQualifier(Syntax::TypeQualifier::Restrict));
    Consume(tok::kw_restrict);
    break;
  }
  case tok::kw_void: {
    seeTy = true;
    declarationSpecifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Void));
    Consume(tok::kw_void);
    break;
  }
  case tok::kw_char: {
    seeTy = true;
    declarationSpecifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Char));
    Consume(tok::kw_char);
    break;
  }
  case tok::kw_short: {
    seeTy = true;
    declarationSpecifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Short));
    Consume(tok::kw_short);
    break;
  }
  case tok::kw_int: {
    seeTy = true;
    declarationSpecifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Int));
    Consume(tok::kw_int);
    break;
  }
  case tok::kw_long: {
    seeTy = true;
    declarationSpecifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Long));
    Consume(tok::kw_long);
    break;
  }
  case tok::kw_float: {
    seeTy = true;
    declarationSpecifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Float));
    Consume(tok::kw_float);
    break;
  }
  case tok::kw_double: {
    seeTy = true;
    declarationSpecifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Double));
    Consume(tok::kw_double);
    break;
  }
  case tok::kw_signed: {
    seeTy = true;
    declarationSpecifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Signed));
    Consume(tok::kw_signed);
    break;
  }
  case tok::kw_unsigned: {
    seeTy = true;
    declarationSpecifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Unsigned));
    Consume(tok::kw_unsigned);
    break;
  }
  case tok::kw_union:
  case tok::kw_struct: {
    auto expected = ParseStructOrUnionSpecifier();
    if (!expected) {
      LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "parse struct or union specifier error");
    }
    declarationSpecifiers.addTypeSpecifier(
        Syntax::TypeSpecifier(std::make_unique<Syntax::StructOrUnionSpecifier>(std::move(*expected))));
    seeTy = true;
    break;
  }
  case tok::kw_enum: {
    auto expected = ParseEnumSpecifier();
    if (!expected) {
      LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "parse enum specifier error");
    }
    declarationSpecifiers.addTypeSpecifier(Syntax::TypeSpecifier(
        std::make_unique<Syntax::EnumSpecifier>(std::move(*expected))));
    seeTy = true;
    break;
  }
  case tok::identifier: {
    auto name = mTokCursor->getContent();
    if (!seeTy && mScope.isTypedefInScope(name)) {
      Consume(tok::identifier);
      declarationSpecifiers.addTypeSpecifier(Syntax::TypeSpecifier(name));
      break;
    }
    return declarationSpecifiers;
  }
  default:
    return declarationSpecifiers;
  }
  goto next_specifier;
}

Syntax::SpecifierQualifiers Parser::ParseSpecifierQualifierList() {
  Syntax::SpecifierQualifiers specifierQualifiers;
  bool seeTy = false;
next_specifier:
  switch (mTokCursor->getTokenKind()) {
  case tok::kw_volatile: {
    specifierQualifiers.addTypeQualifier(Syntax::TypeQualifier(Syntax::TypeQualifier::Volatile));
    Consume(tok::kw_volatile);
    break;
  }
  case tok::kw_const: {
    specifierQualifiers.addTypeQualifier(Syntax::TypeQualifier(Syntax::TypeQualifier::Const));
    Consume(tok::kw_const);
    break;
  }
  case tok::kw_restrict: {
    specifierQualifiers.addTypeQualifier(Syntax::TypeQualifier(Syntax::TypeQualifier::Restrict));
    Consume(tok::kw_restrict);
    break;
  }
  case tok::kw_void: {
    seeTy = true;
    specifierQualifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Void));
    Consume(tok::kw_void);
    break;
  }
  case tok::kw_char: {
    seeTy = true;
    specifierQualifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Char));
    Consume(tok::kw_char);
    break;
  }
  case tok::kw_short: {
    seeTy = true;
    specifierQualifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Short));
    Consume(tok::kw_short);
    break;
  }
  case tok::kw_int: {
    seeTy = true;
    specifierQualifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Int));
    Consume(tok::kw_int);
    break;
  }
  case tok::kw_long: {
    seeTy = true;
    specifierQualifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Long));
    Consume(tok::kw_long);
    break;
  }
  case tok::kw_float: {
    seeTy = true;
    specifierQualifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Float));
    Consume(tok::kw_float);
    break;
  }
  case tok::kw_double: {
    seeTy = true;
    specifierQualifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Double));
    Consume(tok::kw_double);
    break;
  }
  case tok::kw_signed: {
    seeTy = true;
    specifierQualifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Signed));
    Consume(tok::kw_signed);
    break;
  }
  case tok::kw_unsigned: {
    seeTy = true;
    specifierQualifiers.addTypeSpecifier(Syntax::TypeSpecifier(Syntax::TypeSpecifier::Unsigned));
    Consume(tok::kw_unsigned);
    break;
  }
  case tok::kw_union:
  case tok::kw_struct: {
    auto expected = ParseStructOrUnionSpecifier();
    if (!expected) {
      LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "parse struct or union specifier error");
    }
    specifierQualifiers.addTypeSpecifier(
        Syntax::TypeSpecifier(std::make_unique<Syntax::StructOrUnionSpecifier>(std::move(*expected))));
    seeTy = true;
    break;
  }
  case tok::kw_enum: {
    auto expected = ParseEnumSpecifier();
    if (!expected) {
      LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "parse enum specifier error");
    }
    specifierQualifiers.addTypeSpecifier(Syntax::TypeSpecifier(
        std::make_unique<Syntax::EnumSpecifier>(std::move(*expected))));
    seeTy = true;
    break;
  }
  case tok::identifier: {
    auto name = mTokCursor->getContent();
    if (!seeTy && mScope.isTypedefInScope(name)) {
      Consume(tok::identifier);
      specifierQualifiers.addTypeSpecifier(Syntax::TypeSpecifier(name));
      break;
    }
    return specifierQualifiers;
  }
  default:
    return specifierQualifiers;
  }
  goto next_specifier;
}

std::optional<Syntax::Declaration> Parser::FinishDeclaration(
    Syntax::DeclarationSpecifiers &&declarationSpecifiers,
    std::optional<Syntax::Declarator> alreadyParsedDeclarator) {
  bool isTypedef = std::any_of(declarationSpecifiers.getStorageClassSpecifiers().begin(), declarationSpecifiers.getStorageClassSpecifiers().end(),
   [](const Syntax::StorageClassSpecifier& storage) {
     return storage.getSpecifier() == Syntax::StorageClassSpecifier::Typedef;
   });
  std::vector<Syntax::Declaration::InitDeclarator> initDeclarators;
  if (alreadyParsedDeclarator) {
    if (!isTypedef) {
      auto name = getDeclaratorName(*alreadyParsedDeclarator);
      mScope.addToScope(name);
    }
    if (!Peek(tok::equal)) {
      initDeclarators.push_back({
          std::make_unique<Syntax::Declarator>
              (std::move(*alreadyParsedDeclarator)),nullptr});
    }else {
      Consume(tok::equal);
      auto initializer = ParseInitializer();
      if (!initializer) {
        LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "parse initializer error");
        return {};
      }
      initDeclarators.push_back({
          std::make_unique<Syntax::Declarator>(std::move(*alreadyParsedDeclarator)),
              std::make_unique<Syntax::Initializer>(std::move(*initializer))});
    }
  }
  if(Peek(tok::comma)) {
    Consume(tok::comma);
  }
  auto declarator = ParseDeclarator();
  if (!declarator) {
    LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "parse declarator error");
    return {};
  }
  if (!isTypedef) {
    auto name = getDeclaratorName(*declarator);
    mScope.addToScope(name);
  }
  if (!Peek(tok::equal)) {
    initDeclarators.push_back({
        std::make_unique<Syntax::Declarator>
        (std::move(*declarator)),nullptr});
  }else {
    Consume(tok::equal);
    auto initializer = ParseInitializer();
    if (!initializer) {
      LOGE(mTokCursor->getLine(), mTokCursor->getColumn(),
           "parse initializer error");
      return {};
    }
    initDeclarators.push_back(
        {std::make_unique<Syntax::Declarator>(std::move(*declarator)),
         std::make_unique<Syntax::Initializer>(std::move(*initializer))});
  }

  while (Peek(tok::comma)) {
    Consume(tok::comma);
    declarator = ParseDeclarator();
    if (!declarator) {
      LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "parse declarator error");
      return {};
    }
    if (!Peek(tok::equal)) {
      initDeclarators.push_back(
          {std::make_unique<Syntax::Declarator>(std::move(*declarator)),
           nullptr});
    } else {
      Consume(tok::equal);
      auto initializer = ParseInitializer();
      if (!initializer) {
        LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "parse initializer error");
        return {};
      }
      initDeclarators.push_back(
          {std::make_unique<Syntax::Declarator>(std::move(*declarator)),
           std::make_unique<Syntax::Initializer>(
               std::move(*initializer))});
    }
  }
  Consume(tok::semi);
  if (isTypedef) {
    for (auto& iter : initDeclarators) {
      auto name = getDeclaratorName(*iter.mDeclarator);
      mScope.addTypedef(name);
    }
  }
  return Syntax::Declaration(std::move(declarationSpecifiers), std::move(initDeclarators));
}

std::optional<Syntax::ExternalDeclaration> Parser::ParseExternalDeclaration() {
  auto declarationSpecifiers = ParseDeclarationSpecifiers();
  if (declarationSpecifiers.isEmpty()) {
    LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "expect declaration specifier");
    return {};
  }
  if (Peek(tok::semi)) {
    Consume(tok::semi);
    return Syntax::Declaration(std::move(declarationSpecifiers), {});
  }
  auto declarator = ParseDeclarator();
  if (!declarator) {
    LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "parse declarator error");
    return {};
  }
  /// function define
  if (Peek(tok::l_brace)) {
    auto *parameters = std::get_if<Syntax::DirectDeclaratorParentParamTypeList>(&declarator->getDirectDeclarator());
    if (!parameters) {
      LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "expect function declarator");
      return {};
    }
    mScope.pushScope();
    auto &parameterDeclarations = parameters->getParameterTypeList().getParameterList().getParameterDeclarations();
    for (auto &[declaratorSpecifiers, parameterDeclarator] : parameterDeclarations) {
      /// check is void
      if (declaratorSpecifiers.getStorageClassSpecifiers().size() == 0 &&
          declaratorSpecifiers.getTypeQualifiers().size() == 0 &&
          declaratorSpecifiers.getFunctionSpecifiers().size() == 0 &&
          parameterDeclarations.size() == 1 && declaratorSpecifiers.getTypeSpecifiers().size() == 1) {
        auto* primitive = std::get_if<Syntax::TypeSpecifier::PrimitiveTypeSpecifier>(
            &declaratorSpecifiers.getTypeSpecifiers()[0].getVariant());
        if (primitive && *primitive == Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Void) {
          break;
        }
      }
      if (std::holds_alternative<std::unique_ptr<Syntax::AbstractDeclarator>>(parameterDeclarator)) {
        LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "func define need param name");
      }
      auto& decl = std::get<std::unique_ptr<Syntax::Declarator>>(parameterDeclarator);
      mScope.addToScope(getDeclaratorName(*decl));
    }
    auto compoundStmt = ParseBlockStmt();
    mScope.popScope();
    if (!compoundStmt) {
      LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "parse block stmt error");
      return {};
    }
    mScope.addToScope(getDeclaratorName(*declarator));
    return Syntax::FunctionDefinition(std::move(declarationSpecifiers),
                                      std::move(*declarator),std::move(*compoundStmt));
  }
  /// is global declaration
  return FinishDeclaration(std::move(declarationSpecifiers),
                           std::move(declarator));
}

/// declaration: declaration-specifiers init-declarator-list{opt} ;
std::optional<Syntax::Declaration> Parser::ParseDeclaration() {
  auto declarationSpecifiers = ParseDeclarationSpecifiers();
  if (declarationSpecifiers.getStorageClassSpecifiers().empty()
      && declarationSpecifiers.getTypeQualifiers().empty()
      && declarationSpecifiers.getTypeSpecifiers().empty()) {
    LOGE(mTokCursor->getLine(), mTokCursor->getColumn(),
           "Expected declaration specifiers at beginning of declaration");
  }
  if (Peek(tok::semi)) {
    Consume(tok::semi);
    return Syntax::Declaration(std::move(declarationSpecifiers), {});
  }
  return FinishDeclaration(std::move(declarationSpecifiers));
}

std::optional<Syntax::StructOrUnionSpecifier>
Parser::ParseStructOrUnionSpecifier() {
  bool isUnion;
  if (Peek(tok::kw_union)) {
    isUnion = true;
  } else if (Peek(tok::kw_struct)) {
    isUnion = true;
  } else {
    LOGE(mTokCursor->getLine(), mTokCursor->getColumn(),
           "Expected struct or union keyword at beginning of struct or union "
           "specifier");
    return {};
  }
  ConsumeAny();
  std::string name{""};
  if (Peek(tok::identifier)) {
    name = mTokCursor->getContent();
    Consume(tok::identifier);
  }

  if (!Peek(tok::l_brace)) {
    if (name.empty()) {
      LOGE(mTokCursor->getLine(), mTokCursor->getColumn(),
           std::string("Expected identifier after") +
               (isUnion ? "union" : "struct"));
      return {};
    }
    return Syntax::StructOrUnionSpecifier(isUnion, name, {});
  }
  Consume(tok::l_brace);
  std::vector<Syntax::StructOrUnionSpecifier::StructDeclaration>
      structDeclarations;
  while (IsFirstInSpecifierQualifier()) {
    auto specifierQualifiers = ParseSpecifierQualifierList();
    if (specifierQualifiers.isEmpty()) {
      LOGE(mTokCursor->getLine(), mTokCursor->getColumn(),
          "Expected Specifier Qualifiers at beginning of struct declarations");
    }
    std::vector<Syntax::StructOrUnionSpecifier::StructDeclaration::StructDeclarator> declarators;
    auto declarator = ParseDeclarator();
    if (!declarator) {
      LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "parse struct declarator error");
      return {};
    }
    if (Peek(tok::colon)) {
      Consume(tok::colon);
      auto constant = ParseConditionalExpr();
      declarators.push_back({std::make_unique<Syntax::Declarator>(std::move(*declarator)),
                             std::move(constant)});
    }else {
      declarators.push_back({std::make_unique<Syntax::Declarator>(std::move(*declarator)),
                             std::nullopt});
    }
    while (Peek(tok::comma)) {
      Consume(tok::comma);
      declarator = ParseDeclarator();
      if (!declarator) {
        LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "parse struct declarator error");
        return {};
      }
      if (Peek(tok::colon)) {
        Consume(tok::colon);
        auto constant = ParseConditionalExpr();
        declarators.push_back({std::make_unique<Syntax::Declarator>(std::move(*declarator)),
                               std::move(constant)});
      }else {
        declarators.push_back({std::make_unique<Syntax::Declarator>(std::move(*declarator)),
                               std::nullopt});
      }
    }
    Match(tok::semi);
    structDeclarations.push_back(
        {std::move(specifierQualifiers), std::move(declarators)});
  }
  Match(tok::r_brace);
  return Syntax::StructOrUnionSpecifier(isUnion, name,
                                        std::move(structDeclarations));
}

/// declarator: pointer{opt} direct-declarator
std::optional<Syntax::Declarator> Parser::ParseDeclarator() {
  std::vector<Syntax::Pointer> pointers;
  while (Peek(tok::star)) {
    auto result = ParsePointer();
    pointers.push_back(std::move(result));
  }
  auto directDeclarator = ParseDirectDeclarator();
  if (!directDeclarator) {
    LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "parse direct declarator error");
    return {};
  }
  return Syntax::Declarator(std::move(pointers), std::move(*directDeclarator));
}

std::optional<Syntax::DirectDeclarator>
Parser::ParseDirectDeclaratorSuffix(std::unique_ptr<Syntax::DirectDeclarator>&& directDeclarator) {
  while (Peek(tok::l_paren) || Peek(tok::l_square)) {
    switch (mTokCursor->getTokenKind()) {
    case tok::l_paren: {
      Consume(tok::l_paren);
      if (IsFirstInParameterTypeList()) {
        auto parameterTypeList = ParseParameterTypeList();
        if (!parameterTypeList)
          return {};
        directDeclarator = std::make_unique<Syntax::DirectDeclarator>(
            Syntax::DirectDeclaratorParentParamTypeList(std::move(directDeclarator),
                                                        std::move(*parameterTypeList))
        );
      }else {
        if (!Peek(tok::r_paren)) {
          LOGE(mTokCursor->getLine(), mTokCursor->getColumn(),
               "expect declaration specifier");
        }
        directDeclarator = std::make_unique<Syntax::DirectDeclarator>(
    Syntax::DirectDeclaratorParentParamTypeList(std::move(directDeclarator),
                                                Syntax::ParamTypeList(Syntax::ParamList({}), false)));
      }
      Match(tok::r_paren);
      break;
    }
    case tok::l_square: {
      Consume(tok::l_square);
      LCC_ASSERT(directDeclarator);
      if (IsFirstInAssignmentExpr()) {
        auto curr = mTokCursor;
        auto assignment = ParseAssignExpr();
        if (!assignment) {
          LOGE(curr->getLine(), curr->getColumn(), "parse assign expr error");
        }
        if (assignment && directDeclarator) {
          directDeclarator = std::make_unique<Syntax::DirectDeclarator>(Syntax::DirectDeclaratorAssignExpr(
              std::move(directDeclarator),
              std::make_unique<Syntax::AssignExpr>(std::move(*assignment))));
        }
      }else {
        LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "expect assign expr");
      }
      Match(tok::r_square);
      break;
    }
    default:
      break;
    }
  }
  if (!directDeclarator)
    return {};
  return std::move(*directDeclarator);
}

/**
direct-declarator:
    identifier
    ( declarator )
    direct-declarator [ type-qualifier-list{opt} assignment-expression{opt} ]
    direct-declarator [ static type-qualifier-list{opt} assignment-expression]
    direct-declarator [type-qualifier-list static assignment-expression]
    direct-declarator [type-qualifier-list{opt} *]
    direct-declarator ( parameter-type-list )
    direct-declarator ( identifier-list{opt} )
 */
std::optional<Syntax::DirectDeclarator> Parser::ParseDirectDeclarator() {
  std::unique_ptr<Syntax::DirectDeclarator> directDeclarator;

  if (Peek(tok::identifier)) {
    auto name = mTokCursor->getContent();
    Consume(tok::identifier);
    directDeclarator = std::make_unique<Syntax::DirectDeclarator>(Syntax::DirectDeclaratorIdent(name));
  }else if (Peek(tok::l_paren)) {
    Consume(tok::l_paren);
    auto curr = mTokCursor;
    auto declarator = ParseDeclarator();
    if (!declarator) {
      LOGE(curr->getLine(), curr->getColumn(), "parse declarator failed");
    }
    directDeclarator = std::make_unique<Syntax::DirectDeclarator>(
        Syntax::DirectDeclaratorParent(std::make_unique<Syntax::Declarator>(std::move(*declarator))));
    Match(tok::r_paren);
  }else {
    LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "parse direct declarator error");
  }

  return ParseDirectDeclaratorSuffix(std::move(directDeclarator));
}

/*
parameter-type-list:
  parameter-list
  parameter-list , ...
 */
std::optional<Syntax::ParamTypeList> Parser::ParseParameterTypeList() {
  auto cur = mTokCursor;
  auto parameterList = ParseParameterList();
  if (!parameterList)
    return {};
  bool hasEllipse = false;
  if (Peek(tok::comma)) {
    Consume(tok::comma);
    Match(tok::ellipsis);
    hasEllipse = true;
  }
  return Syntax::ParamTypeList(std::move(*parameterList), hasEllipse);
}

/**
parameter-list:
    parameter-declaration
    parameter-list , parameter-declaration
 */
std::optional<Syntax::ParamList> Parser::ParseParameterList() {
  std::vector<Syntax::ParameterDeclaration> parameterDeclarations;
  auto declaration = ParseParameterDeclaration();
  if (!declaration)
    return {};
  parameterDeclarations.push_back(std::move(*declaration));
  /// fix parse tok::ellipsis
  while (Peek(tok::comma) && !PeekN(1, tok::ellipsis)) {
    Consume(tok::comma);
    declaration = ParseParameterDeclaration();
    if (!declaration)
      return {};
    parameterDeclarations.push_back(std::move(*declaration));
  }
  return Syntax::ParamList(std::move(parameterDeclarations));
}
/**
parameter-declaration:
    declaration-specifiers declarator
    declaration-specifiers abstract-declarator{opt}

declarator:
    pointer{opt} direct-declarator

abstract-declarator:
    pointer
    pointer{opt} direct-abstract-declarator
*/
std::optional<Syntax::ParameterDeclaration>
Parser::ParseParameterDeclaration() {
  auto declarationSpecifiers = ParseDeclarationSpecifiers();
  LCC_ASSERT(!declarationSpecifiers.isEmpty());
  bool hasStar = false;
  auto result = std::find_if(mTokCursor, mTokEnd,
                             [&hasStar](const Token &token) -> bool {
                               if (token.getTokenKind() == tok::star) {
                                 if (hasStar == false) {
                                   hasStar = true;
                                 }
                                 return false;
                               }
                               return true;
                             });
  /// begin == end or all star
  if (result == mTokEnd) {
    LCC_UNREACHABLE;
  }
  if (result->getTokenKind() == tok::l_square) {
    auto abstractDeclarator = ParseAbstractDeclarator();
    if (!abstractDeclarator)
      return {};
    return Syntax::ParameterDeclaration(
        std::move(declarationSpecifiers),
        std::make_unique<Syntax::AbstractDeclarator>(
            std::move(*abstractDeclarator)));
  }
  else if (result->getTokenKind() == tok::identifier) {
    auto declarator = ParseDeclarator();
    if (!declarator)
      return {};
    return Syntax::ParameterDeclaration(
        std::move(declarationSpecifiers),
        std::make_unique<Syntax::Declarator>(std::move(*declarator)));
  }
  else if (result->getTokenKind() == tok::l_paren) {
    while (result->getTokenKind() == tok::l_paren) {
      result++;
      if (result->getTokenKind() == tok::identifier) {
        auto declarator = ParseDeclarator();
        if (!declarator)
          return {};
        return Syntax::ParameterDeclaration(
            std::move(declarationSpecifiers),
            std::make_unique<Syntax::Declarator>(std::move(*declarator)));
      } else if (result->getTokenKind() != tok::l_paren) {
        auto abstractDeclarator = ParseAbstractDeclarator();
        if (!abstractDeclarator)
          return {};
        return Syntax::ParameterDeclaration(
            std::move(declarationSpecifiers),
            std::make_unique<Syntax::AbstractDeclarator>(
                std::move(*abstractDeclarator)));
      }
    }
  }
  else {
    if (hasStar) {
      auto abstractDeclarator = ParseAbstractDeclarator();
      if (!abstractDeclarator)
        return {};
      return Syntax::ParameterDeclaration(
          std::move(declarationSpecifiers),
          std::make_unique<Syntax::AbstractDeclarator>(
              std::move(*abstractDeclarator)));
    } else {
      /// abstract-declarator{opt}
      return Syntax::ParameterDeclaration(
          std::move(declarationSpecifiers),
          std::unique_ptr<Syntax::AbstractDeclarator>());
    }
  }
  return {};
}

/**
 pointer:
    * type-qualifier-list{opt}
    * type-qualifier-list{opt} pointer
 */
Syntax::Pointer Parser::ParsePointer() {
  Expect(tok::star);
  Consume(tok::star);
  std::vector<Syntax::TypeQualifier> typeQualifier;
  while (Peek(tok::kw_const) || Peek(tok::kw_restrict) ||
         Peek(tok::kw_volatile)) {
    switch (mTokCursor->getTokenKind()) {
    case tok::kw_const:
      typeQualifier.push_back(Syntax::TypeQualifier::Const);
      break;
    case tok::kw_restrict:
      typeQualifier.push_back(Syntax::TypeQualifier::Restrict);
      break;
    case tok::kw_volatile:
      typeQualifier.push_back(Syntax::TypeQualifier::Volatile);
      break;
    default:
      break;
    }
    ConsumeAny();
  }
  return Syntax::Pointer(std::move(typeQualifier));
}

/**
 abstract-declarator:
   pointer
   pointer{opt} direct-abstract-declarator
 */
std::optional<Syntax::AbstractDeclarator> Parser::ParseAbstractDeclarator() {
  auto cur = mTokCursor;
  std::vector<Syntax::Pointer> pointers;
  while (Peek(tok::star)) {
    auto result = ParsePointer();
    pointers.push_back(std::move(result));
  }
  auto result = ParseDirectAbstractDeclarator();
  if (!result) {
    return {};
  }
  return Syntax::AbstractDeclarator(std::move(pointers), std::move(result));
}

std::optional<Syntax::DirectAbstractDeclarator> Parser::ParseDirectAbstractDeclaratorSuffix(std::unique_ptr<Syntax::DirectAbstractDeclarator>&& directAbstractDeclarator) {
  while (Peek(tok::l_paren) || Peek(tok::l_square)) {
    switch (mTokCursor->getTokenKind()) {
    case tok::l_paren: {
      Consume(tok::l_paren);
      /// direct-abstract-declarator{opt} ( parameter-type-list{opt} )
      if (IsFirstInDeclarationSpecifier()) {
        auto parameterTypeList = ParseParameterTypeList();
        if (!parameterTypeList)
          return {};
        directAbstractDeclarator =
            std::make_unique<Syntax::DirectAbstractDeclarator>(
                Syntax::DirectAbstractDeclaratorParamTypeList(
                    std::move(directAbstractDeclarator),
                    std::make_unique<Syntax::ParamTypeList>(
                        std::move(*parameterTypeList))));
      }
      /// abstract-declarator first set
      else if (IsFirstInAbstractDeclarator()) {
        auto abstractDeclarator = ParseAbstractDeclarator();
        if (!abstractDeclarator)
          return {};
        directAbstractDeclarator =
            std::make_unique<Syntax::DirectAbstractDeclarator>(
                std::make_unique<Syntax::AbstractDeclarator>(
                    std::move(*abstractDeclarator)));
      } else {
        /// direct-abstract-declarator{opt} ( parameter-type-list{opt} )
        directAbstractDeclarator =
            std::make_unique<Syntax::DirectAbstractDeclarator>(
                Syntax::DirectAbstractDeclaratorParamTypeList(
                    std::move(directAbstractDeclarator), nullptr));
      }
      Match(tok::r_paren);
      break;
    }
    case tok::l_square: {
      Consume(tok::l_square);
      if (Peek(tok::star)) {
        Consume(tok::star);
        directAbstractDeclarator =
            std::make_unique<Syntax::DirectAbstractDeclarator>(
                std::move(*directAbstractDeclarator));
      } else {
        if (!Peek(tok::r_square)) {
          auto assignment = ParseAssignExpr();
          if (!assignment) {
            return {};
          }
          directAbstractDeclarator =
              std::make_unique<Syntax::DirectAbstractDeclarator>(
                  Syntax::DirectAbstractDeclaratorAssignExpr(
                      std::move(directAbstractDeclarator),
                      std::make_unique<Syntax::AssignExpr>(
                          std::move(*assignment))));
        } else {
          directAbstractDeclarator =
              std::make_unique<Syntax::DirectAbstractDeclarator>(
                  Syntax::DirectAbstractDeclaratorAssignExpr(
                      std::move(directAbstractDeclarator), nullptr));
        }
      }
      Match(tok::r_square);
      break;
    }
    default:
      goto Exit;
    }
  }
Exit:
  LCC_ASSERT(directAbstractDeclarator);
  return std::move(*directAbstractDeclarator);
}

/**
 abstract-declarator:
   pointer
   pointer{opt} direct-abstract-declarator

 direct-abstract-declarator:
    ( abstract-declarator )
    direct-abstract-declarator{opt} [ type-qualifier-list{opt}
assignment-expression{opt} ]
    direct-abstract-declarator{opt} [ static type-qualifier-list{opt}
assignment-expression ]
    direct-abstract-declarator{opt} [ type-qualifier-list static
assignment-expression ] direct-abstract-declarator{opt} [ * ]
    direct-abstract-declarator{opt} ( parameter-type-list{opt} )
 */
std::optional<Syntax::DirectAbstractDeclarator>
Parser::ParseDirectAbstractDeclarator() {
  return ParseDirectAbstractDeclaratorSuffix({});
}

/**
enum-specifier:
    enum identifier{opt} { enumerator-list }
    enum identifier{opt} { enumerator-list , }
    enum identifier
enumerator-list:
    enumerator
    enumerator-list , enumerator
enumerator:
    enumeration-constant
    enumeration-constant = constant-expression
 */
std::optional<Syntax::EnumSpecifier> Parser::ParseEnumSpecifier() {
  Expect(tok::kw_enum);
  Consume(tok::kw_enum);

  std::string name{""};
  if (Peek(tok::identifier)) {
    name = mTokCursor->getContent();
    Consume(tok::identifier);
  }

  if (!Peek(tok::l_brace)) {
    if (name.empty()) {
      LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "expect identifier");
    }
    return Syntax::EnumSpecifier(name);
  }

  if (Peek(tok::l_brace)) {
    auto declaration = ParseEnumDeclaration(name);
    if (!declaration)
      return {};
    return Syntax::EnumSpecifier(std::move(*declaration));
  }
  return Syntax::EnumSpecifier(std::move(name));
}

std::optional<Syntax::EnumeratorList>
Parser::ParseEnumDeclaration(std::string enumName) {
  Expect(tok::l_brace);
  Consume(tok::l_brace);

  std::vector<Syntax::EnumeratorList::Enumerator> values;
  while (Peek(tok::identifier)) {
    Expect(tok::identifier);
    std::string valueName = mTokCursor->getContent();
    mScope.addToScope(valueName);
    Consume(tok::identifier);
    if (Peek(tok::equal)) {
      Consume(tok::equal);
      auto constant = ParseConditionalExpr();
      values.push_back({valueName, std::move(constant)});
    }else {
      values.push_back({valueName});
    }
    if (Peek(tok::comma)) {
      Consume(tok::comma);
    }
  }
  Match(tok::r_brace);
  return Syntax::EnumeratorList(std::move(enumName), std::move(values));
}

std::optional<Syntax::BlockStmt> Parser::ParseBlockStmt() {
  Match(tok::l_brace);
  std::vector<Syntax::BlockItem> items;
  mScope.pushScope();
  while (IsFirstInBlockItem()) {
    auto result = ParseBlockItem();
    if (!result)
      return {};
    items.push_back(std::move(*result));
  }
  mScope.popScope();
  Match(tok::r_brace);
  return Syntax::BlockStmt(std::move(items));
}

std::optional<Syntax::BlockItem> Parser::ParseBlockItem() {
  auto curr = mTokCursor;
  if (IsFirstInDeclarationSpecifier()) {
    auto declaration = ParseDeclaration();
    if (!declaration) {
      LOGE(curr->getLine(), curr->getColumn(), "parse declaration error");
      return {};
    }
    return Syntax::BlockItem(std::move(*declaration));
  }

  auto statement = ParseStmt();
  if (!statement) {
    LOGE(curr->getLine(), curr->getColumn(), "parse stmt error");
    return {};
  }
  return Syntax::BlockItem(std::move(*statement));
}

/**
 initializer:
    assignment-expression
    { initializer-list }
    { initializer-list , }
 */
std::optional<Syntax::Initializer> Parser::ParseInitializer() {
  if (!Peek(tok::l_brace)) {
    auto assignment = ParseAssignExpr();
    if (!assignment) {
      LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "parse assign expr error");
      return {};
    }
    return Syntax::Initializer(std::move(*assignment));
  } else {
    Consume(tok::l_brace);
    auto initializerList = ParseInitializerList();
    if (!initializerList)
      return {};

    if (Peek(tok::comma)) {
      Consume(tok::comma);
    }
    Match(tok::r_brace);
    return Syntax::Initializer{std::move(*initializerList)};
  }
}

/**
initializer-list:
    designation{opt} initializer
    initializer-list , designation{opt} initializer

designation:
    designator-list =
designator-list:
    designator
    designator-list designator
designator:
    [ constant-expression ]
    . identifier
 */
std::optional<Syntax::InitializerList> Parser::ParseInitializerList() {
  typename Syntax::InitializerList::vector vector;
  Syntax::InitializerList::DesignatorList designation;
  while (Peek(tok::l_square) || Peek(tok::period)) {
    if (Peek(tok::l_square)) {
      Consume(tok::l_square);
      auto constant = ParseConditionalExpr();
      if (!constant)
        return {};
      designation.emplace_back(std::move(*constant));
      Match(tok::r_square);
    } else if (Peek(tok::period)) {
      Consume(tok::period);
      Expect(tok::identifier);
      designation.emplace_back(mTokCursor->getContent());
      Consume(tok::identifier);
    }
  }
  if (!designation.empty()) {
    if (Peek(tok::equal)) {
      Consume(tok::equal);
    }
  }
  auto initializer = ParseInitializer();
  if (!initializer)
    return {};
  vector.push_back({std::move(*initializer), std::move(designation)});

  while (Peek(tok::comma)) {
    Consume(tok::comma);
    Syntax::InitializerList::DesignatorList designation;
    while (Peek(tok::l_square) || Peek(tok::period)) {
      if (Peek(tok::l_square)) {
        Consume(tok::l_square);
        auto constant = ParseConditionalExpr();
        if (!constant)
          return {};
        designation.emplace_back(std::move(*constant));
        Match(tok::r_square);
      } else if (Peek(tok::period)) {
        Consume(tok::period);
        Expect(tok::identifier);
        designation.emplace_back(mTokCursor->getContent());
        Consume(tok::identifier);
      }
    }
    if (!designation.empty()) {
      if (Peek(tok::equal)) {
        Consume(tok::equal);
      }
    }
    auto initializer = ParseInitializer();
    if (!initializer)
      return {};
    vector.push_back({std::move(*initializer), std::move(designation)});
  }
  return Syntax::InitializerList{std::move(vector)};
}

std::optional<Syntax::Stmt> Parser::ParseStmt() {
  if (Peek(tok::kw_if)) {
    return ParseIfStmt();
  } else if (Peek(tok::kw_do)) {
    return ParseDoWhileStmt();
  } else if (Peek(tok::kw_while)) {
    return ParseWhileStmt();
  } else if (Peek(tok::kw_for)) {
    return ParseForStmt();
  } else if (Peek(tok::kw_break)) {
    return ParseBreakStmt();
  } else if (Peek(tok::kw_continue)) {
    return ParseContinueStmt();
  } else if (Peek(tok::kw_return)) {
    return ParseReturnStmt();
  } else if (Peek(tok::l_brace)) {
    auto s = ParseBlockStmt();
    if (!s)
      return {};
    return Syntax::Stmt(std::move(*s));
  } else if (Peek(tok::kw_switch)) {
    return ParseSwitchStmt();
  } else if (Peek(tok::kw_default)) {
    return ParseDefaultStmt();
  } else if (Peek(tok::kw_case)) {
    return ParseCaseStmt();
  } else if (Peek(tok::kw_goto)) {
    return ParseGotoStmt();
  } else {
    /// identifier : stmt
    auto start = mTokCursor;
    if (Peek(tok::identifier)) {
      Consume(tok::identifier);
      if (Peek(tok::colon)) {
        Consume(tok::colon);
        auto name = start->getContent();
        return Syntax::Stmt(Syntax::LabelStmt(name));
      }else {
        mTokCursor = start;
        return ParseExprStmt();
      }
    }else {
      /// expr{opt};
      return ParseExprStmt();
    }
  }
}

/// if ( expression ) statement
/// if ( expression ) statement else statement
std::optional<Syntax::Stmt> Parser::ParseIfStmt() {
  Consume(tok::kw_if);
  Match(tok::l_paren);
  auto expr = ParseExpr();
  if (!expr)
    return {};
  Match(tok::r_paren);
  auto thenStmt = ParseStmt();
  if (!thenStmt)
    return {};
  if (Peek(tok::kw_else)) {
    Consume(tok::kw_else);
    auto elseStmt = ParseStmt();
    if (!elseStmt)
      return {};
    return Syntax::Stmt{Syntax::IfStmt(
        std::move(*expr), std::make_unique<Syntax::Stmt>(std::move(*thenStmt)),
        std::make_unique<Syntax::Stmt>(std::move(*elseStmt)))};
  } else {
    return Syntax::Stmt{
        Syntax::IfStmt(std::move(*expr),
                       std::make_unique<Syntax::Stmt>(std::move(*thenStmt)))};
  }
}

/// while ( expression ) statement
std::optional<Syntax::Stmt> Parser::ParseWhileStmt() {
  Consume(tok::kw_while);
  Match(tok::l_paren);
  auto expr = ParseExpr();
  if (!expr)
    return {};
  Match(tok::r_paren);
  auto stmt = ParseStmt();
  if (!stmt)
    return {};
  return Syntax::Stmt{Syntax::WhileStmt(
      std::move(*expr), std::make_unique<Syntax::Stmt>(std::move(*stmt)))};
}

/// do statement while ( expression ) ;
std::optional<Syntax::Stmt> Parser::ParseDoWhileStmt() {
  Consume(tok::kw_do);
  auto stmt = ParseStmt();
  if (!stmt)
    return {};
  Match(tok::kw_while);
  Match(tok::l_paren);
  auto expr = ParseExpr();
  if (!expr)
    return {};
  Match(tok::r_paren);
  Match(tok::semi);
  return Syntax::Stmt{Syntax::DoWhileStmt(
      std::make_unique<Syntax::Stmt>(std::move(*stmt)), std::move(*expr))};
}

/// for ( expression{opt} ; expression{opt} ; expression{opt} ) statement
/// for ( declaration expression{opt} ; expression{opt} ) statement
std::optional<Syntax::Stmt> Parser::ParseForStmt() {
  Consume(tok::kw_for);
  Match(tok::l_paren);
  auto blockItem = ParseBlockItem();
  if (!blockItem)
    return {};

  std::unique_ptr<Syntax::Expr> control;
  if (std::holds_alternative<Syntax::Declaration>(*blockItem) ||
      mTokCursor->getTokenKind() != tok::semi) {
    auto expr = ParseExpr();
    if (!expr)
      return {};
    Match(tok::semi);
    control = std::make_unique<Syntax::Expr>(std::move(*expr));
  } else {
    Consume(tok::semi);
  }

  std::unique_ptr<Syntax::Expr> post;
  if (Peek(tok::r_paren)) {
    Consume(tok::r_paren);
  } else {
    auto expr = ParseExpr();
    if (!expr)
      return {};
    Match(tok::r_paren);
    post = std::make_unique<Syntax::Expr>(std::move(*expr));
  }

  auto stmt = ParseStmt();
  if (!stmt)
    return {};
  if (auto declaration =
          std::get_if<Syntax::Declaration>(&*blockItem)) {
    return Syntax::Stmt(Syntax::ForStmt(
        std::make_unique<Syntax::Stmt>(std::move(*stmt)),
        std::make_unique<Syntax::Declaration>(std::move(*declaration)),
            std::move(control), std::move(post)));
  } else if (auto expr = std::get_if<Syntax::ExprStmt>(
                 &std::get<Syntax::Stmt>(*blockItem))) {
    return Syntax::Stmt(Syntax::ForStmt(
        std::make_unique<Syntax::Stmt>(std::move(*stmt)),
        expr->moveOptionalExpr(), std::move(control), std::move(post)));
  } else {
    LCC_UNREACHABLE;
  }
}

/// break;
std::optional<Syntax::Stmt> Parser::ParseBreakStmt() {
  Match(tok::kw_break);
  Match(tok::semi);
  return Syntax::Stmt{Syntax::BreakStmt()};
}

/// continue;
std::optional<Syntax::Stmt> Parser::ParseContinueStmt() {
  Match(tok::kw_continue);
  Match(tok::semi);
  return Syntax::Stmt{Syntax::ContinueStmt()};
}

/// return expr{opt};
std::optional<Syntax::Stmt> Parser::ParseReturnStmt() {
  Match(tok::kw_return);
  if (Peek(tok::semi)) {
    Consume(tok::semi);
    return Syntax::Stmt{Syntax::ReturnStmt()};
  }
  auto expr = ParseExpr();
  if (!expr)
    return {};
  Match(tok::semi);
  return Syntax::Stmt{
      Syntax::ReturnStmt(std::make_unique<Syntax::Expr>(std::move(*expr)))};
}

/// expr;
std::optional<Syntax::Stmt> Parser::ParseExprStmt() {
  if (Peek(tok::semi)) {
    Consume(tok::semi);
    return Syntax::Stmt(Syntax::ExprStmt());
  }
  auto expr = ParseExpr();
  if (!expr)
    return {};
  Match(tok::semi);
  return Syntax::Stmt(
      Syntax::ExprStmt(std::make_unique<Syntax::Expr>(std::move(*expr))));
}

/// switch ( expression ) statement
std::optional<Syntax::Stmt> Parser::ParseSwitchStmt() {
  Consume(tok::kw_switch);
  Match(tok::l_paren);
  auto expr = ParseExpr();
  if (!expr)
    return {};
  Match(tok::r_paren);
  auto stmt = ParseStmt();
  if (!stmt)
    return {};
  return Syntax::Stmt(Syntax::SwitchStmt(
      std::move(*expr), std::make_unique<Syntax::Stmt>(std::move(*stmt))));
}

/// case constantExpr: stmt
std::optional<Syntax::Stmt> Parser::ParseCaseStmt() {
  Consume(tok::kw_case);
  auto expr = ParseConditionalExpr();
  if (!expr)
    return {};
  Match(tok::colon);
  auto stmt = ParseStmt();
  if (!stmt)
    return {};
  return Syntax::Stmt(Syntax::CaseStmt(
      std::move(*expr), std::make_unique<Syntax::Stmt>(std::move(*stmt))));
}

/// default: stmt
std::optional<Syntax::Stmt> Parser::ParseDefaultStmt() {
  Consume(tok::kw_default);
  Match(tok::colon);
  auto stmt = ParseStmt();
  if (!stmt)
    return {};
  return Syntax::Stmt(
      Syntax::DefaultStmt(std::make_unique<Syntax::Stmt>(std::move(*stmt))));
}

/// goto identifier;
std::optional<Syntax::Stmt> Parser::ParseGotoStmt() {
  Consume(tok::kw_goto);
  Expect(tok::identifier);
  auto name = mTokCursor->getContent();
  Consume(tok::identifier);
  Match(tok::semi);
  return Syntax::Stmt(Syntax::GotoStmt(name));
}

/**
 expression:
    assignment-expression
    expression , assignment-expression
 */
std::optional<Syntax::Expr> Parser::ParseExpr() {
  std::vector<Syntax::AssignExpr> expressions;
  auto assignment = ParseAssignExpr();
  if (!assignment) {
    return {};
  }
  expressions.push_back(std::move(*assignment));

  while (Peek(tok::comma)) {
    Consume(tok::comma);
    auto curr = mTokCursor;
    assignment = ParseAssignExpr();
    if (!assignment) {
      LOGE(curr->getLine(), curr->getColumn(), "parse assign expr error");
      break;
    }
    expressions.push_back(std::move(*assignment));
  }
  return Syntax::Expr(std::move(expressions));
}


/**
 * assignment-expression:
 *      conditional-expression
 *      unary-expression assignment-operator assignment-expression
 *
 * assignment-operator: one of
 *      =  *=  /=  %=  +=  -=  <<=  >>=  &=  ^=  |=
 *
 * Instead we are doing something similar to clang here though:
 * We'll be using the grammar of the form:
 *
 * assignment-expression:
 *      conditional-expression
 *      conditional-expression assignment-operator assignment-expression
 */
std::optional<Syntax::AssignExpr> Parser::ParseAssignExpr() {
  auto start = mTokCursor;
  auto result = ParseConditionalExpr();
  if (!result) {
    LOGE(start->getLine(), start->getColumn(), "parse conditional expr error");
  }
  std::vector<std::pair<Syntax::AssignExpr::AssignmentOperator, Syntax::ConditionalExpr>> list;
  while (IsAssignment(mTokCursor->getTokenKind())) {
    auto token = mTokCursor;
    ConsumeAny();
    auto assignmentOperator = [token]() -> Syntax::AssignExpr::AssignmentOperator
    {
      switch (token->getTokenKind()){
      case tok::equal:
        return Syntax::AssignExpr::AssignmentOperator::Assign;
      case tok::plus_equal:
        return Syntax::AssignExpr::AssignmentOperator::PlusAssign;
      case tok::minus_equal:
        return Syntax::AssignExpr::AssignmentOperator::MinusAssign;
      case tok::slash_equal:
        return Syntax::AssignExpr::AssignmentOperator::DivideAssign;
      case tok::star_equal:
        return Syntax::AssignExpr::AssignmentOperator::MultiplyAssign;
      case tok::percent_equal:
        return Syntax::AssignExpr::AssignmentOperator::ModuloAssign;
      case tok::less_less_equal:
        return Syntax::AssignExpr::AssignmentOperator::LeftShiftAssign;
      case tok::greater_greater_equal:
        return Syntax::AssignExpr::AssignmentOperator::RightShiftAssign;
      case tok::amp_equal:
        return Syntax::AssignExpr::AssignmentOperator::BitAndAssign;
      case tok::pipe_equal:
        return Syntax::AssignExpr::AssignmentOperator::BitOrAssign;
      case tok::caret_equal:
        return Syntax::AssignExpr::AssignmentOperator::BitXorAssign;
      default: return Syntax::AssignExpr::AssignmentOperator::Assign;
      }
    }();
    start = mTokCursor;
    auto conditional = ParseConditionalExpr();
    if (!conditional) {
        LOGE(start->getLine(), start->getColumn(), "parse conditional expr error");
    }
    list.push_back({assignmentOperator, std::move(*conditional)});
  }
  return Syntax::AssignExpr(std::move(*result), std::move(list));
}

/**
 * conditional-expression:
 *      logical-OR-expression
 *      logical-OR-expression ? expression : conditional-expression
 */
std::optional<Syntax::ConditionalExpr> Parser::ParseConditionalExpr() {
  auto start = mTokCursor;
  auto logOrExpr = ParseLogOrExpr();
  if (!logOrExpr) {
    LOGE(start->getLine(), start->getColumn(), "parse log or expr error");
    return {};
  }
  if (Peek(tok::question)) {
    Consume(tok::question);
    start = mTokCursor;
    auto expr = ParseExpr();
    if (!expr) {
      LOGE(start->getLine(), start->getColumn(), "parse expr error");
      return {};
    }
    Match(tok::colon);
    start = mTokCursor;
    auto optionalConditional = ParseConditionalExpr();
    if (!optionalConditional) {
      LOGE(start->getLine(), start->getColumn(), "parse conditional error");
      return {};
    }
    return Syntax::ConditionalExpr(
        std::move(*logOrExpr), std::make_unique<Syntax::Expr>(std::move(*expr)),
        std::make_unique<Syntax::ConditionalExpr>(
            std::move(*optionalConditional)));
  }

  return Syntax::ConditionalExpr(std::move(*logOrExpr));
}

/**
 * logical-OR-expression:
 *      logical-AND-expression
 *      logical-OR-expression || logical-AND-expression
 */
std::optional<Syntax::LogOrExpr> Parser::ParseLogOrExpr() {
  auto expr = ParseLogAndExpr();
  if (!expr) {
    return {};
  }

  std::vector<Syntax::LogAndExpr> logAndExprArr;
  while (Peek(tok::pipe_pipe)) {
    Consume(tok::pipe_pipe);
    auto logAndExpr = ParseLogAndExpr();
    if (!logAndExpr) {
      break;
    }
    logAndExprArr.push_back(std::move(*logAndExpr));
  }
  return Syntax::LogOrExpr(std::move(*expr), std::move(logAndExprArr));
}

/**
 * logical-AND-expression:
 *      inclusive-OR-expression
 *      logical-AND-expression && inclusive-OR-expression
 */
std::optional<Syntax::LogAndExpr> Parser::ParseLogAndExpr() {
  auto expr = ParseBitOrExpr();
  std::vector<Syntax::BitOrExpr> bitOrExprArr;
  while (Peek(tok::amp_amp)) {
    Consume(tok::amp_amp);
    auto bitOrExpr = ParseBitOrExpr();
    if (!bitOrExpr) {
      return {};
    }
    bitOrExprArr.push_back(std::move(*bitOrExpr));
  }
  return Syntax::LogAndExpr(std::move(*expr), std::move(bitOrExprArr));
}

/**
 * inclusive-OR-expression:
 *      exclusive-OR-expression
 *      inclusive-OR-expression | exclusive-OR-expression
 */
std::optional<Syntax::BitOrExpr> Parser::ParseBitOrExpr() {
  auto expr = ParseBitXorExpr();
  if (!expr) {
    return {};
  }
  std::vector<Syntax::BitXorExpr> bitXorExprArr;
  while (Peek(tok::pipe)) {
    Consume(tok::pipe);
    auto newXor = ParseBitXorExpr();
    if (!newXor) {
      break;
    }
    bitXorExprArr.push_back(std::move(*newXor));
  }
  return Syntax::BitOrExpr(std::move(*expr), std::move(bitXorExprArr));
}

std::optional<Syntax::BitXorExpr> Parser::ParseBitXorExpr() {
  auto expr = ParseBitAndExpr();
  if (!expr) {
    return {};
  }
  std::vector<Syntax::BitAndExpr> bitAndExprArr;
  while (Peek(tok::caret)) {
    Consume(tok::caret);
    auto newAnd = ParseBitAndExpr();
    if (!newAnd) {
      break;
    }
    bitAndExprArr.push_back(std::move(*newAnd));
  }

  return Syntax::BitXorExpr(std::move(*expr), std::move(bitAndExprArr));
}

/**
 * AND-expression:
 *      equality-expression
 *      AND-expression & equality-expression
 */
std::optional<Syntax::BitAndExpr> Parser::ParseBitAndExpr() {
  auto expr = ParseEqualExpr();
  if (!expr) {
    return {};
  }
  std::vector<Syntax::EqualExpr> equalExprArr;
  while (Peek(tok::amp)) {
    Consume(tok::amp);
    auto newEqual = ParseEqualExpr();
    if (!newEqual) {
      break;
    }
    equalExprArr.push_back(std::move(*newEqual));
  }

  return Syntax::BitAndExpr(std::move(*expr), std::move(equalExprArr));
}

/**
 * equality-expression:
 *      relational-expression
 *      equality-expression == relational-expression
 *      equality-expression != relational-expression
 */
std::optional<Syntax::EqualExpr> Parser::ParseEqualExpr() {
  auto result = ParseRelationalExpr();
  if (!result) {
    return {};
  }
  std::vector<std::pair<Syntax::EqualExpr::BinaryOperator, Syntax::RelationalExpr>>
      relationalExpressions;
  while (Peek(tok::equal_equal) || Peek(tok::exclaim_equal)) {
    tok::TokenKind tokenType = mTokCursor->getTokenKind();
    Syntax::EqualExpr::BinaryOperator binaryOperator;
    if (tokenType == tok::equal_equal) {
      binaryOperator = Syntax::EqualExpr::BinaryOperator::Equal;
    }else {
      binaryOperator = Syntax::EqualExpr::BinaryOperator::NotEqual;
    }
    ConsumeAny();
    auto newRelational = ParseRelationalExpr();
    if (!newRelational) {
      break;
    }
    relationalExpressions.emplace_back(binaryOperator, std::move(*newRelational));
  }

  return Syntax::EqualExpr(std::move(*result),
                           std::move(relationalExpressions));
}

/**
 * relational-expression:
 *      shift-expression
 *      relational-expression < shift-expression
 *      relational-expression > shift-expression
 *      relational-expression <= shift-expression
 *      relational-expression >= shift-expression
 */
std::optional<Syntax::RelationalExpr> Parser::ParseRelationalExpr() {
  auto result = ParseShiftExpr();
  if (!result) {
    return {};
  }
  std::vector<std::pair<Syntax::RelationalExpr::BinaryOperator, Syntax::ShiftExpr>> relationalExprArr;
  while (Peek(tok::less) || Peek(tok::less_equal) ||
         Peek(tok::greater) || Peek(tok::greater_equal)) {
    tok::TokenKind tokenType = mTokCursor->getTokenKind();
    auto binaryOperator = [tokenType]()->Syntax::RelationalExpr::BinaryOperator {
      switch (tokenType) {
        case tok::less: return Syntax::RelationalExpr::BinaryOperator::LessThan;
        case tok::less_equal: return Syntax::RelationalExpr::BinaryOperator::LessThanOrEqual;
        case tok::greater: return Syntax::RelationalExpr::BinaryOperator::GreaterThan;
        case tok::greater_equal: return Syntax::RelationalExpr::BinaryOperator::GreaterThanOrEqual;
        default:
          LCC_UNREACHABLE;
      }
    }();
    ConsumeAny();
    auto newShift = ParseShiftExpr();
    if (!newShift) {
      break;
    }

    relationalExprArr.emplace_back(binaryOperator, std::move(*newShift));
  }

  return Syntax::RelationalExpr(std::move(*result),
                                std::move(relationalExprArr));
}


/**
 * shift-expression:
 *      additive-expression
 *      shift-expression << additive-expression
 *      shift-expression >> additive-expression
 */
std::optional<Syntax::ShiftExpr> Parser::ParseShiftExpr() {
  auto result = ParseAdditiveExpr();
  if (!result) {
    return {};
  }

  std::vector<std::pair<Syntax::ShiftExpr::BinaryOperator, Syntax::AdditiveExpr>> additiveExprArr;
  while (Peek(tok::less_less) || Peek(tok::greater_greater)) {
    tok::TokenKind tokenType = mTokCursor->getTokenKind();
    Syntax::ShiftExpr::BinaryOperator binaryOperator;
    if (tokenType == tok::less_less) {
      binaryOperator = Syntax::ShiftExpr::BinaryOperator::Left;
    }else {
      binaryOperator = Syntax::ShiftExpr::BinaryOperator::Right;
    }
    ConsumeAny();
    auto newAdd = ParseAdditiveExpr();
    if (!newAdd) {
      break;
    }
    additiveExprArr.emplace_back(binaryOperator, std::move(*newAdd));
  }
  return Syntax::ShiftExpr(std::move(*result), std::move(additiveExprArr));
}


/**
 * additive-expression:
 * multiplicative-expression
 * additive-expression + multiplicative-expression
 * additive-expression - multiplicative-expression
 */
std::optional<Syntax::AdditiveExpr> Parser::ParseAdditiveExpr() {
  auto result = ParseMultiExpr();
  if (!result) {
    return {};
  }

  std::vector<std::pair<Syntax::AdditiveExpr::BinaryOperator, Syntax::MultiExpr>> multiExprArr;
  while (Peek(tok::plus) || Peek(tok::minus)) {
    tok::TokenKind tokenType = mTokCursor->getTokenKind();
    Syntax::AdditiveExpr::BinaryOperator binaryOperator;
    if (tokenType == tok::plus) {
      binaryOperator = Syntax::AdditiveExpr::BinaryOperator::Plus;
    }else {
      binaryOperator = Syntax::AdditiveExpr::BinaryOperator::Minus;
    }
    ConsumeAny();
    auto newMul = ParseMultiExpr();
    if (!newMul) {
      break;
    }
    multiExprArr.emplace_back(binaryOperator, std::move(*newMul));
  }

  return Syntax::AdditiveExpr(std::move(*result), std::move(multiExprArr));
}

/**
 * multiplicative-expression:
 *  cast-expression
 *  multiplicative-expression * cast-expression
 *  multiplicative-expression / cast-expression
 *  multiplicative-expression % cast-expression
 */
std::optional<Syntax::MultiExpr> Parser::ParseMultiExpr() {
  auto start = mTokCursor;
  auto result = ParseCastExpr();
  if (!result) {
    LOGE(start->getLine(), start->getColumn(), "parse case expr error");
    return {};
  }
  std::vector<std::pair<Syntax::MultiExpr::BinaryOperator, Syntax::CastExpr>> castExprArr;
  while (Peek(tok::star) || Peek(tok::slash) || Peek(tok::percent)) {
    tok::TokenKind tokenType = mTokCursor->getTokenKind();
    auto binaryOperator = [tokenType]()->Syntax::MultiExpr::BinaryOperator {
      switch (tokenType) {
      case tok::star: return Syntax::MultiExpr::BinaryOperator::Multiply;
      case tok::slash: return Syntax::MultiExpr::BinaryOperator::Divide;
      case tok::percent: return Syntax::MultiExpr::BinaryOperator::Modulo;
      default:
        LCC_UNREACHABLE;
      }
    }();
    ConsumeAny();
    auto newCast = ParseCastExpr();
    if (!newCast) {
      break;
    }
    castExprArr.emplace_back(binaryOperator, std::move(*newCast));
  }

  return Syntax::MultiExpr(std::move(*result), std::move(castExprArr));
}

/**
 * type-name:
 *  specifier-qualifier-list abstract-declarator{opt}
 */
std::optional<Syntax::TypeName> Parser::ParseTypeName() {
  auto specifierQualifiers = ParseSpecifierQualifierList();
  if (specifierQualifiers.isEmpty()) {
    LOGE(mTokCursor->getLine(), mTokCursor->getColumn(),
        "Expected at least one specifier qualifier at beginning of typename");
    return {};
  }

  if (IsFirstInAbstractDeclarator()) {
    auto abstractDec = ParseAbstractDeclarator();
    return Syntax::TypeName(
        std::move(specifierQualifiers),
        std::make_unique<Syntax::AbstractDeclarator>(std::move(*abstractDec)));
  }
  return Syntax::TypeName(std::move(specifierQualifiers), nullptr);
}

/**
 * cast-expression:
 *      unary-expression
 *      ( type-name ) cast-expression
 *
 * (unsigned char)(h ? h->height + 1 : 0);
 */
std::optional<Syntax::CastExpr> Parser::ParseCastExpr() {
  auto start = mTokCursor;

  // cast-expression: unary-expression
  if (!Peek(tok::l_paren)) {
    auto unary = ParseUnaryExpr();
    if (!unary) {
      LOGE(start->getLine(), start->getColumn(), "parse unary expr error");
      return {};
    }
    return Syntax::CastExpr(std::move(*unary));
  }

  Match(tok::l_paren);

  if (!IsFirstInTypeName()) {
    // cast-expression: unary-expression
    mTokCursor = start;
    auto unary = ParseUnaryExpr();
    if (!unary) {
      LOGE(start->getLine(), start->getColumn(), "parse unary expr error");
      return {};
    }
    return Syntax::CastExpr(std::move(*unary));
  }else {
    // cast-expression: ( type-name ) cast-expression
    auto typeName = ParseTypeName();
    if (!typeName) {
      LOGE(start->getLine(), start->getColumn(), "parse type name error");
    }
    Match(tok::r_paren);
    start = mTokCursor;
    auto cast = ParseCastExpr();
    if (!cast) {
      LOGE(start->getLine(), start->getColumn(), "parse cast expr error");
    }
    return Syntax::CastExpr(
        std::pair{std::move(*typeName),
                  std::make_unique<Syntax::CastExpr>(std::move(*cast))});
  }
}

/**
 * unary-expression:
 *  postfix-expression
 *  ++ unary-expression
 *  -- unary-expression
 *  unary-operator cast-expression
 *  sizeof unary-expression
 *  sizeof ( type-name )
 *
 *  unary-operator: one of
 *      & * + - ~ !
 */
std::optional<Syntax::UnaryExpr> Parser::ParseUnaryExpr() {
  if (Peek(tok::kw_sizeof)) {
    Consume(tok::kw_sizeof);
    if (Peek(tok::l_paren)) {
      Consume(tok::l_paren);
      auto type = ParseTypeName();
      if (!type) {
        return {};
      }
      Expect(tok::r_paren);
      ConsumeAny();
      return Syntax::UnaryExpr(Syntax::UnaryExprSizeOf(
          std::make_unique<Syntax::TypeName>(std::move(*type))));
    } else {
      auto unary = ParseUnaryExpr();
      if (!unary) {
        return {};
      }
      return Syntax::UnaryExpr(Syntax::UnaryExprSizeOf(
          std::make_unique<Syntax::UnaryExpr>(std::move(*unary))));
    }
  } else if (IsUnaryOp(mTokCursor->getTokenKind())) {
    tok::TokenKind tokenType = mTokCursor->getTokenKind();
    auto unaryOperator = [tokenType]()->Syntax::UnaryExprUnaryOperator::UnaryOperator{
      switch (tokenType) {
      case tok::amp: return Syntax::UnaryExprUnaryOperator::UnaryOperator::Ampersand;
      case tok::star: return Syntax::UnaryExprUnaryOperator::UnaryOperator::Asterisk;
      case tok::plus: return Syntax::UnaryExprUnaryOperator::UnaryOperator::Plus;
      case tok::minus: return Syntax::UnaryExprUnaryOperator::UnaryOperator::Minus;
      case tok::tilde: return Syntax::UnaryExprUnaryOperator::UnaryOperator::BitNot;
      case tok::exclaim: return Syntax::UnaryExprUnaryOperator::UnaryOperator::LogicalNot;
      case tok::plus_plus: return Syntax::UnaryExprUnaryOperator::UnaryOperator::Increment;
      case tok::minus_minus: return Syntax::UnaryExprUnaryOperator::UnaryOperator::Decrement;
      default:
        LCC_UNREACHABLE;
      }
    }();
    ConsumeAny();
    auto castExpr = ParseCastExpr();
    if (!castExpr) {
      return {};
    }
    return Syntax::UnaryExpr(Syntax::UnaryExprUnaryOperator(
        unaryOperator, std::make_unique<Syntax::CastExpr>(std::move(*castExpr))));
  } else {
    auto postFix = ParsePostFixExpr();
    if (!postFix) {
      return {};
    }
    return Syntax::UnaryExpr(Syntax::UnaryExprPostFixExpr(std::move(*postFix)));
  }
}

/**
 * postfix-expression:
 *    primary-expression
 *    postfix-expression [ expression ]
 *    postfix-expression ( argument-expression-list{opt} )
 *    postfix-expression . identifier
 *    postfix-expression -> identifier
 *    postfix-expression ++
 *    postfix-expression --
 *    ( type-name ) { initializer-list }
 *    ( type-name ) { initializer-list , }
 */

void Parser::ParsePostFixExprSuffix(std::unique_ptr<Syntax::PostFixExpr>& current) {
  while (IsPostFixExpr(mTokCursor->getTokenKind())) {
    auto tokType = mTokCursor->getTokenKind();
    if (tokType == tok::l_paren) {
      Consume(tok::l_paren);
      std::vector<Syntax::AssignExpr> params;
      if (!Peek(tok::r_paren)) {
        auto start = mTokCursor;
        auto assignment = ParseAssignExpr();
        if (!assignment) {
          LOGE(start->getLine(), start->getColumn(), "parse assign expr error");
        }
        params.push_back(std::move(*assignment));
      }
      while (!Peek(tok::r_paren)) {
        Match(tok::comma);
        auto start = mTokCursor;
        auto assignment = ParseAssignExpr();
        if (!assignment) {
          LOGE(start->getLine(), start->getColumn(), "parse assign expr error");
        }
        params.push_back(std::move(*assignment));
      }
      Consume(tok::r_paren);
      if (current) {
        current = std::make_unique<Syntax::PostFixExpr>(
            Syntax::PostFixExprFuncCall(std::move(current), std::move(params)));
      }
    } else if (tokType == tok::l_square) {
      Consume(tok::l_square);
      auto start = mTokCursor;
      auto expr = ParseExpr();
      if (!expr) {
        LOGE(start->getLine(), start->getColumn(), "parse expr error");
      }
      Match(tok::r_square);
      if (current) {
        current = std::make_unique<Syntax::PostFixExpr>(
            Syntax::PostFixExprSubscript(std::move(current), std::move(*expr)));
      }
    } else if (tokType == tok::plus_plus) {
      Consume(tok::plus_plus);
      current = std::make_unique<Syntax::PostFixExpr>(
          Syntax::PostFixExprIncrement(std::move(current)));
    } else if (tokType == tok::minus_minus) {
      Consume(tok::minus_minus);
      current = std::make_unique<Syntax::PostFixExpr>(
          Syntax::PostFixExprDecrement(std::move(current)));
    } else if (tokType == tok::period) {
      Consume(tok::period);
      Expect(tok::identifier);
      std::string identifier = mTokCursor->getContent();
      Match(tok::identifier);
      current = std::make_unique<Syntax::PostFixExpr>(
          Syntax::PostFixExprDot(std::move(current), identifier));
    } else if (tokType == tok::arrow) {
      Consume(tok::arrow);
      Expect(tok::identifier);
      auto name = mTokCursor->getContent();
      Match(tok::identifier);
      current = std::make_unique<Syntax::PostFixExpr>(
          Syntax::PostFixExprArrow(std::move(current), name));
    }
  }
}

/**
 * postfix-expression:
 *    primary-expression
 *    postfix-expression [ expression ]
 *    postfix-expression ( argument-expression-list{opt} )
 *    postfix-expression . identifier
 *    postfix-expression -> identifier
 *    postfix-expression ++
 *    postfix-expression --
 *    ( type-name ) { initializer-list }
 *    ( type-name ) { initializer-list , }
 */
std::optional<Syntax::PostFixExpr> Parser::ParsePostFixExpr() {
  std::unique_ptr<Syntax::PostFixExpr> current;
  auto start = mTokCursor;
  std::optional<Syntax::PrimaryExpr> newPrimary;
  if (Peek(tok::identifier)) {
    std::string name = mTokCursor->getContent();
    Consume(tok::identifier);
    newPrimary = Syntax::PrimaryExpr(Syntax::PrimaryExprIdent(name));
  }else if (Peek(tok::char_constant) || Peek(tok::numeric_constant) || Peek(tok::string_literal)) {
    auto value = std::visit([](auto &&value)->typename Syntax::PrimaryExprConstant::Variant {
      using T = std::decay_t<decltype(value)>;
      if constexpr (std::is_constructible_v<typename Syntax::PrimaryExprConstant::Variant, T>) {
        return std::forward<decltype(value)>(value);
      }else {
        LCC_UNREACHABLE;
      }
    }, mTokCursor->getValue());
    ConsumeAny();
    newPrimary = Syntax::PrimaryExpr(Syntax::PrimaryExprConstant(value));
  }else if (Peek(tok::l_paren)) {
    start = mTokCursor;
    Consume(tok::l_paren);
    if (IsFirstInTypeName()) {
      auto type = ParseTypeName();
      if (!type) {
        LOGE(start->getLine(), start->getColumn(), "parse type error");
      }
      Match(tok::r_paren);
      Consume(tok::l_brace);
      start = mTokCursor;
      auto initializer = ParseInitializerList();
      if (!initializer) {
        LOGE(start->getLine(), start->getColumn(), "parse initializer list error");
      }
      if (Peek(tok::comma)) {
        Consume(tok::comma);
      }
      Match(tok::r_brace);
//      std::unique_ptr<Syntax::TypeName> typeName = std::make_unique<Syntax::TypeName>(std::move(*type));
//      std::unique_ptr<Syntax::Initializer> init = std::make_unique<Syntax::Initializer>(std::move(*initializer));
//      Syntax::PostFixExprTypeInitializer(std::move(typeName), nullptr);
      current = std::make_unique<Syntax::PostFixExpr>(
          Syntax::PostFixExprTypeInitializer(std::move(*type), std::move(*initializer)));
    }else {
      auto expr = ParseExpr();
      if (!expr) {
        LOGE(start->getLine(), start->getColumn(), "parse expr error");
      }
      Match(tok::r_paren);
      newPrimary = Syntax::PrimaryExpr(Syntax::PrimaryExprParent(std::move(*expr)));
    }
  }else {
    LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "expect primary expr or ( type-name )");
  }

  if (newPrimary) {
    current = std::make_unique<Syntax::PostFixExpr>(
        Syntax::PostFixExprPrimaryExpr(std::move(*newPrimary)));
  }
  ParsePostFixExprSuffix(current);
  if (!current) {
    LOGE(mTokCursor->getLine(), mTokCursor->getColumn(), "parse postfix expr error");
  }
  return std::move(*current);
}

std::optional<Syntax::PrimaryExpr> Parser::ParsePrimaryExpr() {
  if (Peek(tok::identifier)) {
    std::string identifier = mTokCursor->getContent();
    Consume(tok::identifier);
    return Syntax::PrimaryExpr(Syntax::PrimaryExprIdent(identifier));
  } else if (Peek(tok::char_constant) || Peek(tok::numeric_constant) ||
             Peek(tok::string_literal)) {
    auto cur = mTokCursor;
    ConsumeAny();
    return Syntax::PrimaryExpr(Syntax::PrimaryExprConstant(std::visit(
        [](auto &&value) -> typename Syntax::PrimaryExprConstant::Variant {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_constructible_v<
                            typename Syntax::PrimaryExprConstant::Variant, T>) {
            return {std::forward<decltype(value)>(value)};
          } else {
            throw std::runtime_error(
                "Can't convert type of variant to constant expression");
          }
        },
        cur->getValue())));
  } else {
    Expect(tok::l_paren);
    Consume(tok::l_paren);

    auto expr = ParseExpr();
    if (!expr) {
      return {};
    }
    Match(tok::r_paren);
    return Syntax::PrimaryExpr(Syntax::PrimaryExprParent(std::move(*expr)));
  }
}

bool Parser::IsAssignment(tok::TokenKind type) {
  return type == tok::equal || type == tok::plus_equal ||
         type == tok::minus_equal || type == tok::star_equal ||
         type == tok::slash_equal || type == tok::percent_equal ||
         type == tok::less_less_equal || type == tok::greater_greater_equal ||
         type == tok::amp_equal || type == tok::pipe_equal ||
         type == tok::caret_equal;
}

bool Parser::Match(tok::TokenKind tokenType) {
  if (mTokCursor->getTokenKind() == tokenType) {
    ++mTokCursor;
    return true;
  }
  LOGE(mTokCursor->getLine(), mTokCursor->getColumn(),
       "Should Match tok: " + std::string(tok::getTokenName(tokenType))
           + ", but get: " +  std::string(tok::getTokenName(mTokCursor->getTokenKind())));
  return false;
}

bool Parser::Expect(tok::TokenKind tokenType) {
  if (mTokCursor->getTokenKind() == tokenType)
    return true;
  LOGE(mTokCursor->getLine(), mTokCursor->getColumn(),
       "Expect tok: " + std::string(tok::getTokenName(tokenType))
           + ", but get: " + std::string(tok::getTokenName(mTokCursor->getTokenKind())));
  return false;
}

bool Parser::Consume(tok::TokenKind tokenType) {
  if (mTokCursor->getTokenKind() == tokenType) {
    ++mTokCursor;
    return true;
  } else {
    LOGE(mTokCursor->getLine(), mTokCursor->getColumn(),
         "Should Consume tok: " + std::string(tok::getTokenName(tokenType))
             + ", but get: " +  std::string(tok::getTokenName(mTokCursor->getTokenKind())));
    return false;
  }
}
bool Parser::ConsumeAny() {
  ++mTokCursor;
  return true;
}
bool Parser::Peek(tok::TokenKind tokenType) {
  if (mTokCursor >= mTokEnd) {
    auto lastToken = mTokCursor - 1;
    LOGE(lastToken->getLine(), lastToken->getColumn(), "peek token but consume end");
  }
  return mTokCursor->getTokenKind() == tokenType;
}

bool Parser::PeekN(int n, tok::TokenKind tokenType) {
  if (n == 0) {
    return Peek(tokenType);
  }
  if (mTokCursor + n >= mTokEnd) {
    auto lastToken = mTokCursor;
    LOGE(lastToken->getLine(), lastToken->getColumn(), "peek N token but consume end");
  }
  return (mTokCursor+n)->getTokenKind() == tokenType;
}

bool Parser::IsUnaryOp(tok::TokenKind tokenType) {
  if (tokenType == tok::amp || tokenType == tok::star ||
      tokenType == tok::plus || tokenType == tok::minus ||
      tokenType == tok::tilde || tokenType == tok::exclaim ||
      tokenType == tok::plus_plus || tokenType == tok::minus_minus) {
    return true;
  }
  return false;
}

bool Parser::IsPostFixExpr(tok::TokenKind tokenType) {
  return (tokenType == tok::l_paren || tokenType == tok::l_square ||
          tokenType == tok::period || tokenType == tok::arrow ||
          tokenType == tok::plus_plus || tokenType == tok::minus_minus ||
          tokenType == tok::identifier || tokenType == tok::char_constant ||
          tokenType == tok::numeric_constant);
}

void Parser::Scope::addTypedef(const std::string& name) {
  mCurrentScope.back().emplace(name, Symbol{name, true});
}

bool Parser::Scope::isTypedef(const std::string& name) const {
  for (auto &iter : mCurrentScope) {
    if (auto result = iter.find(name); result != iter.end() && result->second.isTypedef) {
      return true;
    }
  }
  return false;
}

bool Parser::Scope::isTypedefInScope(const std::string& name) const {
  for (auto iter = mCurrentScope.rbegin(); iter != mCurrentScope.rend();
       iter++) {
    if (auto result = iter->find(name); result != iter->end()) {
      return result->second.isTypedef;
    }
  }
  return false;
}

void Parser::Scope::addToScope(const std::string& name) {
  mCurrentScope.back().emplace(name, Symbol{name, false});
}

void Parser::Scope::pushScope() {
  mCurrentScope.emplace_back();
}

void Parser::Scope::popScope() {
  mCurrentScope.pop_back();
}

bool Parser::IsFirstInExternalDeclaration() const {
  return IsFirstInDeclaration() || IsFirstInFunctionDefinition();
}
bool Parser::IsFirstInFunctionDefinition() const {
  return IsFirstInDeclarationSpecifier();
}
bool Parser::IsFirstInDeclaration() const {
  return IsFirstInDeclarationSpecifier();
}
bool Parser::IsFirstInDeclarationSpecifier() const {
  switch (mTokCursor->getTokenKind()) {
  case tok::kw_typedef:
  case tok::kw_extern:
  case tok::kw_static:
  case tok::kw_auto:
  case tok::kw_register:
  case tok::kw_void:
  case tok::kw_char:
  case tok::kw_short:
  case tok::kw_int:
  case tok::kw_long:
  case tok::kw_float:
  case tok::kw__Bool:
  case tok::kw_double:
  case tok::kw_signed:
  case tok::kw_unsigned:
  case tok::kw_enum:
  case tok::kw_struct:
  case tok::kw_union:
  case tok::kw_const:
  case tok::kw_restrict:
  case tok::kw_volatile:
  case tok::kw_inline: return true;
  case tok::identifier:
    return mScope.isTypedefInScope(std::get<std::string>(mTokCursor->getValue()));
  default:
    return false;
  }
}
bool Parser::IsFirstInSpecifierQualifier() const {
  switch (mTokCursor->getTokenKind()) {
  case tok::kw_void:
  case tok::kw_char:
  case tok::kw_short:
  case tok::kw_int:
  case tok::kw_long:
  case tok::kw_float:
  case tok::kw__Bool:
  case tok::kw_double:
  case tok::kw_signed:
  case tok::kw_unsigned:
  case tok::kw_enum:
  case tok::kw_struct:
  case tok::kw_union:
  case tok::kw_const:
  case tok::kw_restrict:
  case tok::kw_volatile:
  case tok::kw_inline: return true;
  case tok::identifier:
    return mScope.isTypedefInScope(std::get<std::string>(mTokCursor->getValue()));
  default:
    return false;
  }
}
bool Parser::IsFirstInDeclarator() const {
  return IsFirstInPointer() || IsFirstInDirectDeclarator();
}
bool Parser::IsFirstInDirectDeclarator() const {
  return (mFirstDirectDeclaratorSet.find(mTokCursor->getTokenKind()) != mFirstDirectDeclaratorSet.end());
}
bool Parser::IsFirstInParameterTypeList() const {
  return IsFirstInParameterList();
}
bool Parser::IsFirstInAbstractDeclarator() const {
  return IsFirstInPointer() || IsFirstInDirectAbstractDeclarator();
}
bool Parser::IsFirstInDirectAbstractDeclarator() const {
  return (mFirstDirectAbstractDeclaratorSet.find(mTokCursor->getTokenKind()) != mFirstDirectAbstractDeclaratorSet.end());
}
bool Parser::IsFirstInParameterList() const {
  return IsFirstInDeclarationSpecifier();
}
bool Parser::IsFirstInPointer() const {
  return mTokCursor->getTokenKind() == tok::star;
}
bool Parser::IsFirstInBlockItem() const {
  return IsFirstInDeclaration() || IsFirstInStatement();
}
bool Parser::IsFirstInInitializer() const {
  return (mFirstInitializerSet.find(mTokCursor->getTokenKind()) != mFirstInitializerSet.end());
}
bool Parser::IsFirstInInitializerList() const {
  return (mFirstInitializerListSet.find(mTokCursor->getTokenKind()) != mFirstInitializerListSet.end());
}
bool Parser::IsFirstInStatement() const {
  return (mFirstStatementSet.find(mTokCursor->getTokenKind()) != mFirstStatementSet.end())
      || IsFirstInExpr();
}
bool Parser::IsFirstInExpr() const {
  return IsFirstInAssignmentExpr();
}
bool Parser::IsFirstInAssignmentExpr() const {
  return IsFirstInConditionalExpr();
}
bool Parser::IsFirstInConditionalExpr() const {
  return IsFirstInLogicalOrExpr();
}
bool Parser::IsFirstInLogicalOrExpr() const {
  return IsFirstInLogicalAndExpr();
}
bool Parser::IsFirstInLogicalAndExpr() const {
  return IsFirstInBitOrExpr();
}
bool Parser::IsFirstInBitOrExpr() const {
  return IsFirstInBitXorExpr();
}
bool Parser::IsFirstInBitXorExpr() const {
  return IsFirstInBitAndExpr();
}
bool Parser::IsFirstInBitAndExpr() const {
  return IsFirstInEqualExpr();
}
bool Parser::IsFirstInEqualExpr() const {
  return IsFirstRelationalExpr();
}
bool Parser::IsFirstRelationalExpr() const {
  return IsFirstInShiftExpr();
}
bool Parser::IsFirstInShiftExpr() const {
  return IsFirstInAdditiveExpr();
}
bool Parser::IsFirstInAdditiveExpr() const {
  return IsFirstInMultiExpr();
}
bool Parser::IsFirstInMultiExpr() const {
 return IsFirstInCastExpr();
}
bool Parser::IsFirstInTypeName() const {
  return IsFirstInSpecifierQualifier();
}
bool Parser::IsFirstInCastExpr() const {
  return mTokCursor->getTokenKind() == tok::l_paren ||
  IsFirstInUnaryExpr();
}
bool Parser::IsFirstInUnaryExpr() const {
  return IsFirstInPostFixExpr() ||
         mTokCursor->getTokenKind() == tok::plus_plus ||
         mTokCursor->getTokenKind() == tok::minus_minus ||
         mTokCursor->getTokenKind() == tok::amp ||
         mTokCursor->getTokenKind() == tok::star ||
         mTokCursor->getTokenKind() == tok::plus ||
         mTokCursor->getTokenKind() == tok::minus ||
         mTokCursor->getTokenKind() == tok::tilde ||
         mTokCursor->getTokenKind() == tok::exclaim ||
         mTokCursor->getTokenKind() == tok::kw_sizeof;
}
bool Parser::IsFirstInPostFixExpr() const {
  return mTokCursor->getTokenKind() == tok::l_paren || IsFirstInPrimaryExpr();
}
bool Parser::IsFirstInPrimaryExpr() const {
  return mTokCursor->getTokenKind() == tok::l_paren ||
  mTokCursor->getTokenKind() == tok::identifier ||
  mTokCursor->getTokenKind() == tok::char_constant ||
  mTokCursor->getTokenKind() == tok::numeric_constant ||
  mTokCursor->getTokenKind() == tok::string_literal;
}
} // namespace lcc