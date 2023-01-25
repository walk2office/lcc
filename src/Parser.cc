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
#include "Semantics.h"
#include <algorithm>
#include <stack>

namespace lcc {

namespace {
template <typename G> struct Y {
  template <typename... X> decltype(auto) operator()(X &&...x) const & {
    return g(*this, std::forward<X>(x)...);
  }
  G g;
};
template <typename G> Y(G) -> Y<G>;
} // namespace

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

Parser::Parser(const CTokenObject &sourceObject)
    : mSourceInterface(sourceObject), mTokCursor(sourceObject.data().cbegin()),
      mTokEnd(sourceObject.data().cend()) {
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

std::optional<Syntax::ExternalDeclaration> Parser::ParseExternalDeclaration() {
  auto curr = mTokCursor;
  // backtracking
  auto function = ParseFunctionDefinition();
  if (function) {
    return Syntax::ExternalDeclaration(std::move(*function));
  }
  mTokCursor = curr;
  if (auto declaration = ParseDeclaration(); declaration) {
    return Syntax::ExternalDeclaration(std::move(*declaration));
  }
  LCC_UNREACHABLE;
  return {};
}

/// declaration: declaration-specifiers init-declarator-list{opt} ;
std::optional<Syntax::Declaration> Parser::ParseDeclaration() {
  std::vector<Syntax::DeclarationSpecifier> declarationSpecifiers;
  while (IsDeclarationSpecifier()) {
    auto result = ParseDeclarationSpecifier();
    if (!result) {
      return {};
    }
    declarationSpecifiers.push_back(std::move(*result));
  }
  if (declarationSpecifiers.empty()) {
    logErr(mTokCursor->getLine(mSourceInterface),
           mTokCursor->getColumn(mSourceInterface),
           "Expected declaration specifiers at beginning of declaration");
  }
  if (Peek(tok::semi)) {
    Consume(tok::semi);
    return Syntax::Declaration(std::move(declarationSpecifiers), {});
  }

  using InitDeclarator = std::pair<std::unique_ptr<Syntax::Declarator>,
                                   std::unique_ptr<Syntax::Initializer>>;
  std::vector<InitDeclarator> initDeclarators;

  auto declarator = ParseDeclarator();
  if (!declarator)
    return {};
  mScope.addToScope(Semantics::declaratorToName(*declarator));
  if (!Peek(tok::equal)) {
    initDeclarators.emplace_back(
        std::make_unique<Syntax::Declarator>(std::move(*declarator)), nullptr);
  } else {
    Consume(tok::equal);
    auto initializer = ParseInitializer();
    if (!initializer)
      return {};
    initDeclarators.emplace_back(
        std::make_unique<Syntax::Declarator>(std::move(*declarator)),
        std::make_unique<Syntax::Initializer>(std::move(*initializer)));
  }

  while (Peek(tok::comma)) {
    Consume(tok::comma);
    declarator = ParseDeclarator();
    if (!declarator)
      return {};
    mScope.addToScope(Semantics::declaratorToName(*declarator));
    if (!Peek(tok::equal)) {
      initDeclarators.emplace_back(
          std::make_unique<Syntax::Declarator>(std::move(*declarator)),
          nullptr);
    } else {
      Consume(tok::equal);
      auto initializer = ParseInitializer();
      if (!initializer)
        return {};
      initDeclarators.emplace_back(
          std::make_unique<Syntax::Declarator>(std::move(*declarator)),
          std::make_unique<Syntax::Initializer>(std::move(*initializer)));
    }
  }
  Consume(tok::semi);

  if (auto *storage = std::get_if<Syntax::StorageClassSpecifier>(
          &declarationSpecifiers.front());
      storage && *storage == Syntax::StorageClassSpecifier::Typedef) {
    for (auto &[dec, init] : initDeclarators) {
      (void)init;
      auto visitor = [](auto self, auto &&value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<std::string, T>) {
          (void)self;
          return value;
        } else if constexpr (std::is_same_v<
                                 T, std::unique_ptr<Syntax::Declarator>>) {
          return std::visit(
              [&](auto &&value) -> std::string { return self(value); },
              value->getDirectDeclarator().getVariant());
        } else {
          return std::visit(
              [&](auto &&value) -> std::string { return self(value); },
              value.getDirectDeclarator().getVariant());
        }
      };
//      mScope.mTypedefs.back().insert(
//          std::visit(Y{visitor}, dec->getDirectDeclarator().getVariant()));
    }
  }
  return Syntax::Declaration(std::move(declarationSpecifiers),
                             std::move(initDeclarators));
}

std::optional<Syntax::DeclarationSpecifier>
Parser::ParseDeclarationSpecifier() {
  if (Peek(tok::kw_typedef)) {
    Consume(tok::kw_typedef);
    return Syntax::DeclarationSpecifier(Syntax::StorageClassSpecifier::Typedef);
  } else if (Peek(tok::kw_extern)) {
    Consume(tok::kw_extern);
    return Syntax::DeclarationSpecifier(Syntax::StorageClassSpecifier::Extern);
  } else if (Peek(tok::kw_static)) {
    Consume(tok::kw_static);
    return Syntax::DeclarationSpecifier(Syntax::StorageClassSpecifier::Static);
  } else if (Peek(tok::kw_auto)) {
    Consume(tok::kw_auto);
    return Syntax::DeclarationSpecifier(Syntax::StorageClassSpecifier::Auto);
  } else if (Peek(tok::kw_register)) {
    Consume(tok::kw_register);
    return Syntax::DeclarationSpecifier(
        Syntax::StorageClassSpecifier::Register);
  } else if (Peek(tok::kw_const)) {
    Consume(tok::kw_const);
    return Syntax::DeclarationSpecifier(Syntax::TypeQualifier::Const);
  } else if (Peek(tok::kw_restrict)) {
    Consume(tok::kw_restrict);
    return Syntax::DeclarationSpecifier(Syntax::TypeQualifier::Restrict);
  } else if (Peek(tok::kw_volatile)) {
    Consume(tok::kw_volatile);
    return Syntax::DeclarationSpecifier(Syntax::TypeQualifier::Volatile);
  } else if (Peek(tok::kw_inline)) {
    Consume(tok::kw_inline);
    return Syntax::DeclarationSpecifier(Syntax::FunctionSpecifier());
  } else if (Peek(tok::kw_void)) {
    Consume(tok::kw_void);
    return Syntax::DeclarationSpecifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Void));
  } else if (Peek(tok::kw_char)) {
    Consume(tok::kw_char);
    return Syntax::DeclarationSpecifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Char));
  } else if (Peek(tok::kw_short)) {
    Consume(tok::kw_short);
    return Syntax::DeclarationSpecifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Short));
  } else if (Peek(tok::kw_int)) {
    Consume(tok::kw_int);
    return Syntax::DeclarationSpecifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Int));
  } else if (Peek(tok::kw_long)) {
    Consume(tok::kw_long);
    return Syntax::DeclarationSpecifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Long));
  } else if (Peek(tok::kw_float)) {
    Consume(tok::kw_float);
    return Syntax::DeclarationSpecifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Float));
  } else if (Peek(tok::kw_double)) {
    Consume(tok::kw_double);
    return Syntax::DeclarationSpecifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Double));
  } else if (Peek(tok::kw_signed)) {
    Consume(tok::kw_signed);
    return Syntax::DeclarationSpecifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Signed));
  } else if (Peek(tok::kw_unsigned)) {
    Consume(tok::kw_unsigned);
    return Syntax::DeclarationSpecifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Unsigned));
  } else if (Peek(tok::kw_union) || Peek(tok::kw_struct)) {
    auto expected = ParseStructOrUnionSpecifier();
    if (!expected)
      return {};
    auto name = expected->getIdentifier();
    auto isDefinition = !expected->getStructDeclarations().empty();
    auto result = Syntax::TypeSpecifier(
        std::make_unique<Syntax::StructOrUnionSpecifier>(std::move(*expected)));
    if (isDefinition) {
      auto type = Semantics::declaratorsToType({std::cref(result)});
      if (!type)
        return {};
//      mScope.structOrUnions.emplace(
//          name, std::get<Semantics::RecordType>(type->get()));
    }
    return Syntax::DeclarationSpecifier{std::move(result)};
  } else if (Peek(tok::kw_enum)) {
    auto expected = ParseEnumSpecifier();
    if (!expected)
      return {};
    return Syntax::DeclarationSpecifier{Syntax::TypeSpecifier(
        std::make_unique<Syntax::EnumSpecifier>(std::move(*expected)))};
  } else if (Peek(tok::identifier)) {
    auto name = std::get<std::string>(mTokCursor->getValue());
//    if (!mScope.isInScope(name) && mScope.isTypedef(name)) {
//      return Syntax::DeclarationSpecifier{Syntax::TypeSpecifier(name)};
//    } else if (mScope.isTypedef(name)) {
//      /// error
//      logErr(mTokCursor->getLine(mSourceInterface),
//             mTokCursor->getColumn(mSourceInterface),
//             "\"" + name +
//                 "\" is a typedef but cannot be used as such because another "
//                 "symbol overshadows it");
//      return {};
//    }
  } else {
    LCC_UNREACHABLE;
  }
  return {};
}

