/***********************************
 * File:     CToken.h
 *
 * Author:   蔡鹏
 *
 * Email:    iiicp@outlook.com
 *
 * Date:     2022/10/11
 ***********************************/

#ifndef LCC_TOKEN_H
#define LCC_TOKEN_H
#include "SourceInterface.h"
#include "TokenKinds.h"
#include <cstdint>
#include <string>
#include <variant>
namespace lcc{
class TokenBase {
protected:
  bool mLeadingWhiteSpace;
  tok::TokenKind mTokenKind;
  uint32_t mMacroId;
  uint32_t mFileId;
  uint32_t mOffset; // byte offset
  uint32_t mLength;

  TokenBase() = default;

  TokenBase(tok::TokenKind tokenKind, uint32_t offset, uint32_t length,
            uint32_t fileId, uint32_t macroId)
      : mLeadingWhiteSpace(false), mTokenKind(tokenKind), mOffset(offset),
        mLength(length), mFileId(fileId), mMacroId(macroId) {}

public:
  std::string_view getRepresentation(const SourceInterface & sourceObject) const;
  uint32_t getLine(const SourceInterface & sourceObject) const;
  uint32_t getColumn(const SourceInterface & sourceObject) const;

  tok::TokenKind getTokenKind() const {
    return mTokenKind;
  }
  uint32_t getOffset() const {
    return mOffset;
  }
  uint32_t getLength() const {
    return mLength;
  }
  uint32_t getFileId() const {
    return mFileId;
  }
  void setFileId(uint32_t fileId) {
    mFileId = fileId;
  }
  bool hasLeadingWhitespace() const {
    return mLeadingWhiteSpace;
  }
  void setLeadingWhitespace(bool leadingWhitespace) {
    mLeadingWhiteSpace = leadingWhitespace;
  }
  uint32_t getMacroId() const {
    return mMacroId;
  }
  void setMacroId(uint32_t macroId) {
    mMacroId = macroId;
  }
  bool isMacroInserted() const {
    return static_cast<bool>(mMacroId);
  }
};

class PPToken final : public TokenBase {
  std::string mValue;
public:
  PPToken(tok::TokenKind tokenKind, uint32_t offset, uint32_t length,
          uint32_t fileId, uint32_t macroId = 0, std::string_view value = {})
      : TokenBase(tokenKind, offset, length, fileId, macroId),
        mValue(value.begin(), value.end()) {}
  std::string_view getValue() const {
    return mValue;
  }
};

class CToken final : public TokenBase {
private:
  using TokenValue =
      std::variant<std::monostate, int32_t, uint32_t, int64_t, uint64_t, float, double, std::string>;
  TokenValue mValue;

public:
  using ValueType = TokenValue;
  explicit CToken(tok::TokenKind tokenKind, uint32_t offset, uint32_t length,
                  uint32_t fileId, uint32_t macroId, ValueType value = std::monostate{})
      : TokenBase(tokenKind, offset, length, fileId, macroId), mValue(std::move(value)) {}

  const ValueType &getValue() const {
    return mValue;
  }

  void setValue(const ValueType& value) {
    mValue = value;
  }
};

using CTokenIterator = const CToken*;

} // namespace lcc::lexer

#endif // LCC_CTOKEN_H
