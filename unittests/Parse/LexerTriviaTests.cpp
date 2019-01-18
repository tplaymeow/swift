#include "swift/Basic/LangOptions.h"
#include "swift/Basic/SourceManager.h"
#include "swift/Parse/Lexer.h"
#include "swift/Syntax/Trivia.h"
#include "swift/Subsystems.h"
#include "llvm/Support/MemoryBuffer.h"
#include "gtest/gtest.h"

using namespace swift;
using namespace llvm;
using syntax::TriviaKind;

class LexerTriviaTest : public ::testing::Test {};

TEST_F(LexerTriviaTest, RestoreWithTrivia) {
  using namespace swift::syntax;
  StringRef SourceStr = "aaa \n bbb /*C*/ccc";

  LangOptions LangOpts;
  SourceManager SourceMgr;
  unsigned BufferID = SourceMgr.addMemBufferCopy(SourceStr);

  Lexer L(LangOpts, SourceMgr, BufferID, /*Diags=*/nullptr, /*InSILMode=*/false,
          HashbangMode::Disallowed, CommentRetentionMode::AttachToNextToken,
          TriviaRetentionMode::WithTrivia);

  Token Tok;
  ParsedTrivia LeadingTrivia, TrailingTrivia;

  L.lex(Tok, LeadingTrivia, TrailingTrivia);
  ASSERT_EQ(tok::identifier, Tok.getKind());
  ASSERT_EQ("aaa", Tok.getText());
  ASSERT_TRUE(Tok.isAtStartOfLine());
  ASSERT_EQ(LeadingTrivia, ParsedTrivia());
  ASSERT_EQ(TrailingTrivia,
            (ParsedTrivia{{ParsedTriviaPiece(TriviaKind::Space, 1)}}));

  L.lex(Tok, LeadingTrivia, TrailingTrivia);
  ASSERT_EQ(tok::identifier, Tok.getKind());
  ASSERT_EQ("bbb", Tok.getText());
  ASSERT_TRUE(Tok.isAtStartOfLine());
  ASSERT_EQ(LeadingTrivia,
            (ParsedTrivia{{ParsedTriviaPiece(TriviaKind::Newline, 1),
             ParsedTriviaPiece(TriviaKind::Space, 1)}}));
  ASSERT_EQ(TrailingTrivia,
            (ParsedTrivia{{ParsedTriviaPiece(TriviaKind::Space, 1)}}));

  LexerState S = L.getStateForBeginningOfToken(Tok, LeadingTrivia);

  L.lex(Tok, LeadingTrivia, TrailingTrivia);
  ASSERT_EQ(tok::identifier, Tok.getKind());
  ASSERT_EQ("ccc", Tok.getText());
  ASSERT_FALSE(Tok.isAtStartOfLine());
  ASSERT_EQ(LeadingTrivia,
            (ParsedTrivia{{ParsedTriviaPiece(
                             TriviaKind::BlockComment, strlen("/*C*/"))}}));
  ASSERT_EQ(TrailingTrivia, ParsedTrivia());

  L.restoreState(S);
  L.lex(Tok, LeadingTrivia, TrailingTrivia);
  ASSERT_EQ(tok::identifier, Tok.getKind());
  ASSERT_EQ("bbb", Tok.getText());
  ASSERT_TRUE(Tok.isAtStartOfLine());
  ASSERT_EQ(LeadingTrivia,
            (ParsedTrivia{{ParsedTriviaPiece(TriviaKind::Newline, 1),
                           ParsedTriviaPiece(TriviaKind::Space, 1)}}));
  ASSERT_EQ(TrailingTrivia,
            (ParsedTrivia{{ParsedTriviaPiece(TriviaKind::Space, 1)}}));
}

TEST_F(LexerTriviaTest, TriviaHashbang) {
  StringRef SourceStr = "#!/bin/swift\naaa";

  LangOptions LangOpts;
  SourceManager SourceMgr;
  unsigned BufferID = SourceMgr.addMemBufferCopy(SourceStr);

  Lexer L(LangOpts, SourceMgr, BufferID, /*Diags=*/nullptr, /*InSILMode=*/false,
          HashbangMode::Disallowed, CommentRetentionMode::AttachToNextToken,
          TriviaRetentionMode::WithTrivia);

  Token Tok;
  ParsedTrivia LeadingTrivia, TrailingTrivia;
  L.lex(Tok, LeadingTrivia, TrailingTrivia);

  ASSERT_EQ(tok::identifier, Tok.getKind());
  ASSERT_EQ("aaa", Tok.getText());
  ASSERT_TRUE(Tok.isAtStartOfLine());
  ASSERT_EQ(LeadingTrivia, (ParsedTrivia{{
    ParsedTriviaPiece(TriviaKind::GarbageText, strlen("#!/bin/swift")),
    ParsedTriviaPiece(TriviaKind::Newline, 1)}}));
}

