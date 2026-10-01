// Tests of the syntax modes: file extension detection, the Python/XML/JSON scanners
// and the block states kept by STCSyntaxHighlighter (what lets a construct span many lines).
#include <gtest/gtest.h>

#include <QApplication>
#include <QColor>
#include <QPlainTextDocumentLayout>
#include <QSyntaxStyle>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <vector>

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>

#include "utils/CodeLanguageHighlighting.h"
#include "utils/LinkDetection.h"
#include "utils/STCSyntaxHighlighter.h"
#include "utils/SyntaxMode.h"
#include "utils/SyntaxModeMemory.h"

namespace
{
struct Span
{
    int start;
    int length;
    QColor color;

    bool operator==(const Span& other) const
    {
        return start == other.start && length == other.length && color == other.color;
    }
};

// NOTE: in QCodeEditor's default style some names share a color ("String" and "Comment", "Keyword"
// and "PrimitiveType", "Number" and "Preprocessor"), so a test can't tell those apart - it checks
// the range and the color group, which is enough to see what is and what is not highlighted.
QColor styleColor(const char* name)
{
    return QSyntaxStyle::defaultStyle()->getFormat(name).foreground().color();
}

Span span(int start, int length, const char* styleName)
{
    return { start, length, styleColor(styleName) };
}

std::ostream& operator<<(std::ostream& os, const Span& s)
{
    return os << "{" << s.start << "," << s.length << "," << s.color.name().toStdString() << "}";
}

struct Recorder
{
    std::vector<Span> spans;

    stc::codehl::FormatSetter setter()
    {
        return [this](int start, int length, const QTextCharFormat& format) {
            spans.push_back({ start, length, format.foreground().color() });
        };
    }
};

std::vector<Span> pythonSpans(const QString& text, int stateIn = 0, int* stateOut = nullptr)
{
    Recorder recorder;
    const int state = stc::codehl::highlightPython(text, 0, text.length(), stateIn, recorder.setter());
    if (stateOut)
        *stateOut = state;
    return recorder.spans;
}

std::vector<Span> xmlSpans(const QString& text, int stateIn = 0, int* stateOut = nullptr)
{
    Recorder recorder;
    const int state = stc::codehl::highlightXml(text, 0, text.length(), stateIn, recorder.setter());
    if (stateOut)
        *stateOut = state;
    return recorder.spans;
}

std::vector<Span> jsonSpans(const QString& text)
{
    Recorder recorder;
    stc::codehl::highlightJson(text, 0, text.length(), recorder.setter());
    return recorder.spans;
}

/// The formats which STCSyntaxHighlighter put on a block (only the ones with a color).
std::vector<Span> blockSpans(const QTextBlock& block)
{
    std::vector<Span> result;
    for (const auto& range : block.layout()->formats())
    {
        if (range.format.foreground().style() != Qt::NoBrush)
            result.push_back({ range.start, range.length, range.format.foreground().color() });
    }
    return result;
}

/// True if any character of `[start, start+length)` of the block got the given color.
bool blockHasColor(const QTextBlock& block, const char* styleName)
{
    const QColor expected = styleColor(styleName);
    for (const auto& s : blockSpans(block))
        if (s.color == expected)
            return true;
    return false;
}

QTextBlock blockAt(const QTextDocument& document, int number)
{
    return document.findBlockByNumber(number);
}

struct HighlightedDocument
{
    QTextDocument document;
    STCSyntaxHighlighter* highlighter;

    explicit HighlightedDocument(const QString& text, SyntaxMode mode = SyntaxMode::Stc)
    {
        // The document of CodeEditor (a QPlainTextEdit) has this layout; without it QSyntaxHighlighter
        // does not react to the edits, so the "edit and see the next lines change" tests would be meaningless.
        document.setDocumentLayout(new QPlainTextDocumentLayout(&document));
        document.setPlainText(text);
        highlighter = new STCSyntaxHighlighter(&document); // owned by the document
        highlighter->setSyntaxMode(mode, false);
        highlighter->rehighlight();
    }
};
} // namespace

// ------------------------------------------------------------------ SyntaxMode

TEST(SyntaxModeDetection, RecognizesKnownExtensions)
{
    using syntaxmode::detectFromFileName;
    EXPECT_EQ(detectFromFileName("script.py"), SyntaxMode::Python);
    EXPECT_EQ(detectFromFileName("main.cpp"), SyntaxMode::Cpp);
    EXPECT_EQ(detectFromFileName("header.hpp"), SyntaxMode::Cpp);
    EXPECT_EQ(detectFromFileName("old.h"), SyntaxMode::Cpp);
    EXPECT_EQ(detectFromFileName("data.xml"), SyntaxMode::Xml);
    EXPECT_EQ(detectFromFileName("form.ui"), SyntaxMode::Xml);
    EXPECT_EQ(detectFromFileName("data.json"), SyntaxMode::Json);
}