std::optional<Syntax::StructOrUnionSpecifier>
Parser::ParseStructOrUnionSpecifier() {
  bool isUnion;
  if (Peek(tok::kw_union)) {
    isUnion = true;
  } else if (Peek(tok::kw_struct)) {
    isUnion = true;
  } else {
    logErr(mTokCursor->getLine(mSourceInterface),
           mTokCursor->getColumn(mSourceInterface),
           "Expected struct or union keyword at beginning of struct or union "
           "specifier");
    return {};
  }
  ConsumeAny();
  if (!Peek(tok::identifier)) {
    logErr(mTokCursor->getLine(mSourceInterface),
           mTokCursor->getColumn(mSourceInterface),
           std::string("Expected identifier after") +
               (isUnion ? "union" : "struct"));
    return Syntax::StructOrUnionSpecifier(isUnion, "", {});
  }
  const auto &name = std::get<std::string>(mTokCursor->getValue());
  Consume(tok::identifier);
  if (!Peek(tok::l_brace)) {
    return Syntax::StructOrUnionSpecifier(isUnion, name, {});
  }
  Consume(tok::l_brace);
  std::vector<Syntax::StructOrUnionSpecifier::StructDeclaration>
      structDeclarations;
  while (!Peek(tok::r_brace)) {
    std::vector<Syntax::SpecifierQualifier> specifierQualifiers;
    while (IsSpecifierQualifier()) {
      auto result = ParseSpecifierQualifier();
      if (!result)
        return {};
      specifierQualifiers.push_back(std::move(*result));
    }
    if (specifierQualifiers.empty()) {
      logErr(
          mTokCursor->getLine(mSourceInterface),
          mTokCursor->getColumn(mSourceInterface),
          "Expected Specifier Qualifiers at beginning of struct declarations");
    }
    std::vector<std::unique_ptr<Syntax::Declarator>> declarators;
    auto declarator = ParseDeclarator();
    if (!declarator)
      return {};
    declarators.emplace_back(
        std::make_unique<Syntax::Declarator>(std::move(*declarator)));
    while (!Peek(tok::comma)) {
      declarator = ParseDeclarator();
      if (!declarator)
        return {};
      declarators.emplace_back(
          std::make_unique<Syntax::Declarator>(std::move(*declarator)));
    }
    assert(Match(tok::semi));
    structDeclarations.push_back(
        {std::move(specifierQualifiers), std::move(declarators)});
  }
  assert(Match(tok::r_brace));
  return Syntax::StructOrUnionSpecifier(isUnion, name,
                                        std::move(structDeclarations));
}

std::optional<Syntax::SpecifierQualifier> Parser::ParseSpecifierQualifier() {
  if (Peek(tok::kw_const)) {
    Consume(tok::kw_const);
    return Syntax::SpecifierQualifier(Syntax::TypeQualifier::Const);
  } else if (Peek(tok::kw_restrict)) {
    Consume(tok::kw_restrict);
    return Syntax::SpecifierQualifier(Syntax::TypeQualifier::Restrict);
  } else if (Peek(tok::kw_volatile)) {
    Consume(tok::kw_volatile);
    return Syntax::SpecifierQualifier(Syntax::TypeQualifier::Volatile);
  } else if (Peek(tok::kw_void)) {
    Consume(tok::kw_void);
    return Syntax::SpecifierQualifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Void));
  } else if (Peek(tok::kw_char)) {
    Consume(tok::kw_char);
    return Syntax::SpecifierQualifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Char));
  } else if (Peek(tok::kw_short)) {
    Consume(tok::kw_short);
    return Syntax::SpecifierQualifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Short));
  } else if (Peek(tok::kw_int)) {
    Consume(tok::kw_int);
    return Syntax::SpecifierQualifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Int));
  } else if (Peek(tok::kw_long)) {
    Consume(tok::kw_long);
    return Syntax::SpecifierQualifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Long));
  } else if (Peek(tok::kw_float)) {
    Consume(tok::kw_float);
    return Syntax::SpecifierQualifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Float));
  } else if (Peek(tok::kw_double)) {
    Consume(tok::kw_double);
    return Syntax::SpecifierQualifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Double));
  } else if (Peek(tok::kw_signed)) {
    Consume(tok::kw_signed);
    return Syntax::SpecifierQualifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Signed));
  } else if (Peek(tok::kw_unsigned)) {
    Consume(tok::kw_unsigned);
    return Syntax::SpecifierQualifier(Syntax::TypeSpecifier(
        Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Unsigned));
  } else if (Peek(tok::kw_union) || Peek(tok::kw_struct)) {
    auto expected = ParseStructOrUnionSpecifier();
    if (!expected)
      return {};
    auto name = expected->getIdentifier();
    auto isDefinition = !expected->getStructDeclarations().empty();
    auto result = Syntax::TypeSpecifier(
        std::make_unique<Syntax::StructOrUnionSpecifier>(std::move(*expected)));
    if (isDefinition) {
      auto type = Semantics::declaratorsToType({std::cref(result)});
      if (!type)
        return {};
//      mScope.structOrUnions.emplace(
//          name, std::get<Semantics::RecordType>(type->get()));
    }
    return Syntax::SpecifierQualifier{std::move(result)};
  } else if (Peek(tok::kw_enum)) {
    auto expected = ParseEnumSpecifier();
    if (!expected)
      return {};
    return Syntax::SpecifierQualifier{Syntax::TypeSpecifier(
        std::make_unique<Syntax::EnumSpecifier>(std::move(*expected)))};
  } else if (Peek(tok::identifier)) {
    auto name = std::get<std::string>(mTokCursor->getValue());
//    if (!mScope.isInScope(name) && mScope.isTypedef(name)) {
//      return Syntax::SpecifierQualifier{Syntax::TypeSpecifier(name)};
//    } else if (mScope.isTypedef(name)) {
//      /// error
//      logErr(mTokCursor->getLine(mSourceInterface),
//             mTokCursor->getColumn(mSourceInterface),
//             "\"" + name +
//                 "\" is a typedef but cannot be used as such because another "
//                 "symbol overshadows it");
//      return {};
//    }
  } else {
    LCC_UNREACHABLE;
  }
  return {};
}