TEST_F(LexerTriviaTest, TriviaHashbangAfterBOM) {
  StringRef SourceStr = "\xEF\xBB\xBF" "#!/bin/swift\naaa";

  LangOptions LangOpts;
  SourceManager SourceMgr;
  unsigned BufferID = SourceMgr.addMemBufferCopy(SourceStr);

  Lexer L(LangOpts, SourceMgr, BufferID, /*Diags=*/nullptr, /*InSILMode=*/false,
          HashbangMode::Disallowed, CommentRetentionMode::AttachToNextToken,
          TriviaRetentionMode::WithTrivia);

  Token Tok;
  ParsedTrivia LeadingTrivia, TrailingTrivia;
  L.lex(Tok, LeadingTrivia, TrailingTrivia);

  ASSERT_EQ(tok::identifier, Tok.getKind());
  ASSERT_EQ("aaa", Tok.getText());
  ASSERT_TRUE(Tok.isAtStartOfLine());

  ASSERT_EQ(LeadingTrivia, (ParsedTrivia{{
    ParsedTriviaPiece(TriviaKind::GarbageText, strlen("\xEF\xBB\xBF")),
    ParsedTriviaPiece(TriviaKind::GarbageText, strlen("#!/bin/swift")),
    ParsedTriviaPiece(TriviaKind::Newline, 1)}}));
}

TEST_F(LexerTriviaTest, TriviaConflictMarker) {
  using namespace swift::syntax;
  StringRef SourceStr =
      "aaa\n"
      "<<<<<<< HEAD:conflict_markers.swift\n"
      "new\n"
      "=======\n"
      "old\n"
      ">>>>>>> 18844bc65229786b96b89a9fc7739c0f:conflict_markers.swift\n"
      "bbb\n";

  LangOptions LangOpts;
  SourceManager SourceMgr;
  unsigned BufferID = SourceMgr.addMemBufferCopy(SourceStr);

  Lexer L(LangOpts, SourceMgr, BufferID, /*Diags=*/nullptr, /*InSILMode=*/false,
          HashbangMode::Disallowed, CommentRetentionMode::AttachToNextToken,
          TriviaRetentionMode::WithTrivia);

  Token Tok;
  ParsedTrivia LeadingTrivia, TrailingTrivia;

  L.lex(Tok, LeadingTrivia, TrailingTrivia);
  ASSERT_EQ(tok::identifier, Tok.getKind());
  ASSERT_EQ("aaa", Tok.getText());

  L.lex(Tok, LeadingTrivia, TrailingTrivia);
  ASSERT_EQ(tok::identifier, Tok.getKind());
  ASSERT_EQ("bbb", Tok.getText());
  ASSERT_TRUE(Tok.isAtStartOfLine());
  StringRef expectedTrivia =
      "<<<<<<< HEAD:conflict_markers.swift\n"
      "new\n"
      "=======\n"
      "old\n"
      ">>>>>>> 18844bc65229786b96b89a9fc7739c0f:conflict_markers.swift";
  ASSERT_EQ(LeadingTrivia, (ParsedTrivia{{
    ParsedTriviaPiece(TriviaKind::Newline, 1),
    ParsedTriviaPiece(TriviaKind::GarbageText, expectedTrivia.size()),
    ParsedTriviaPiece(TriviaKind::Newline, 1)}}));
}

TEST_F(LexerTriviaTest, TriviaCarriageReturn) {
  using namespace swift::syntax;
  StringRef SourceStr = "aaa\r\rbbb\r";

  LangOptions LangOpts;
  SourceManager SourceMgr;
  unsigned BufferID = SourceMgr.addMemBufferCopy(SourceStr);

  Lexer L(LangOpts, SourceMgr, BufferID, /*Diags=*/nullptr, /*InSILMode=*/false,
          HashbangMode::Disallowed, CommentRetentionMode::AttachToNextToken,
          TriviaRetentionMode::WithTrivia);

  Token Tok;
  ParsedTrivia LeadingTrivia, TrailingTrivia;

  L.lex(Tok, LeadingTrivia, TrailingTrivia);
  ASSERT_EQ(tok::identifier, Tok.getKind());
  ASSERT_EQ("aaa", Tok.getText());
  ASSERT_TRUE(Tok.isAtStartOfLine());
  ASSERT_EQ(LeadingTrivia, ParsedTrivia());
  ASSERT_EQ(TrailingTrivia, ParsedTrivia());

  L.lex(Tok, LeadingTrivia, TrailingTrivia);
  ASSERT_EQ(tok::identifier, Tok.getKind());
  ASSERT_EQ("bbb", Tok.getText());
  ASSERT_TRUE(Tok.isAtStartOfLine());
  ASSERT_EQ(LeadingTrivia,
            (ParsedTrivia{{ParsedTriviaPiece(TriviaKind::CarriageReturn, 2)}}));
  ASSERT_EQ(TrailingTrivia, ParsedTrivia());

  L.lex(Tok, LeadingTrivia, TrailingTrivia);
  ASSERT_EQ(tok::eof, Tok.getKind());
  ASSERT_TRUE(Tok.isAtStartOfLine());
  ASSERT_EQ(LeadingTrivia,
            (ParsedTrivia{{ParsedTriviaPiece(TriviaKind::CarriageReturn, 1)}}));
  ASSERT_EQ(TrailingTrivia, ParsedTrivia());
}