TEST(SyntaxModeDetection, IsCaseInsensitiveAndIgnoresDirectories)
{
    using syntaxmode::detectFromFileName;
    EXPECT_EQ(detectFromFileName("/home/user/projects.v2/SCRIPT.PY"), SyntaxMode::Python);
    EXPECT_EQ(detectFromFileName("C:\\dir.with.dots\\Main.CPP"), SyntaxMode::Cpp);
}

TEST(SyntaxModeDetection, MarkdownAndProseAreAPlainText)
{
    using syntaxmode::detectFromFileName;
    EXPECT_EQ(detectFromFileName("README.md"), SyntaxMode::PlainText);
    EXPECT_EQ(detectFromFileName("notes.text"), SyntaxMode::PlainText);
}

TEST(SyntaxModeDetection, TxtIsNotDetected_ItIsWhatTheStcArticlesAreSavedAs)
{
    // otherwise opening an article would turn its tags off
    EXPECT_FALSE(syntaxmode::detectFromFileName("article.txt").has_value());
}

TEST(SyntaxModeDetection, UnknownExtensionsGiveNothing)
{
    using syntaxmode::detectFromFileName;
    EXPECT_FALSE(detectFromFileName("notes.txt").has_value());
    EXPECT_FALSE(detectFromFileName("Makefile").has_value());
    EXPECT_FALSE(detectFromFileName("archive.tar.gz").has_value());
    EXPECT_FALSE(detectFromFileName("translation.ts").has_value()); // Qt translation XML or TypeScript?
    EXPECT_FALSE(detectFromFileName("").has_value());
}

// ------------------------------------------------------------------ Python scanner

TEST(PythonScanner, KeywordsFunctionsAndClasses)
{
    const auto spans = pythonSpans("def greet(name):");
    ASSERT_EQ(spans.size(), 2u) << "`name` is not a keyword nor a call";
    EXPECT_EQ(spans[0], span(0, 3, "Keyword"));   // def
    EXPECT_EQ(spans[1], span(4, 5, "Function"));  // greet

    const auto classSpans = pythonSpans("class Foo(Base):");
    ASSERT_EQ(classSpans.size(), 2u);
    EXPECT_EQ(classSpans[0], span(0, 5, "Keyword"));
    EXPECT_EQ(classSpans[1], span(6, 3, "Type")); // Foo
}

TEST(PythonScanner, KeywordsMissingInQCodeEditorsPythonXmlAreCovered)
{
    for (const char* word : { "try", "except", "with", "lambda", "yield", "from", "as", "pass", "raise" })
    {
        const auto spans = pythonSpans(word);
        ASSERT_EQ(spans.size(), 1u) << word;
        EXPECT_EQ(spans[0], span(0, static_cast<int>(strlen(word)), "Keyword")) << word;
    }
}

TEST(PythonScanner, AttributeNamedLikeKeywordIsNotAKeyword)
{
    EXPECT_TRUE(pythonSpans("obj.print").empty());
    EXPECT_TRUE(pythonSpans("self.class_").empty());
}

TEST(PythonScanner, CallsAreHighlightedAsFunctions)
{
    const auto spans = pythonSpans("compute (x)");
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0], span(0, 7, "Function"));
}

TEST(PythonScanner, CommentIsNotStartedInsideAString)
{
    // exactly one span: the string, nothing like a comment after the `#`
    const auto spans = pythonSpans("x = \"a#b\"");
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0], span(4, 5, "String"));
}

TEST(PythonScanner, CommentRunsToTheEndOfTheLine)
{
    const auto spans = pythonSpans("x = 1  # def class 'x");
    ASSERT_EQ(spans.size(), 2u);
    EXPECT_EQ(spans[0], span(4, 1, "Number"));
    EXPECT_EQ(spans[1], span(7, 14, "Comment"));
}

TEST(PythonScanner, EscapedQuoteDoesNotEndTheString)
{
    const QString text = R"(s = 'it\'s' + "a\"b")";
    const auto spans = pythonSpans(text);
    ASSERT_EQ(spans.size(), 2u);
    EXPECT_EQ(spans[0], span(4, 7, "String"));
    EXPECT_EQ(spans[1], span(14, 6, "String"));
}

TEST(PythonScanner, StringPrefixesAreIncludedInTheString)
{
    const auto spans = pythonSpans(R"(f"x" rb'y')");
    ASSERT_EQ(spans.size(), 2u);
    EXPECT_EQ(spans[0], span(0, 4, "String"));
    EXPECT_EQ(spans[1], span(5, 5, "String"));
}

TEST(PythonScanner, IdentifierEndingWithPrefixLetterIsNotAString)
{
    // `bar"` would be an `r` prefix if the identifier was not taken as a whole
    const auto spans = pythonSpans("bar = 1");
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0], span(6, 1, "Number"));
}