/// declarator: pointer{opt} direct-declarator
std::optional<Syntax::Declarator> Parser::ParseDeclarator() {
  std::vector<Syntax::Pointer> pointers;
  while (Peek(tok::star)) {
    auto result = ParsePointer();
    if (!result)
      return {};
    pointers.push_back(std::move(*result));
  }
  auto directDeclarator = ParseDirectDeclarator();
  if (!directDeclarator)
    return {};
  return Syntax::Declarator(std::move(pointers), std::move(*directDeclarator));
}

std::optional<Syntax::DirectDeclaratorSquare>
Parser::ParseDirectDeclaratorSquare(Syntax::DirectDeclarator &declarator) {
  assert(Match(tok::l_square));
  std::vector<Syntax::TypeQualifier> typeQualifiers;
  while (Peek(tok::kw_const) || Peek(tok::kw_restrict) ||
         Peek(tok::kw_volatile)) {
    switch (mTokCursor->getTokenKind()) {
    case tok::kw_const: {
      Consume(tok::kw_const);
      typeQualifiers.push_back(Syntax::TypeQualifier::Const);
      break;
    }
    case tok::kw_restrict: {
      Consume(tok::kw_restrict);
      typeQualifiers.push_back(Syntax::TypeQualifier::Restrict);
      break;
    }
    case tok::kw_volatile: {
      Consume(tok::kw_volatile);
      typeQualifiers.push_back(Syntax::TypeQualifier::Volatile);
      break;
    }
    default:
      break;
    }
  }
  auto assignment = ParseAssignExpr();
  Consume(tok::r_square);
  return Syntax::DirectDeclaratorSquare(
      std::make_unique<Syntax::DirectDeclarator>(std::move(declarator)),
      std::move(typeQualifiers),
      assignment ? std::make_unique<Syntax::AssignExpr>(std::move(*assignment))
                 : nullptr);
}

/**
direct-declarator:
    identifier
    ( declarator )
    direct-declarator [ type-qualifier-list{opt} assignment-expression{opt} ]
//    direct-declarator [ static type-qualifier-list{opt} assignment-expression
]
//    direct-declarator [type-qualifier-list static assignment-expression]
//    direct-declarator [type-qualifier-list{opt} *]
    direct-declarator ( parameter-type-list )
//    direct-declarator ( identifier-list{opt} )
 */
std::optional<Syntax::DirectDeclarator> Parser::ParseDirectDeclarator() {
  std::unique_ptr<Syntax::DirectDeclarator> directDeclarator;
  while (Peek(tok::identifier) || Peek(tok::l_paren) || Peek(tok::l_square)) {
    switch (mTokCursor->getTokenKind()) {
    case tok::identifier: {
      directDeclarator = std::make_unique<Syntax::DirectDeclarator>(
          std::get<std::string>(mTokCursor->getValue()));
      Consume(tok::identifier);
      break;
    }
    case tok::l_paren: {
      Consume(tok::l_paren);
      if (directDeclarator) {
        if (IsDeclarationSpecifier()) {
          auto parameterTypeList = ParseParameterTypeList();
          if (!parameterTypeList)
            return {};
          directDeclarator = std::make_unique<Syntax::DirectDeclarator>(
              Syntax::DirectDeclaratorParentParameters(
                  std::move(*directDeclarator), std::move(*parameterTypeList)));
        } else {
          LCC_UNREACHABLE;
        }
      } else {
        /// ( declarator )
        auto declarator = ParseDeclarator();
        if (!declarator)
          return {};
        directDeclarator = std::make_unique<Syntax::DirectDeclarator>(
            std::make_unique<Syntax::Declarator>(std::move(*declarator)));
      }
      assert(Match(tok::r_paren));
      break;
    }
    case tok::l_square: {
      assert(directDeclarator);
      auto noStaticDecl = ParseDirectDeclaratorSquare(*directDeclarator);
      if (!noStaticDecl)
        return {};
      directDeclarator =
          std::make_unique<Syntax::DirectDeclarator>(std::move(*noStaticDecl));
      break;
    }
    default:
      break;
    }
  }
  if (!directDeclarator) {
    logErr(mTokCursor->getLine(mSourceInterface),
           mTokCursor->getColumn(mSourceInterface), "Expected declarator");
    return {};
  }
  return std::move(*directDeclarator);
}

/*
parameter-type-list:
  parameter-list
  parameter-list , ...
 */
std::optional<Syntax::ParameterTypeList> Parser::ParseParameterTypeList() {
  auto cur = mTokCursor;
  auto parameterList = ParseParameterList();
  if (!parameterList)
    return {};
  bool hasEllipse = false;
  if (Peek(tok::comma)) {
    Consume(tok::comma);
    assert(Match(tok::ellipsis));
    hasEllipse = true;
  }
  return Syntax::ParameterTypeList(std::move(*parameterList), hasEllipse);
}

/**
parameter-list:
    parameter-declaration
    parameter-list , parameter-declaration
 */