TEST_F(LexerTriviaTest, TriviaNewLines) {
  using namespace swift::syntax;
  StringRef SourceStr = "\n\r\r\n\r\r\r\n\r\n\n\n"
                        "aaa"
                        "\n\r\r\n\r\r\r\n\r\n\n\n"
                        "bbb"
                        "\n\r\r\n\r\r\r\n\r\n\n\n";

  LangOptions LangOpts;
  SourceManager SourceMgr;
  unsigned BufferID = SourceMgr.addMemBufferCopy(SourceStr);

  Lexer L(LangOpts, SourceMgr, BufferID, /*Diags=*/nullptr, /*InSILMode=*/false,
          HashbangMode::Disallowed, CommentRetentionMode::AttachToNextToken,
          TriviaRetentionMode::WithTrivia);

  Token Tok;
  ParsedTrivia LeadingTrivia, TrailingTrivia;

  L.lex(Tok, LeadingTrivia, TrailingTrivia);
  ASSERT_EQ(tok::identifier, Tok.getKind());
  ASSERT_EQ("aaa", Tok.getText());
  ASSERT_TRUE(Tok.isAtStartOfLine());
  ASSERT_EQ((ParsedTrivia{{
    ParsedTriviaPiece(TriviaKind::Newline, 1),
    ParsedTriviaPiece(TriviaKind::CarriageReturn, 1),
    ParsedTriviaPiece(TriviaKind::CarriageReturnLineFeed, 2),
    ParsedTriviaPiece(TriviaKind::CarriageReturn, 2),
    ParsedTriviaPiece(TriviaKind::CarriageReturnLineFeed, 4),
    ParsedTriviaPiece(TriviaKind::Newline, 2),
  }}), LeadingTrivia);
  ASSERT_EQ(ParsedTrivia(), TrailingTrivia);

  L.lex(Tok, LeadingTrivia, TrailingTrivia);
  ASSERT_EQ(tok::identifier, Tok.getKind());
  ASSERT_EQ("bbb", Tok.getText());
  ASSERT_TRUE(Tok.isAtStartOfLine());
  ASSERT_EQ((ParsedTrivia{{
    ParsedTriviaPiece(TriviaKind::Newline, 1),
    ParsedTriviaPiece(TriviaKind::CarriageReturn, 1),
    ParsedTriviaPiece(TriviaKind::CarriageReturnLineFeed, 2),
    ParsedTriviaPiece(TriviaKind::CarriageReturn, 2),
    ParsedTriviaPiece(TriviaKind::CarriageReturnLineFeed, 4),
    ParsedTriviaPiece(TriviaKind::Newline, 2),
  }}), LeadingTrivia);
  ASSERT_EQ(ParsedTrivia(), TrailingTrivia);

  L.lex(Tok, LeadingTrivia, TrailingTrivia);
  ASSERT_EQ(tok::eof, Tok.getKind());
  ASSERT_TRUE(Tok.isAtStartOfLine());
  ASSERT_EQ((ParsedTrivia{{
    ParsedTriviaPiece(TriviaKind::Newline, 1),
    ParsedTriviaPiece(TriviaKind::CarriageReturn, 1),
    ParsedTriviaPiece(TriviaKind::CarriageReturnLineFeed, 2),
    ParsedTriviaPiece(TriviaKind::CarriageReturn, 2),
    ParsedTriviaPiece(TriviaKind::CarriageReturnLineFeed, 4),
    ParsedTriviaPiece(TriviaKind::Newline, 2),
  }}), LeadingTrivia);
  ASSERT_EQ(ParsedTrivia(), TrailingTrivia);
}