TEST(PythonScanner, Numbers)
{
    const auto spans = pythonSpans("0x1F + 3.14e-2 + 1_000 + 2j");
    ASSERT_EQ(spans.size(), 4u);
    EXPECT_EQ(spans[0], span(0, 4, "Number"));
    EXPECT_EQ(spans[1], span(7, 7, "Number"));
    EXPECT_EQ(spans[2], span(17, 5, "Number"));
    EXPECT_EQ(spans[3], span(25, 2, "Number"));
}

TEST(PythonScanner, DecoratorOnlyAtTheBeginningOfTheStatement)
{
    const auto decorator = pythonSpans("@app.route");
    ASSERT_EQ(decorator.size(), 1u);
    EXPECT_EQ(decorator[0], span(0, 10, "Preprocessor"));

    EXPECT_TRUE(pythonSpans("a @ b").empty()) << "matrix multiplication is not a decorator";
}

TEST(PythonScanner, TripleQuotedStringOnOneLine)
{
    int state = -1;
    const auto spans = pythonSpans(R"(x = """doc""" + 1)", 0, &state);
    ASSERT_EQ(spans.size(), 2u);
    EXPECT_EQ(spans[0], span(4, 9, "String"));
    EXPECT_EQ(spans[1], span(16, 1, "Number"));
    EXPECT_EQ(state, stc::codehl::PY_STATE_NONE);
}

TEST(PythonScanner, TripleQuotedStringContinuesOnNextLines)
{
    int state = -1;
    auto spans = pythonSpans(R"(x = """first # not a comment)", 0, &state);
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(state, stc::codehl::PY_STATE_TRIPLE_DOUBLE);

    // a middle line is entirely the string, with the keywords inside of it left alone
    spans = pythonSpans("def class", state, &state);
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0], span(0, 9, "String"));
    EXPECT_EQ(state, stc::codehl::PY_STATE_TRIPLE_DOUBLE);

    // the closing line: string up to the `"""`, normal code after it
    spans = pythonSpans(R"(tail""" + 5)", state, &state);
    ASSERT_EQ(spans.size(), 2u);
    EXPECT_EQ(spans[0], span(0, 7, "String"));
    EXPECT_EQ(spans[1], span(10, 1, "Number"));
    EXPECT_EQ(state, stc::codehl::PY_STATE_NONE);
}

TEST(PythonScanner, SingleQuoteTripleStringIsNotClosedByDoubleQuotes)
{
    int state = -1;
    pythonSpans("x = '''abc", 0, &state);
    EXPECT_EQ(state, stc::codehl::PY_STATE_TRIPLE_SINGLE);

    pythonSpans(R"(still """ inside)", state, &state);
    EXPECT_EQ(state, stc::codehl::PY_STATE_TRIPLE_SINGLE);

    pythonSpans("end'''", state, &state);
    EXPECT_EQ(state, stc::codehl::PY_STATE_NONE);
}

TEST(PythonScanner, EmptyLineInsideTripleQuotedStringKeepsTheState)
{
    int state = -1;
    pythonSpans("", stc::codehl::PY_STATE_TRIPLE_DOUBLE, &state);
    EXPECT_EQ(state, stc::codehl::PY_STATE_TRIPLE_DOUBLE);
}

TEST(PythonScanner, UnclosedSingleLineStringStopsAtTheEndOfTheLine)
{
    int state = -1;
    const auto spans = pythonSpans("x = 'abc", 0, &state);
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0], span(4, 4, "String"));
    EXPECT_EQ(state, stc::codehl::PY_STATE_NONE) << "a plain string does not continue on the next line";
}

TEST(PythonScanner, HighlightsOnlyTheRequestedFragment)
{
    const QString text = "print(1) [py]x = 2[/py] def";
    Recorder recorder;
    // only `x = 2`, like the content of an inline [py] tag
    stc::codehl::highlightPython(text, 13, 18, 0, recorder.setter());
    ASSERT_EQ(recorder.spans.size(), 1u);
    EXPECT_EQ(recorder.spans[0], span(17, 1, "Number"));
}

// ------------------------------------------------------------------ XML scanner

TEST(XmlScanner, ElementWithAttributes)
{
    int state = -1;
    const auto spans = xmlSpans(R"(<item name="x" id='7'>text</item>)", 0, &state);

    // <item | name | "x" | id | '7' | > | </ + item | >
    ASSERT_GE(spans.size(), 8u);
    EXPECT_EQ(spans[0], span(0, 1, "Keyword"));   // <
    EXPECT_EQ(spans[1], span(1, 4, "Keyword"));   // item
    EXPECT_EQ(spans[2], span(6, 4, "Function"));  // name
    EXPECT_EQ(spans[3], span(11, 3, "String"));   // "x"
    EXPECT_EQ(spans[4], span(15, 2, "Function")); // id
    EXPECT_EQ(spans[5], span(18, 3, "String"));   // '7'
    EXPECT_EQ(spans[6], span(21, 1, "Keyword"));  // >
    EXPECT_EQ(state, stc::codehl::XML_STATE_NONE);

    for (const auto& s : spans)
        EXPECT_FALSE(s.start >= 22 && s.start < 26) << "the text content must stay unhighlighted";
}

