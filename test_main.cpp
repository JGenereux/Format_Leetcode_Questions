#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <utility>
#include "utils.h"

// Test FormatHTMLToString function
class FormatHTMLToStringTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(FormatHTMLToStringTest, RemovesSimpleHTMLTags) {
    std::string input = "<p>Hello World</p>";
    std::string expected = "Hello World";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLToStringTest, RemovesNestedHTMLTags) {
    std::string input = "<div><p>Hello</p><span>World</span></div>";
    std::string expected = "HelloWorld";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLToStringTest, ConvertsHTMLEntities) {
    std::string input = "&lt;div";
    std::string expected = "<div";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLToStringTest, ConvertsMultipleHTMLEntities) {
    std::string input = "&lt; and &gt;";
    std::string expected = "< and >";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLToStringTest, ConvertsAmpersandEntity) {
    std::string input = "Hello &amp; Goodbye";
    std::string expected = "Hello & Goodbye";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLToStringTest, RemovesNBSP) {
    std::string input = "Hello&nbsp;World";
    std::string expected = "HelloWorld";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLToStringTest, HandlesMultipleNewlines) {
    std::string input = "Line1\n\n\nLine2";
    std::string expected = "Line1\nLine2";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLToStringTest, RemovesMultipleTabs) {
    std::string input = "Hello\t\t\tWorld";
    std::string expected = "HelloWorld";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLToStringTest, HandlesEmptyString) {
    std::string input = "";
    std::string expected = "";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLToStringTest, HandlesComplexHTML) {
    std::string input = "<p>Given an array &lt;int&gt;&amp; target</p>";
    std::string expected = "Given an array <int>& target";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLToStringTest, RemovesApostropheS) {
    std::string input = "array&#39;s length";
    std::string expected = "array length";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

// Test GetTestCases function
class GetTestCasesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GetTestCasesTest, ParsesSingleTestCase) {
    std::string content = "Example 1:\nInput: nums = [2,7,11,15], target = 9\nOutput: [0,1]";
    TestCaseResponse result = GetTestCases(content);
    
    EXPECT_EQ(result.testCases.size(), 1);
    EXPECT_EQ(result.testCases[0], "[0,1]");
    EXPECT_EQ(result.testCaseParams.size(), 2);
    EXPECT_EQ(result.testCaseParams[0].first, "nums");
    EXPECT_EQ(result.testCaseParams[0].second, "[2,7,11,15]");
    EXPECT_EQ(result.testCaseParams[1].first, "target");
    EXPECT_EQ(result.testCaseParams[1].second, "9");
}

TEST_F(GetTestCasesTest, ParsesMultipleTestCases) {
    std::string content = "Example 1:\nInput: nums = [2,7], target = 9\nOutput: [0,1]\nExample 2:\nInput: nums = [3,2,4], target = 6\nOutput: [1,2]";
    TestCaseResponse result = GetTestCases(content);
    
    EXPECT_EQ(result.testCases.size(), 2);
    EXPECT_EQ(result.testCases[0], "[0,1]");
    EXPECT_EQ(result.testCases[1], "[1,2]");
    EXPECT_EQ(result.testCaseParams.size(), 4);
}

TEST_F(GetTestCasesTest, HandlesEmptyContent) {
    std::string content = "";
    TestCaseResponse result = GetTestCases(content);
    
    EXPECT_EQ(result.testCases.size(), 0);
    EXPECT_EQ(result.testCaseParams.size(), 0);
}

TEST_F(GetTestCasesTest, HandlesContentWithoutExamples) {
    std::string content = "This is some problem description without examples.";
    TestCaseResponse result = GetTestCases(content);
    
    EXPECT_EQ(result.testCases.size(), 0);
    EXPECT_EQ(result.testCaseParams.size(), 0);
}

TEST_F(GetTestCasesTest, HandlesSingleParameter) {
    std::string content = "Example 1:\nInput: n = 5\nOutput: 5";
    TestCaseResponse result = GetTestCases(content);
    
    EXPECT_EQ(result.testCases.size(), 1);
    EXPECT_EQ(result.testCases[0], "5");
    EXPECT_EQ(result.testCaseParams.size(), 1);
    EXPECT_EQ(result.testCaseParams[0].first, "n");
    EXPECT_EQ(result.testCaseParams[0].second, "5");
}

// Test GetParamName function
class GetParamNameTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GetParamNameTest, ParsesSimpleParam) {
    std::string param = "nums = [1,2,3]";
    auto result = GetParamName(param);
    
    EXPECT_EQ(result.first, "nums");
    EXPECT_EQ(result.second, "[1,2,3]");
}

TEST_F(GetParamNameTest, ParsesParamWithSpaces) {
    std::string param = "target = 9";
    auto result = GetParamName(param);
    
    EXPECT_EQ(result.first, "target");
    EXPECT_EQ(result.second, "9");
}

TEST_F(GetParamNameTest, ParsesParamWithoutSpaces) {
    std::string param = "x=5";
    auto result = GetParamName(param);
    
    EXPECT_EQ(result.first, "x");
    EXPECT_EQ(result.second, "5");
}

TEST_F(GetParamNameTest, ParsesComplexArrayParam) {
    std::string param = "matrix = [[1,2],[3,4]]";
    auto result = GetParamName(param);
    
    EXPECT_EQ(result.first, "matrix");
    EXPECT_EQ(result.second, "[[1,2],[3,4]]");
}

TEST_F(GetParamNameTest, HandlesEmptyString) {
    std::string param = "";
    auto result = GetParamName(param);
    
    EXPECT_EQ(result.first, "");
    EXPECT_EQ(result.second, "");
}

TEST_F(GetParamNameTest, HandlesStringParam) {
    std::string param = "s = \"hello\"";
    auto result = GetParamName(param);
    
    EXPECT_EQ(result.first, "s");
    EXPECT_EQ(result.second, "\"hello\"");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