std::optional<Syntax::ParameterList> Parser::ParseParameterList() {
  std::vector<Syntax::ParameterDeclaration> parameterDeclarations;
  auto declaration = ParseParameterDeclaration();
  if (!declaration)
    return {};
  parameterDeclarations.push_back(std::move(*declaration));
  while (Peek(tok::comma)) {
    Consume(tok::comma);
    declaration = ParseParameterDeclaration();
    if (!declaration)
      return {};
    parameterDeclarations.push_back(std::move(*declaration));
  }
  return Syntax::ParameterList(std::move(parameterDeclarations));
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
  std::vector<Syntax::DeclarationSpecifier> declarationSpecifiers;
  while (IsDeclarationSpecifier()) {
    auto result = ParseDeclarationSpecifier();
    if (!result)
      return {};
    declarationSpecifiers.push_back(std::move(*result));
  }
  assert(!declarationSpecifiers.empty());
  bool hasStar = false;
  auto result = std::find_if(mTokCursor, mTokEnd,
                             [&hasStar](const CToken &token) -> bool {
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
std::optional<Syntax::Pointer> Parser::ParsePointer() {
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
    if (!result)
      return {};
    pointers.push_back(std::move(*result));
  }
  auto result = ParseDirectAbstractDeclarator();
  if (!result) {
    return {};
  }
  return Syntax::AbstractDeclarator(std::move(pointers), std::move(*result));
}

/**
 abstract-declarator:
   pointer
   pointer{opt} direct-abstract-declarator

 direct-abstract-declarator:
    ( abstract-declarator )
    direct-abstract-declarator{opt} [ type-qualifier-list{opt}
assignment-expression{opt} ]
//    direct-abstract-declarator{opt} [ static type-qualifier-list{opt}
assignment-expression ]
//    direct-abstract-declarator{opt} [ type-qualifier-list static
assignment-expression ] direct-abstract-declarator{opt} [ * ]
    direct-abstract-declarator{opt} ( parameter-type-list{opt} )
 */
std::optional<Syntax::DirectAbstractDeclarator>
Parser::ParseDirectAbstractDeclarator() {
  std::unique_ptr<Syntax::DirectAbstractDeclarator> directAbstractDeclarator;
  while (Peek(tok::l_paren) || Peek(tok::l_square)) {
    switch (mTokCursor->getTokenKind()) {
    case tok::l_paren: {
      Consume(tok::l_paren);
      /// direct-abstract-declarator{opt} ( parameter-type-list{opt} )
      if (IsDeclarationSpecifier()) {
        auto parameterTypeList = ParseParameterTypeList();
        if (!parameterTypeList)
          return {};
        directAbstractDeclarator =
            std::make_unique<Syntax::DirectAbstractDeclarator>(
                Syntax::DirectAbstractDeclaratorParameterTypeList(
                    std::move(directAbstractDeclarator),
                    std::make_unique<Syntax::ParameterTypeList>(
                        std::move(*parameterTypeList))));
      }
      /// abstract-declarator first set
      else if (Peek(tok::l_paren) || Peek(tok::l_square) || Peek(tok::star)) {
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
                Syntax::DirectAbstractDeclaratorParameterTypeList(
                    std::move(directAbstractDeclarator), nullptr));
      }
      assert(Match(tok::r_paren));
      break;
    }
    case tok::l_square: {
      Consume(tok::l_square);
      if (Peek(tok::star)) {
        Consume(tok::star);
        directAbstractDeclarator =
            std::make_unique<Syntax::DirectAbstractDeclarator>(
                std::move(directAbstractDeclarator));
      } else {
        if (!Peek(tok::r_square)) {
          auto assignment = ParseAssignExpr();
          if (!assignment) {
            return {};
          }
          directAbstractDeclarator =
              std::make_unique<Syntax::DirectAbstractDeclarator>(
                  Syntax::DirectAbstractDeclaratorAssignmentExpression(
                      std::move(directAbstractDeclarator),
                      std::make_unique<Syntax::AssignExpr>(
                          std::move(*assignment))));
        } else {
          directAbstractDeclarator =
              std::make_unique<Syntax::DirectAbstractDeclarator>(
                  Syntax::DirectAbstractDeclaratorAssignmentExpression(
                      std::move(directAbstractDeclarator), nullptr));
        }
      }
      if (mTokCursor->getTokenKind() != tok::r_square) {
        logErr(mTokCursor->getLine(mSourceInterface),
               mTokCursor->getColumn(mSourceInterface),
               "Expected ] to match [");
      }
      break;
    }
    default:
      goto Exit;
    }
  }
Exit:
  assert(directAbstractDeclarator);
  return std::move(*directAbstractDeclarator);
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
  if (Peek(tok::l_brace)) {
    auto declaration = ParseEnumDeclaration("");
    if (!declaration)
      return {};
    return Syntax::EnumSpecifier(std::move(*declaration));
  }

  Expect(tok::identifier);
  auto identifier = mTokCursor;
  auto name = std::get<std::string>(identifier->getValue());
  Consume(tok::identifier);
  if (Peek(tok::l_brace)) {
    auto declaration = ParseEnumDeclaration(name);
    if (!declaration)
      return {};
    return Syntax::EnumSpecifier(std::move(*declaration));
  }
  return Syntax::EnumSpecifier(std::move(name));
}

std::optional<Syntax::EnumDeclaration>
Parser::ParseEnumDeclaration(std::string enumName) {
  Expect(tok::l_brace);
  Consume(tok::l_brace);

  std::vector<std::pair<std::string, std::int32_t>> values;
  while (!Peek(tok::r_brace)) {
    Expect(tok::identifier);
    std::string valueName = std::get<std::string>(mTokCursor->getValue());
    Consume(tok::identifier);
    std::int32_t value = values.empty() ? 0 : values.back().second + 1;
    if (Peek(tok::comma)) {
      Consume(tok::comma);
    }
    values.emplace_back(valueName, value);
  }
  assert(Match(tok::r_brace));
  return Syntax::EnumDeclaration(std::move(enumName), values);
}

/**
function-definition:
    declaration-specifiers declarator declaration-list{opt} compound-statement
    declaration-list:
        declaration
        declaration-list declaration
 */
std::optional<Syntax::FunctionDefinition> Parser::ParseFunctionDefinition() {
  std::vector<Syntax::DeclarationSpecifier> declarationSpecifiers;
  while (IsDeclarationSpecifier()) {
    auto result = ParseDeclarationSpecifier();
    if (!result)
      return {};
    declarationSpecifiers.push_back(std::move(*result));
  }
  assert(!declarationSpecifiers.empty());
  auto declarator = ParseDeclarator();
  if (!declarator)
    return {};
  mScope.addToScope(Semantics::declaratorToName(*declarator));
  mScope.pushScope();
  auto *parameters = std::get_if<Syntax::DirectDeclaratorParentParameters>(
      &declarator->getDirectDeclarator().getVariant());
  assert(parameters);
  {
    auto &parameterDeclarations = parameters->getParameterTypeList()
                                      .getParameterList()
                                      .getParameterDeclarations();
    for (auto &[specifier, paramDeclarator] : parameterDeclarations) {
      if (parameterDeclarations.size() == 1 && specifier.size() == 1 &&
          std::holds_alternative<Syntax::TypeSpecifier>(specifier[0])) {
        auto *primitive =
            std::get_if<Syntax::TypeSpecifier::PrimitiveTypeSpecifier>(
                &std::get<Syntax::TypeSpecifier>(specifier[0]).getVariant());
        if (primitive &&
            *primitive == Syntax::TypeSpecifier::PrimitiveTypeSpecifier::Void) {
          break;
        }
      }

      if (std::holds_alternative<std::unique_ptr<Syntax::AbstractDeclarator>>(
              paramDeclarator)) {
        continue;
      }
      auto &decl =
          std::get<std::unique_ptr<Syntax::Declarator>>(paramDeclarator);
      auto visitor = [](auto self, auto &&value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<std::string, T>) {
          (void)self;
          return value;
        } else if constexpr (std::is_same_v<
                                 T, std::unique_ptr<Syntax::Declarator>>) {
          return std::visit(
              [&](auto &&value) -> std::string { return self(value); },
              value->getDirectDeclarator().getVariant());
        } else if constexpr (!std::is_same_v<
                                 T, Syntax::DirectDeclaratorParentParameters>) {
          return std::visit(
              [&](auto &&value) -> std::string { return self(value); },
              value.getDirectDeclarator().getVariant());
        } else {
          (void)self;
          return "";
        }
      };
      auto result =
          std::visit(Y{visitor}, decl->getDirectDeclarator().getVariant());
      if (!result.empty()) {
        mScope.addToScope(result);
      }
    }
  }
  auto compoundStatement = ParseBlockStmt();
  mScope.popScope();
  if (!compoundStatement) {
    return {};
  }

  return Syntax::FunctionDefinition(std::move(declarationSpecifiers),
                                    std::move(*declarator),
                                    std::move(*compoundStatement));
}