TEST(XmlScanner, CommentSpansManyLines)
{
    int state = -1;
    xmlSpans("<a/> <!-- start", 0, &state);
    EXPECT_EQ(state, stc::codehl::XML_STATE_COMMENT);

    auto spans = xmlSpans("<b>not a tag</b>", state, &state);
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0], span(0, 16, "Comment"));
    EXPECT_EQ(state, stc::codehl::XML_STATE_COMMENT);

    spans = xmlSpans("end --> <c/>", state, &state);
    ASSERT_GE(spans.size(), 2u);
    EXPECT_EQ(spans[0], span(0, 7, "Comment"));
    EXPECT_EQ(state, stc::codehl::XML_STATE_NONE);
}

TEST(XmlScanner, TagWithAttributesOnSeveralLines)
{
    int state = -1;
    xmlSpans("<item", 0, &state);
    EXPECT_EQ(state, stc::codehl::XML_STATE_IN_TAG);

    const auto spans = xmlSpans(R"(   name="multi" disabled>text)", state, &state);
    EXPECT_EQ(state, stc::codehl::XML_STATE_NONE);
    ASSERT_GE(spans.size(), 4u);
    EXPECT_EQ(spans[0], span(3, 4, "Function"));  // name
    EXPECT_EQ(spans[1], span(8, 7, "String"));    // "multi"
    EXPECT_EQ(spans[2], span(16, 8, "Function")); // disabled
    EXPECT_EQ(spans[3], span(24, 1, "Keyword"));  // >
}

TEST(XmlScanner, SelfClosingAndProcessingInstruction)
{
    int state = -1;
    auto spans = xmlSpans("<empty/>", 0, &state);
    EXPECT_EQ(state, stc::codehl::XML_STATE_NONE);
    EXPECT_EQ(spans.back(), span(6, 2, "Keyword")); // />

    xmlSpans(R"(<?xml version="1.0"?>)", 0, &state);
    EXPECT_EQ(state, stc::codehl::XML_STATE_NONE);
}

// ------------------------------------------------------------------ JSON scanner

TEST(JsonScanner, KeysAndValuesHaveDifferentColors)
{
    const auto spans = jsonSpans(R"(  "name": "STC", "n": -12.5e3, "ok": true, "x": null)");

    EXPECT_EQ(spans[0], span(2, 6, "Type"));    // "name"
    EXPECT_EQ(spans[1], span(10, 5, "String")); // "STC"
    EXPECT_EQ(spans[2], span(17, 3, "Type"));   // "n"
    EXPECT_EQ(spans[3], span(22, 7, "Number")); // -12.5e3
    EXPECT_EQ(spans[4], span(31, 4, "Type"));   // "ok"
    EXPECT_EQ(spans[5], span(37, 4, "Keyword")); // true
    EXPECT_EQ(spans[6], span(43, 3, "Type"));   // "x"
    EXPECT_EQ(spans[7], span(48, 4, "Keyword")); // null
    EXPECT_EQ(spans.size(), 8u);
}

TEST(JsonScanner, EscapedQuoteInAString)
{
    const auto spans = jsonSpans(R"("a \"quoted\" word": 1)");
    ASSERT_EQ(spans.size(), 2u);
    EXPECT_EQ(spans[0], span(0, 19, "Type"));
    EXPECT_EQ(spans[1], span(21, 1, "Number"));
}

TEST(JsonScanner, WordsInsideOtherTokensAreNotKeywords)
{
    // `nullable` must not be cut into `null` + `able`
    EXPECT_TRUE(jsonSpans("nullable").empty());
}

// ------------------------------------------------------------------ STCSyntaxHighlighter: states

TEST(HighlighterStates, PythonDocstringInsideStcTagContinuesAndEndsWithTheTag)
{
    HighlightedDocument d("[py]\n"
                          "def f():\n"
                          "    \"\"\"doc\n"
                          "    def class\n"
                          "    \"\"\"\n"
                          "    return 1\n"
                          "[/py]\n"
                          "def not_highlighted");

    // `def class` inside of the docstring is a string, not keywords: one span, the whole line
    const auto docstringLine = blockSpans(blockAt(d.document, 3));
    ASSERT_EQ(docstringLine.size(), 1u);
    EXPECT_EQ(docstringLine[0], span(0, 13, "String"));

    // after the docstring it is a code again
    EXPECT_TRUE(blockHasColor(blockAt(d.document, 5), "Keyword")); // return

    // text after [/py] is not Python
    EXPECT_TRUE(blockSpans(blockAt(d.document, 7)).empty());
    EXPECT_EQ(blockAt(d.document, 6).userState(), -1) << "[/py] has to reset the state";
}

TEST(HighlighterStates, UnclosedDocstringDoesNotLeakPastTheClosingTag)
{
    HighlightedDocument d("[py]\n"
                          "x = \"\"\"never closed\n"
                          "[/py]\n"
                          "[b]bold[/b]\n"
                          "plain");
    const auto after = blockAt(d.document, 4);
    EXPECT_EQ(after.userState(), -1);
    EXPECT_TRUE(blockSpans(after).empty());
}

TEST(HighlighterStates, InlinePythonTag)
{
    HighlightedDocument d("text [py]print(\"a#b\", None)[/py] more");
    const auto spans = blockSpans(blockAt(d.document, 0));

    bool hasFunction = false, hasString = false;
    for (const auto& s : spans)
    {
        hasFunction |= (s.color == styleColor("Function") && s.start == 9 && s.length == 5);
        hasString |= (s.color == styleColor("String") && s.start == 15 && s.length == 5);
    }
    EXPECT_TRUE(hasFunction) << "print";
    EXPECT_TRUE(hasString) << "\"a#b\"";
}

TEST(HighlighterStates, CppBlockCommentSpansLinesInsideStcTag)
{
    // a regression: the "inside of a comment" state used to be overwritten right after being set
    HighlightedDocument d("[cpp]\n"
                          "/* start\n"
                          "\n"
                          "   int x; end */\n"
                          "int y;\n"
                          "[/cpp]");

    EXPECT_TRUE(blockHasColor(blockAt(d.document, 1), "Comment"));
    const auto lastCommentLine = blockSpans(blockAt(d.document, 3)); // `   int x; end */`: all of it is the comment
    ASSERT_EQ(lastCommentLine.size(), 1u);
    EXPECT_EQ(lastCommentLine[0], span(0, 16, "Comment"));

    const auto last = blockSpans(blockAt(d.document, 4)); // `int y;` is a code again
    ASSERT_FALSE(last.empty());
    EXPECT_EQ(last[0], span(0, 3, "Keyword"));
}

// ------------------------------------------------------------------ STCSyntaxHighlighter: whole-file modes

TEST(HighlighterModes, WholeFileAsPython)
{
    HighlightedDocument d("import os\n"
                          "def f():\n"
                          "    '''doc\n"
                          "    more'''\n"
                          "    return [b]\n",
                          SyntaxMode::Python);

    EXPECT_TRUE(blockHasColor(blockAt(d.document, 0), "Keyword"));
    const auto docLine = blockSpans(blockAt(d.document, 3));
    ASSERT_FALSE(docLine.empty());
    EXPECT_EQ(docLine[0].color, styleColor("String"));
    EXPECT_TRUE(blockHasColor(blockAt(d.document, 4), "Keyword")); // return
}

TEST(HighlighterModes, StcTagsAreNotInterpretedInASourceFile)
{
    // `[b]` is an index expression in Python, not a bold tag: it must not get the gray, small tag format
    HighlightedDocument d("x = a[b]\n", SyntaxMode::Python);
    for (const auto& s : blockSpans(blockAt(d.document, 0)))
        EXPECT_NE(s.color, QColor(Qt::gray));
}

TEST(HighlighterModes, WholeFileAsJsonAndXml)
{
    HighlightedDocument json("{\"k\": [1, true]}\n", SyntaxMode::Json);
    EXPECT_TRUE(blockHasColor(blockAt(json.document, 0), "Type"));
    EXPECT_TRUE(blockHasColor(blockAt(json.document, 0), "Number"));

    HighlightedDocument xml("<!-- multi\n"
                            "line -->\n"
                            "<a b=\"c\"/>\n",
                            SyntaxMode::Xml);
    EXPECT_TRUE(blockHasColor(blockAt(xml.document, 1), "Comment"));
    EXPECT_TRUE(blockHasColor(blockAt(xml.document, 2), "String"));
}

TEST(HighlighterModes, CppBlockCommentSpansLinesInASourceFile)
{
    HighlightedDocument d("/* a\n"
                          "int x;\n"
                          "*/ int y;\n",
                          SyntaxMode::Cpp);
    EXPECT_TRUE(blockHasColor(blockAt(d.document, 1), "Comment"));
    EXPECT_TRUE(blockHasColor(blockAt(d.document, 2), "Keyword")); // `int` after the comment ends
}

TEST(HighlighterModes, SwitchingTheModeRehighlightsTheDocument)
{
    HighlightedDocument d("def f(): pass\n");
    EXPECT_TRUE(blockSpans(blockAt(d.document, 0)).empty()) << "STC mode: plain text is not a Python";

    d.highlighter->setSyntaxMode(SyntaxMode::Python);
    EXPECT_TRUE(blockHasColor(blockAt(d.document, 0), "Keyword"));

    d.highlighter->setSyntaxMode(SyntaxMode::Stc);
    EXPECT_TRUE(blockSpans(blockAt(d.document, 0)).empty());
}