std::optional<Syntax::BlockStmt> Parser::ParseBlockStmt() {
  assert(Match(tok::l_brace));
  std::vector<Syntax::BlockItem> items;
  mScope.pushScope();
  while (!Peek(tok::r_brace)) {
    auto result = ParseBlockItem();
    if (!result)
      return {};
    items.push_back(std::move(*result));
  }
  mScope.popScope();
  assert(Match(tok::r_brace));
  return Syntax::BlockStmt(std::move(items));
}

std::optional<Syntax::BlockItem> Parser::ParseBlockItem() {
  // backtracking
  auto curr = mTokCursor;
  if (auto declaration = ParseDeclaration(); declaration) {
    return Syntax::BlockItem(std::move(*declaration));
  } else {
    mTokCursor = curr;
    auto statement = ParseStmt();
    if (!statement) {
      assert(0);
      return {};
    }
    return Syntax::BlockItem(std::move(*statement));
  }
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
    if (!assignment)
      return {};
    return Syntax::Initializer(std::move(*assignment));
  } else {
    Consume(tok::l_brace);
    auto initializerList = ParseInitializerList();
    if (!initializerList)
      return {};

    if (Peek(tok::comma)) {
      Consume(tok::comma);
    }
    assert(Match(tok::r_brace));
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
  std::vector<std::variant<std::size_t, std::string>> variants;
  while (Peek(tok::l_square) || Peek(tok::period)) {
    if (Peek(tok::l_square)) {
      Consume(tok::l_square);
      auto constant = ParseAssignExpr();
      if (!constant)
        return {};
      Expect(tok::r_square);
      Consume(tok::r_square);
//      Semantics::ConstantEvaluator evaluator(mScope.structOrUnions);
//      auto constValue = evaluator.visit(*constant);
//      if (!constValue)
//        return {};
//      variants.emplace_back(std::visit(
//          [](auto &&value) -> std::size_t {
//            using T = std::decay_t<decltype(value)>;
//            if constexpr (std::is_convertible_v<T, std::size_t>) {
//              return value;
//            } else {
//              throw std::runtime_error("Invalid type of constant expression");
//            }
//          },
//          *constValue));
    } else if (Peek(tok::period)) {
      Consume(tok::period);
      Expect(tok::identifier);
      variants.emplace_back(std::get<std::string>(mTokCursor->getValue()));
      Consume(tok::identifier);
    }
  }
  if (!variants.empty()) {
    if (Peek(tok::equal)) {
      Consume(tok::equal);
    }
  }
  auto initializer = ParseInitializer();
  if (!initializer)
    return {};
  vector.push_back({std::move(*initializer), variants});

  while (Peek(tok::comma)) {
    Consume(tok::comma);
    std::vector<std::variant<std::size_t, std::string>> variants;
    while (Peek(tok::l_square) || Peek(tok::period)) {
      if (Peek(tok::l_square)) {
        Consume(tok::l_square);
        auto constant = ParseAssignExpr();
        if (!constant)
          return {};
        Expect(tok::r_square);
        Consume(tok::r_square);
//        Semantics::ConstantEvaluator evaluator(mScope.structOrUnions);
//        auto constValue = evaluator.visit(*constant);
//        if (!constValue)
//          return {};
//        variants.emplace_back(std::visit(
//            [](auto &&value) -> std::size_t {
//              using T = std::decay_t<decltype(value)>;
//              if constexpr (std::is_convertible_v<T, std::size_t>) {
//                return value;
//              } else {
//                throw std::runtime_error("Invalid type of constant expression");
//              }
//            },
//            *constValue));
      } else if (Peek(tok::period)) {
        Consume(tok::period);
        Expect(tok::identifier);
        variants.emplace_back(std::get<std::string>(mTokCursor->getValue()));
        Consume(tok::identifier);
      }
    }
    if (!variants.empty()) {
      if (Peek(tok::equal)) {
        Consume(tok::equal);
      }
    }
    auto initializer = ParseInitializer();
    if (!initializer)
      return {};
    vector.push_back({std::move(*initializer), variants});
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
        const auto &name = std::get<std::string>(start->getValue());
        return Syntax::Stmt(Syntax::LabelStmt(name));
      }
    }
    /// expr{opt};
    return ParseExprStmt();
  }
}

/// if ( expression ) statement
/// if ( expression ) statement else statement
std::optional<Syntax::Stmt> Parser::ParseIfStmt() {
  Consume(tok::kw_if);
  assert(Match(tok::l_paren));
  auto expr = ParseExpr();
  if (!expr)
    return {};
  assert(Match(tok::r_paren));
  auto thenStmt = ParseStmt();
  if (!thenStmt)
    return {};
  if (Match(tok::kw_else)) {
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
  assert(Match(tok::l_paren));
  auto expr = ParseExpr();
  if (!expr)
    return {};
  assert(Match(tok::r_paren));
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
  assert(Match(tok::kw_while));
  assert(Match(tok::l_paren));
  auto expr = ParseExpr();
  if (!expr)
    return {};
  assert(Match(tok::r_paren));
  assert(Match(tok::semi));
  return Syntax::Stmt{Syntax::DoWhileStmt(
      std::make_unique<Syntax::Stmt>(std::move(*stmt)), std::move(*expr))};
}

/// for ( expression{opt} ; expression{opt} ; expression{opt} ) statement
/// for ( declaration expression{opt} ; expression{opt} ) statement
std::optional<Syntax::Stmt> Parser::ParseForStmt() {
  Consume(tok::kw_for);
  assert(Match(tok::l_paren));
  auto blockItem = ParseBlockItem();
  if (!blockItem)
    return {};

  std::unique_ptr<Syntax::Expr> control;
  if (std::holds_alternative<Syntax::Declaration>(blockItem->getVariant()) ||
      mTokCursor->getTokenKind() != tok::semi) {
    auto expr = ParseExpr();
    if (!expr)
      return {};
    assert(Match(tok::semi));
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
    assert(Match(tok::r_paren));
    post = std::make_unique<Syntax::Expr>(std::move(*expr));
  }

  auto stmt = ParseStmt();
  if (!stmt)
    return {};
  if (auto declaration =
          std::get_if<Syntax::Declaration>(&blockItem->getVariant())) {
    return Syntax::Stmt(Syntax::ForDeclarationStmt(
        std::make_unique<Syntax::Stmt>(std::move(*stmt)),
        std::move(*declaration), std::move(control), std::move(post)));
  } else if (auto expr = std::get_if<Syntax::ExprStmt>(
                 &std::get<Syntax::Stmt>(blockItem->getVariant())
                      .getVariant())) {
    return Syntax::Stmt(Syntax::ForStmt(
        std::make_unique<Syntax::Stmt>(std::move(*stmt)),
        expr->moveOptionalExpr(), std::move(control), std::move(post)));
  } else {
    LCC_UNREACHABLE;
  }
}

/// break;
std::optional<Syntax::Stmt> Parser::ParseBreakStmt() {
  assert(Match(tok::kw_break));
  assert(Match(tok::semi));
  return Syntax::Stmt{Syntax::BreakStmt()};
}

/// continue;
std::optional<Syntax::Stmt> Parser::ParseContinueStmt() {
  assert(Match(tok::kw_continue));
  assert(Match(tok::semi));
  return Syntax::Stmt{Syntax::ContinueStmt()};
}

/// return expr{opt};
std::optional<Syntax::Stmt> Parser::ParseReturnStmt() {
  assert(Match(tok::kw_return));
  if (Peek(tok::semi)) {
    return Syntax::Stmt{Syntax::ReturnStmt()};
  }
  auto expr = ParseExpr();
  if (!expr)
    return {};
  assert(Match(tok::semi));
  return Syntax::Stmt{
      Syntax::ReturnStmt(std::make_unique<Syntax::Expr>(std::move(*expr)))};
}

/// expr;
std::optional<Syntax::Stmt> Parser::ParseExprStmt() {
  if (Peek(tok::semi)) {
    return Syntax::Stmt(Syntax::ExprStmt());
  }
  auto expr = ParseExpr();
  if (!expr)
    return {};
  assert(Match(tok::semi));
  return Syntax::Stmt(
      Syntax::ExprStmt(std::make_unique<Syntax::Expr>(std::move(*expr))));
}

/// switch ( expression ) statement
std::optional<Syntax::Stmt> Parser::ParseSwitchStmt() {
  Consume(tok::kw_switch);
  assert(Match(tok::l_paren));
  auto expr = ParseExpr();
  if (!expr)
    return {};
  assert(Match(tok::r_paren));
  auto stmt = ParseStmt();
  if (!stmt)
    return {};
  return Syntax::Stmt(Syntax::SwitchStmt(
      std::move(*expr), std::make_unique<Syntax::Stmt>(std::move(*stmt))));
}

/// case expr: stmt
std::optional<Syntax::Stmt> Parser::ParseCaseStmt() {
  Consume(tok::kw_case);
  auto expr = ParseAssignExpr();
  if (!expr)
    return {};
  assert(Match(tok::colon));
//  Semantics::ConstantEvaluator evaluator(mScope.structOrUnions);
//  auto stmt = ParseStmt();
//  if (!stmt)
//    return {};
//  auto constValue = evaluator.visit(*expr);
//  if (!constValue)
//    return {};
//  return Syntax::Stmt(Syntax::CaseStmt(
//      *constValue, std::make_unique<Syntax::Stmt>(std::move(*stmt))));
}

/// default: stmt
std::optional<Syntax::Stmt> Parser::ParseDefaultStmt() {
  Consume(tok::kw_default);
  assert(Match(tok::colon));
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
  const auto &name = std::get<std::string>(mTokCursor->getValue());
  Consume(tok::identifier);
  assert(Match(tok::semi));
  return Syntax::Stmt(Syntax::GotoStmt(name));
}

std::optional<Syntax::Expr> Parser::ParseExpr() {
  std::vector<Syntax::AssignExpr> expressions;
  auto assignment = ParseAssignExpr();
  if (!assignment) {
    return {};
  }
  expressions.push_back(std::move(*assignment));

  while (mTokCursor->getTokenKind() == tok::comma) {
    mTokCursor++;
    assignment = ParseAssignExpr();
    if (!assignment) {
      assert(0);
      break;
    }
    expressions.push_back(std::move(*assignment));
  }
  return Syntax::Expr(std::move(expressions));
}

std::optional<Syntax::AssignExpr> Parser::ParseAssignExpr() {
  auto curr = mTokCursor;
  auto unary = ParseUnaryExpr();
  // TODO: backtracking
  if (unary) {
    tok::TokenKind tokenKind = mTokCursor->getTokenKind();
    if (IsAssignment(mTokCursor->getTokenKind())) {
      ConsumeAny();
      auto assignment = ParseAssignExpr();
      if (!assignment) {
        return {};
      }
      return Syntax::AssignExpr(Syntax::AssignExprAssign(
          std::move(*unary), tokenKind,
          std::make_unique<Syntax::AssignExpr>(std::move(*assignment))));
    } else {
      mTokCursor = curr;
    }
  }
  auto cond = ParseConditionalExpr();
  if (!cond) {
    return {};
  }
  return Syntax::AssignExpr(std::move(*cond));
}

std::optional<Syntax::ConditionalExpr> Parser::ParseConditionalExpr() {
  auto logOrExpr = ParseLogOrExpr();
  if (!logOrExpr) {
    return {};
  }
  if (Peek(tok::question)) {
    Consume(tok::question);
    auto expr = ParseExpr();
    if (!expr) {
      return {};
    }
    assert(Match(tok::colon));
    auto optionalConditional = ParseConditionalExpr();
    if (!optionalConditional) {
      return optionalConditional;
    }
    return Syntax::ConditionalExpr(
        std::move(*logOrExpr), std::make_unique<Syntax::Expr>(std::move(*expr)),
        std::make_unique<Syntax::ConditionalExpr>(
            std::move(*optionalConditional)));
  }

  return Syntax::ConditionalExpr(std::move(*logOrExpr));
}

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

std::optional<Syntax::EqualExpr> Parser::ParseEqualExpr() {
  auto result = ParseRelationalExpr();
  if (!result) {
    return {};
  }

  std::vector<std::pair<tok::TokenKind, Syntax::RelationalExpr>>
      relationalExpressions;
  while (mTokCursor->getTokenKind() == tok::equal_equal ||
         mTokCursor->getTokenKind() == tok::exclaim_equal) {
    tok::TokenKind tokenType = mTokCursor->getTokenKind();
    auto newRelational = ParseRelationalExpr();
    if (!newRelational) {
      break;
    }
    relationalExpressions.emplace_back(tokenType, std::move(*newRelational));
  }

  return Syntax::EqualExpr(std::move(*result),
                           std::move(relationalExpressions));
}

std::optional<Syntax::RelationalExpr> Parser::ParseRelationalExpr() {
  auto result = ParseShiftExpr();
  if (!result) {
    return {};
  }
  std::vector<std::pair<tok::TokenKind, Syntax::ShiftExpr>> relationalExprArr;
  while (mTokCursor->getTokenKind() == tok::less ||
         mTokCursor->getTokenKind() == tok::less_equal ||
         mTokCursor->getTokenKind() == tok::greater ||
         mTokCursor->getTokenKind() == tok::greater_equal) {
    tok::TokenKind tokenType = mTokCursor->getTokenKind();
    ConsumeAny();
    auto newShift = ParseShiftExpr();
    if (!newShift) {
      break;
    }

    relationalExprArr.emplace_back(tokenType, std::move(*newShift));
  }

  return Syntax::RelationalExpr(std::move(*result),
                                std::move(relationalExprArr));
}

std::optional<Syntax::ShiftExpr> Parser::ParseShiftExpr() {
  auto result = ParseAdditiveExpr();
  if (!result) {
    return {};
  }

  std::vector<std::pair<tok::TokenKind, Syntax::AdditiveExpr>> additiveExprArr;
  while (mTokCursor->getTokenKind() == tok::less_less ||
         mTokCursor->getTokenKind() == tok::greater_greater) {
    tok::TokenKind tokenType = mTokCursor->getTokenKind();
    auto newAdd = ParseAdditiveExpr();
    if (!newAdd) {
      break;
    }
    additiveExprArr.emplace_back(tokenType, std::move(*newAdd));
  }
  return Syntax::ShiftExpr(std::move(*result), std::move(additiveExprArr));
}

std::optional<Syntax::AdditiveExpr> Parser::ParseAdditiveExpr() {
  auto result = ParseMultiExpr();
  if (!result) {
    return {};
  }

  std::vector<std::pair<tok::TokenKind, Syntax::MultiExpr>> multiExprArr;
  while (mTokCursor->getTokenKind() == tok::plus ||
         mTokCursor->getTokenKind() == tok::minus) {
    tok::TokenKind tokenType = mTokCursor->getTokenKind();
    ConsumeAny();
    auto newMul = ParseMultiExpr();
    if (!newMul) {
      break;
    }
    multiExprArr.emplace_back(tokenType, std::move(*newMul));
  }

  return Syntax::AdditiveExpr(std::move(*result), std::move(multiExprArr));
}

std::optional<Syntax::MultiExpr> Parser::ParseMultiExpr() {
  auto result = ParseCastExpr();
  if (!result) {
    return {};
  }

  std::vector<std::pair<tok::TokenKind, Syntax::CastExpr>> castExprArr;
  while (mTokCursor->getTokenKind() == tok::star ||
         mTokCursor->getTokenKind() == tok::slash ||
         mTokCursor->getTokenKind() == tok::percent) {
    tok::TokenKind tokenType = mTokCursor->getTokenKind();
    ConsumeAny();
    auto newCast = ParseCastExpr();
    if (!newCast) {
      break;
    }
    castExprArr.emplace_back(tokenType, std::move(*newCast));
  }

  return Syntax::MultiExpr(std::move(*result), std::move(castExprArr));
}

std::optional<Syntax::TypeName> Parser::ParseTypeName() {
  std::vector<Syntax::SpecifierQualifier> specifierQualifiers;
  while (auto result = ParseSpecifierQualifier()) {
    specifierQualifiers.push_back(std::move(*result));
  }
  if (specifierQualifiers.empty()) {
    logErr(
        mTokCursor->getLine(mSourceInterface),
        mTokCursor->getColumn(mSourceInterface),
        "Expected at least one specifier qualifier at beginning of typename");
    return {};
  }

  if (auto abstractDec = ParseAbstractDeclarator()) {
    return Syntax::TypeName(
        std::move(specifierQualifiers),
        std::make_unique<Syntax::AbstractDeclarator>(std::move(*abstractDec)));
  }
  return Syntax::TypeName(std::move(specifierQualifiers), nullptr);
}

std::optional<Syntax::CastExpr> Parser::ParseCastExpr() {
  auto start = mTokCursor;
  if (Peek(tok::l_paren)) {
    ConsumeAny();
    auto typeName = ParseTypeName();
    if (typeName) {
      assert(Match(tok::r_paren));
      auto cast = ParseCastExpr();
      if (cast) {
        return Syntax::CastExpr(
            std::pair{std::move(*typeName),
                      std::make_unique<Syntax::CastExpr>(std::move(*cast))});
      }
    }
  }
  mTokCursor = start;
  auto unary = ParseUnaryExpr();
  if (!unary) {
    return {};
  }
  return Syntax::CastExpr(std::move(*unary));
}

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
    ConsumeAny();
    auto unary = ParseUnaryExpr();
    if (!unary) {
      return {};
    }
    return Syntax::UnaryExpr(Syntax::UnaryExprUnaryOperator(
        tokenType, std::make_unique<Syntax::UnaryExpr>(std::move(*unary))));
  } else {
    auto postFix = ParsePostFixExpr();
    if (!postFix) {
      return {};
    }
    return Syntax::UnaryExpr(Syntax::UnaryExprPostFixExpr(std::move(*postFix)));
  }
}