TEST(HighlighterModes, EditingARestOfTheFileUpdatesFollowingLines)
{
    HighlightedDocument d("x = 1\n"
                          "y = 2\n",
                          SyntaxMode::Python);
    EXPECT_FALSE(blockHasColor(blockAt(d.document, 1), "String"));

    // opening a docstring on the first line turns the second one into a part of it
    QTextCursor cursor(&d.document);
    cursor.setPosition(4);
    cursor.insertText("\"\"\"");
    EXPECT_TRUE(blockHasColor(blockAt(d.document, 1), "String"));

    // ...and removing it gets the second line back
    cursor.setPosition(4);
    cursor.setPosition(7, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    EXPECT_FALSE(blockHasColor(blockAt(d.document, 1), "String"));
}

// ------------------------------------------------------------------ SyntaxMode: names, predicates, wildcards

TEST(SyntaxModeNames, EveryModeSurvivesAConversionToTextAndBack)
{
    for (auto mode : { SyntaxMode::Stc, SyntaxMode::PlainText, SyntaxMode::Cpp, SyntaxMode::Python, SyntaxMode::Xml,
                       SyntaxMode::Json })
    {
        const auto back = syntaxmode::fromString(syntaxmode::toString(mode));
        ASSERT_TRUE(back.has_value());
        EXPECT_EQ(*back, mode);
    }
    EXPECT_FALSE(syntaxmode::fromString("klingon").has_value());
    EXPECT_FALSE(syntaxmode::fromString("").has_value());
}

TEST(SyntaxModePredicates, PlainTextIsNeitherSourceCodeNorStc)
{
    EXPECT_TRUE(syntaxmode::usesStcMarkup(SyntaxMode::Stc));
    EXPECT_FALSE(syntaxmode::usesStcMarkup(SyntaxMode::PlainText));
    EXPECT_FALSE(syntaxmode::isSourceCodeMode(SyntaxMode::PlainText)); // keeps the font and the spell checking
    EXPECT_FALSE(syntaxmode::isSourceCodeMode(SyntaxMode::Stc));
    EXPECT_TRUE(syntaxmode::isSourceCodeMode(SyntaxMode::Python));
}

TEST(SyntaxModeWildcards, ListSourceFilesButNeitherTxtNorProse)
{
    const auto wildcards = syntaxmode::knownSourceFileWildcards();
    EXPECT_TRUE(wildcards.contains("*.py"));
    EXPECT_TRUE(wildcards.contains("*.json"));
    EXPECT_TRUE(wildcards.contains("*.hpp"));
    EXPECT_FALSE(wildcards.contains("*.txt"));
    EXPECT_FALSE(wildcards.contains("*.md"));
}

// ------------------------------------------------------------------ remembered choice

class SyntaxModeMemoryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // a settings file of its own, so the tests never touch the real configuration of the user
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());
        QCoreApplication::setOrganizationName("STC_editor_tests");
        QCoreApplication::setApplicationName("syntax_memory_tests");
        QSettings().clear();
    }

    QTemporaryDir tempDir;
};

TEST_F(SyntaxModeMemoryTest, NothingIsRememberedAtFirst)
{
    EXPECT_FALSE(syntaxmode::memory::recall("/tmp/notes.txt").has_value());
    EXPECT_FALSE(syntaxmode::memory::recall("").has_value());
}

TEST_F(SyntaxModeMemoryTest, RemembersTheChoiceForAFile)
{
    syntaxmode::memory::remember("/tmp/notes.txt", SyntaxMode::PlainText);
    EXPECT_EQ(syntaxmode::memory::recall("/tmp/notes.txt"), SyntaxMode::PlainText);
    EXPECT_FALSE(syntaxmode::memory::recall("/tmp/other.txt").has_value()) << "it is a per-file choice";
}

TEST_F(SyntaxModeMemoryTest, TheNewestChoiceWinsAndForgettingRemovesIt)
{
    syntaxmode::memory::remember("/tmp/a.txt", SyntaxMode::Python);
    syntaxmode::memory::remember("/tmp/a.txt", SyntaxMode::Json);
    EXPECT_EQ(syntaxmode::memory::recall("/tmp/a.txt"), SyntaxMode::Json);

    syntaxmode::memory::forget("/tmp/a.txt");
    EXPECT_FALSE(syntaxmode::memory::recall("/tmp/a.txt").has_value());
}

TEST_F(SyntaxModeMemoryTest, PathsWithTheSeparatorCharacterWork)
{
    syntaxmode::memory::remember("/tmp/we|rd name.txt", SyntaxMode::PlainText);
    EXPECT_EQ(syntaxmode::memory::recall("/tmp/we|rd name.txt"), SyntaxMode::PlainText);
}