std::optional<Syntax::PostFixExpr> Parser::ParsePostFixExpr() {
  std::stack<std::unique_ptr<Syntax::PostFixExpr>> stack;
  while (IsPostFixExpr(mTokCursor->getTokenKind())) {
    auto tokType = mTokCursor->getTokenKind();
    if (tokType == tok::identifier || tokType == tok::numeric_constant ||
        tokType == tok::char_constant || tokType == tok::string_literal) {
      assert(stack.empty());
      auto newPrimary = ParsePrimaryExpr();
      if (!newPrimary) {
        return {};
      }
      stack.push(std::make_unique<Syntax::PostFixExpr>(
          Syntax::PostFixExprPrimary(std::move(*newPrimary))));
    } else if (tokType == tok::l_paren) {
      Consume(tok::l_paren);
      std::vector<Syntax::AssignExpr> params;
      if (!Peek(tok::r_paren)) {
        auto assignment = ParseAssignExpr();
        if (!assignment) {
          return {};
        }
        params.push_back(std::move(*assignment));
      }
      while (mTokCursor->getTokenKind() != tok::r_paren) {
        assert(Match(tok::comma));
        auto assignment = ParseAssignExpr();
        if (!assignment) {
          return {};
        }
        params.push_back(std::move(*assignment));
      }
      Consume(tok::r_paren);
      auto postExpr = std::move(stack.top());
      stack.pop();
      stack.push(std::make_unique<Syntax::PostFixExpr>(
          Syntax::PostFixExprFuncCall(std::move(postExpr), std::move(params))));
    } else if (tokType == tok::l_square) {
      Consume(tok::l_square);
      auto expr = ParseExpr();
      if (!expr) {
        return {};
      }
      assert(Match(tok::r_square));
      auto postExpr = std::move(stack.top());
      stack.pop();
      stack.push(std::make_unique<Syntax::PostFixExpr>(
          Syntax::PostFixExprSubscript(std::move(postExpr), std::move(*expr))));
    } else if (tokType == tok::plus_plus) {
      Consume(tok::plus_plus);
      auto postExpr = std::move(stack.top());
      stack.pop();
      stack.push(std::make_unique<Syntax::PostFixExpr>(
          Syntax::PostFixExprIncrement(std::move(postExpr))));
    } else if (tokType == tok::minus_minus) {
      auto postExpr = std::move(stack.top());
      stack.pop();
      stack.push(std::make_unique<Syntax::PostFixExpr>(
          Syntax::PostFixExprDecrement(std::move(postExpr))));
    } else if (tokType == tok::period) {
      Consume(tok::period);
      Expect(tok::identifier);
      std::string identifier = std::get<std::string>(mTokCursor->getValue());
      assert(Match(tok::identifier));
      auto postExpr = std::move(stack.top());
      stack.pop();
      stack.push(std::make_unique<Syntax::PostFixExpr>(
          Syntax::PostFixExprDot(std::move(postExpr), identifier)));
    } else if (tokType == tok::arrow) {
      Consume(tok::arrow);
      Expect(tok::identifier);
      const auto &name = std::get<std::string>(mTokCursor->getValue());
      assert(Match(tok::identifier));
      auto postExpr = std::move(stack.top());
      stack.pop();
      stack.push(std::make_unique<Syntax::PostFixExpr>(
          Syntax::PostFixExprArrow(std::move(postExpr), name)));
    }
  }
  assert(stack.size() == 1);
  auto ret = std::move(*stack.top());
  stack.pop();
  return ret;
}