TEST_F(SyntaxModeMemoryTest, OldestEntriesAreDroppedWhenThereAreTooMany)
{
    for (int i = 0; i < 600; ++i)
        syntaxmode::memory::remember(QString("/tmp/file%1.txt").arg(i), SyntaxMode::PlainText);

    EXPECT_FALSE(syntaxmode::memory::recall("/tmp/file0.txt").has_value());
    EXPECT_TRUE(syntaxmode::memory::recall("/tmp/file599.txt").has_value());
}

// ------------------------------------------------------------------ links and e-mail addresses

namespace
{
using stc::links::LinkKind;
using stc::links::LinkSpan;

std::vector<QString> linkTexts(const QString& text, LinkKind kind)
{
    std::vector<QString> result;
    for (const auto& link : stc::links::findLinks(text))
        if (link.kind == kind)
            result.push_back(text.mid(link.start, link.length));
    return result;
}
} // namespace

TEST(LinkDetection, FindsWebAddresses)
{
    const QString text = "Zobacz https://cpp0x.pl/kurs/x?a=1&b=2#frag oraz http://example.com i ftp://files.example.org/a.zip tam.";
    const auto urls = linkTexts(text, LinkKind::Url);
    ASSERT_EQ(urls.size(), 3u);
    EXPECT_EQ(urls[0], "https://cpp0x.pl/kurs/x?a=1&b=2#frag");
    EXPECT_EQ(urls[1], "http://example.com");
    EXPECT_EQ(urls[2], "ftp://files.example.org/a.zip");
}

TEST(LinkDetection, WwwWithoutASchemeIsAnAddress)
{
    const auto urls = linkTexts("strona www.cpp0x.pl, zapraszam", LinkKind::Url);
    ASSERT_EQ(urls.size(), 1u);
    EXPECT_EQ(urls[0], "www.cpp0x.pl");
}

TEST(LinkDetection, PunctuationAroundAnAddressIsNotPartOfIt)
{
    EXPECT_EQ(linkTexts("(zob. https://example.com/a).", LinkKind::Url), std::vector<QString>{ "https://example.com/a" });
    EXPECT_EQ(linkTexts("\"https://example.com\"", LinkKind::Url), std::vector<QString>{ "https://example.com" });
    EXPECT_EQ(linkTexts("Czy to https://example.com/x?!", LinkKind::Url), std::vector<QString>{ "https://example.com/x" });
    EXPECT_EQ(linkTexts("<https://example.com/x>", LinkKind::Url), std::vector<QString>{ "https://example.com/x" });
}

TEST(LinkDetection, ClosingParenthesisOfTheAddressIsKept)
{
    EXPECT_EQ(linkTexts("https://pl.wikipedia.org/wiki/C_(język)", LinkKind::Url),
              std::vector<QString>{ "https://pl.wikipedia.org/wiki/C_(język)" });
    EXPECT_EQ(linkTexts("(https://pl.wikipedia.org/wiki/C_(język))", LinkKind::Url),
              std::vector<QString>{ "https://pl.wikipedia.org/wiki/C_(język)" });
}

TEST(LinkDetection, ASchemeOrWwwAloneIsNotAnAddress)
{
    EXPECT_TRUE(stc::links::findLinks("ftp:// oraz www. oraz https://.").isEmpty());
}

TEST(LinkDetection, SchemeInsideAWordIsNotAnAddress)
{
    EXPECT_TRUE(stc::links::findLinks("xhttp://example.com").isEmpty());
}

TEST(LinkDetection, FindsEmailAddresses)
{
    const auto emails = linkTexts("Pisz do jan.kowalski+stc@agh.edu.pl lub mailto:biuro@example.com, dzięki.", LinkKind::Email);
    ASSERT_EQ(emails.size(), 2u);
    EXPECT_EQ(emails[0], "jan.kowalski+stc@agh.edu.pl");
    EXPECT_EQ(emails[1], "mailto:biuro@example.com");
}

TEST(LinkDetection, NotEverythingWithAnAtSignIsAnEmail)
{
    EXPECT_TRUE(stc::links::findLinks("@decorator oraz a @ b oraz user@localhost oraz @mention").isEmpty());
}

TEST(LinkDetection, EmailInsideOfAWebAddressIsNotReportedTwice)
{
    const auto links = stc::links::findLinks("https://user@example.com/path");
    ASSERT_EQ(links.size(), 1);
    EXPECT_EQ(links[0].kind, LinkKind::Url);
}

TEST(LinkDetection, ReportsPositionsInTheOrderOfAppearance)
{
    const QString text = "a@b.pl i http://x.pl i c@d.pl";
    const auto links = stc::links::findLinks(text);
    ASSERT_EQ(links.size(), 3);
    EXPECT_EQ(links[0], (LinkSpan{ 0, 6, LinkKind::Email }));
    EXPECT_EQ(links[1], (LinkSpan{ 9, 11, LinkKind::Url }));
    EXPECT_EQ(links[2], (LinkSpan{ 23, 6, LinkKind::Email }));
}

TEST(LinkDetection, SearchesOnlyTheRequestedFragment)
{
    const QString text = "http://a.pl http://b.pl http://c.pl";
    const auto links = stc::links::findLinks(text, 12, 23);
    ASSERT_EQ(links.size(), 1);
    EXPECT_EQ(text.mid(links[0].start, links[0].length), "http://b.pl");
}

// ------------------------------------------------------------------ STCSyntaxHighlighter: plain text

namespace
{
/// The format of the character at `column`, as it was set by the highlighter.
QTextCharFormat formatAt(const QTextBlock& block, int column)
{
    for (const auto& range : block.layout()->formats())
        if (column >= range.start && column < range.start + range.length)
            return range.format;
    return {};
}

bool isLinkFormat(const QTextCharFormat& f)
{
    return f.fontUnderline() && f.foreground().color() == QColor("blue");
}

bool isMisspelledFormat(const QTextCharFormat& f)
{
    return f.underlineStyle() == QTextCharFormat::SpellCheckUnderline;
}
} // namespace

TEST(PlainTextMode, TagsAreJustCharacters)
{
    HighlightedDocument d("[h1]Tytuł[/h1] [b]pogrubione[/b]\n[cpp]\nint x;\n[/cpp]\n", SyntaxMode::PlainText);
    for (int line = 0; line < 4; ++line)
        for (const auto& s : blockSpans(blockAt(d.document, line)))
            EXPECT_NE(s.color, QColor(Qt::gray)) << "line " << line << ": the STC tag format leaked into a plain text";

    EXPECT_FALSE(blockHasColor(blockAt(d.document, 2), "Keyword")) << "`int` inside [cpp] is not a code here";
    EXPECT_EQ(blockAt(d.document, 2).userState(), 0) << "a plain text has no multi-line states";
}

TEST(PlainTextMode, LinksAndEmailsAreFormatted)
{
    const QString text = "Wejdź na https://cpp0x.pl/kurs lub napisz: jan@agh.edu.pl, dziękuję.";
    HighlightedDocument d(text, SyntaxMode::PlainText);
    const auto block = blockAt(d.document, 0);

    EXPECT_FALSE(isLinkFormat(formatAt(block, 0))) << "ordinary text";
    EXPECT_TRUE(isLinkFormat(formatAt(block, text.indexOf("https"))));
    EXPECT_TRUE(isLinkFormat(formatAt(block, text.indexOf("kurs"))));
    EXPECT_TRUE(isLinkFormat(formatAt(block, text.indexOf("jan@"))));
    EXPECT_TRUE(isLinkFormat(formatAt(block, text.indexOf("edu"))));
    EXPECT_FALSE(isLinkFormat(formatAt(block, text.indexOf("dziękuję")))) << "the comma and what follows are not the address";
}

TEST(PlainTextMode, MisspelledWordsAreUnderlined)
{
    const QString text = "Ten kot ma kttkkkkk dom";
    HighlightedDocument d(text, SyntaxMode::PlainText);
    const auto block = blockAt(d.document, 0);

    EXPECT_FALSE(isMisspelledFormat(formatAt(block, text.indexOf("kot"))));
    EXPECT_FALSE(isMisspelledFormat(formatAt(block, text.indexOf("dom"))));
    EXPECT_TRUE(isMisspelledFormat(formatAt(block, text.indexOf("kttkkkkk"))));
}

TEST(PlainTextMode, PartsOfAddressesAreNotSpellChecked)
{
    const QString text = "https://zzzzqqqq.example.com/qwrtp i bzdurny@zzzzqqqq.pl";
    HighlightedDocument d(text, SyntaxMode::PlainText);
    const auto block = blockAt(d.document, 0);

    for (int column = 0; column < text.length(); ++column)
        EXPECT_FALSE(isMisspelledFormat(formatAt(block, column))) << "column " << column << " '" << text[column].toLatin1() << "'";
}

TEST(PlainTextMode, StcModeKeepsItsOwnBehavior)
{
    // the same text in the STC mode: no link formatting is added there
    const QString text = "https://example.com";
    HighlightedDocument d(text, SyntaxMode::Stc);
    EXPECT_FALSE(isLinkFormat(formatAt(blockAt(d.document, 0), 3)));
}

TEST(PlainTextMode, SwitchingBackAndForthRehighlights)
{
    HighlightedDocument d("[b]x[/b] www.cpp0x.pl\n");
    EXPECT_FALSE(isLinkFormat(formatAt(blockAt(d.document, 0), 10)));

    d.highlighter->setSyntaxMode(SyntaxMode::PlainText);
    EXPECT_TRUE(isLinkFormat(formatAt(blockAt(d.document, 0), 10)));

    d.highlighter->setSyntaxMode(SyntaxMode::Stc);
    EXPECT_FALSE(isLinkFormat(formatAt(blockAt(d.document, 0), 10)));
}

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", qgetenv("QT_QPA_PLATFORM").isEmpty() ? QByteArray("offscreen") : qgetenv("QT_QPA_PLATFORM"));
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