std::optional<Syntax::PrimaryExpr> Parser::ParsePrimaryExpr() {
  if (Peek(tok::identifier)) {
    std::string identifier = std::get<std::string>(mTokCursor->getValue());
    Consume(tok::identifier);
    return Syntax::PrimaryExpr(Syntax::PrimaryExprIdentifier(identifier));
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
    assert(Match(tok::r_paren));
    return Syntax::PrimaryExpr(Syntax::PrimaryExprParent(std::move(*expr)));
  }
}

bool Parser::IsDeclarationSpecifier() {
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
  case tok::kw_double:
  case tok::kw_signed:
  case tok::kw_unsigned:
  case tok::kw_enum:
  case tok::kw_struct:
  case tok::kw_union:
  case tok::kw_const:
  case tok::kw_restrict:
  case tok::kw_volatile:
  case tok::kw_inline:
    return true;
  case tok::identifier:
    /// todo
    return false;
//    return !mScope.isInScope(std::get<std::string>(mTokCursor->getValue())) &&
//           mScope.isTypedef(std::get<std::string>(mTokCursor->getValue()));
  default:
    return false;
  }
}

bool Parser::IsSpecifierQualifier() {
  switch (mTokCursor->getTokenKind()) {
  case tok::kw_void:
  case tok::kw_char:
  case tok::kw_short:
  case tok::kw_int:
  case tok::kw_long:
  case tok::kw_float:
  case tok::kw_double:
  case tok::kw_signed:
  case tok::kw_unsigned:
  case tok::kw_enum:
  case tok::kw_struct:
  case tok::kw_union:
  case tok::kw_const:
  case tok::kw_restrict:
  case tok::kw_volatile:
    return true;
  case tok::identifier:
    /// todo
    return false;
//    return !mScope.isInScope(std::get<std::string>(mTokCursor->getValue())) &&
//           mScope.isTypedef(std::get<std::string>(mTokCursor->getValue()));
  default:
    return false;
  }
}

bool Parser::IsAssignment(tok::TokenKind type) {
  return type == tok::plus || type == tok::plus_equal ||
         type == tok::minus_equal || type == tok::star_equal ||
         type == tok::slash_equal || type == tok::percent_equal ||
         type == tok::minus_minus || type == tok::greater_greater ||
         type == tok::amp_equal || type == tok::pipe_equal ||
         type == tok::caret_equal;
}

bool Parser::Match(tok::TokenKind tokenType) {
  if (mTokCursor->getTokenKind() == tokenType) {
    ++mTokCursor;
    return true;
  }
  return false;
}

bool Parser::Expect(tok::TokenKind tokenType) {
  if (mTokCursor->getTokenKind() == tokenType)
    return true;
  assert(0);
  return false;
}

bool Parser::Consume(tok::TokenKind tokenType) {
  if (mTokCursor->getTokenKind() == tokenType) {
    ++mTokCursor;
    return true;
  } else {
    assert(0);
    return false;
  }
}
bool Parser::ConsumeAny() {
  ++mTokCursor;
  return true;
}
bool Parser::Peek(tok::TokenKind tokenType) {
  return mTokCursor->getTokenKind() == tokenType;
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

void Parser::Scope::addTypedef(std::string_view name) {
  mCurrentScope.back().emplace(name, Symbol{name, true});
}

bool Parser::Scope::isTypedef(std::string_view name) const {
  for (auto &iter : mCurrentScope) {
    if (auto result = iter.find(name); result != iter.end() && result->second.isTypedef) {
      return true;
    }
  }
  return false;
}

bool Parser::Scope::isTypedefInScope(std::string_view name) const {
  for (auto iter = mCurrentScope.rbegin(); iter != mCurrentScope.rend();
       iter++) {
    if (auto result = iter->find(name); result != iter->end()) {
      return result->second.isTypedef;
    }
  }
  return false;
}

void Parser::Scope::addToScope(std::string_view name) {
  mCurrentScope.back().emplace(name, Symbol{name, true});
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
  return (mFirstStatementSet.find(mTokCursor->getTokenKind()) != mFirstStatementSet.end());
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