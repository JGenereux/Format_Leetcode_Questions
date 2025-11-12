#include <gtest/gtest.h>
#include <curl/curl.h>
#include <string.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <string>

using json = nlohmann::json;

// Copy the struct definitions from main.cpp
typedef struct Response
{
  char *string;
  size_t size;
};

struct TestCaseResponse
{
  std::vector<std::string> testCases;
  std::vector<std::pair<std::string, std::string>> testCaseParams;
};

// Forward declarations of functions to test
std::string FormatHTMLToString(const std::string &response);
TestCaseResponse GetTestCases(const std::string &content);
std::pair<std::string, std::string> GetParamName(const std::string &param);
size_t write_chunk(void *data, size_t size, size_t nmemb, void *userData);

// Implementation of the functions (copied from main.cpp for testing)
std::string FormatHTMLToString(const std::string &response)
{
  int i = 0;
  std::string result = "";

  while (i < response.length())
  {
    // check for HTML elements
    if (response[i] == '<')
    {
      while (response[i] != '>')
      {
        i++;
      }
      i++;
      continue;
    }

    // check for &lt; (<) , &gt (>);
    if (i < response.length() - 4 && (response.substr(i, 4) == "&lt;" || response.substr(i, 4) == "&gt;"))
    {
      std::string expression = response.substr(i, 4);
      if (expression == "&lt;")
      {
        result += "<";
      }
      else if (expression == "&gt;")
      {
        result += ">";
      }
      i += 4;
      continue;
    }

    // check for &amp (&)
    if (i < response.length() - 5 && (response.substr(i, 5) == "&amp;"))
    {
      result += "&";
      i += 5;
      continue;
    }

    // check for &#39;s
    if (i < response.length() - 6 && response.substr(i, 6) == "&#39;s")
    {
      i += 6;
      continue;
    }

    // check for &nbsp; tags
    if (i < response.length() - 6 && response.substr(i, 6) == "&nbsp;")
    {
      i += 6;
      continue;
    }

    // check for multiple whitespace characters
    // want to keep 1 where there are multiple
    if (response[i] == '\n')
    {
      result += "\n";
      while (i + 1 < response.length() && response[i + 1] == '\n')
      {
        i++;
      }
      i++;
      continue;
    }

    if (response[i] == '\t')
    {
      while (i + 1 < response.length() && response[i + 1] == '\t')
      {
        i++;
      }
      i++;
      continue;
    }

    result += (response[i]);
    i++;
  }
  return result;
}

TestCaseResponse GetTestCases(const std::string &content)
{
  TestCaseResponse tests;

  int i = 0;
  while (i < content.length())
  {
    if (i < content.length() - 7 && content.substr(i, 7) == "Example")
    {
      i += 7;
      while (i < content.length())
      {
        if (i <= content.length() - 6 && content.substr(i, 6) == "Input:")
        {
          i += 6;
          std::string input = "";
          std::string paramName = "";
          std::string paramRes = "";
          int j = -1;
          while (i < content.length() - 7 && content.substr(i, 7) != "\nOutput")
          {
            // check if new param is being searched
            if (i < content.length() - 1 && (content[i] == ',' && content[i + 1] == ' '))
            {
              tests.testCaseParams.push_back({paramName, paramRes});
              paramName = "";
              paramRes = "";
              j = -1;
              i++;
              continue;
            }
            // now looking for paramResult so set j (flag for where = is)
            if (content[i] == '=')
            {
              j = i;
              i++;
              continue;
            }

            if (j == -1 && content[i] != ' ')
            {
              paramName += content[i];
            }
            else if (j != -1 && content[i] != ' ')
            {
              paramRes += content[i];
            }
            i++;
          }
          if (paramName.length() != 0 && paramRes.length() != 0)
          {
            tests.testCaseParams.push_back({paramName, paramRes});
          }
        }

        if (i <= content.length() - 6 && content.substr(i, 6) == "Output")
        {
          i += 6;
          std::string testCase = "";
          while (i < content.length() && content[i] != '\n')
          {
            if (content[i] != ' ' && content[i] != ':')
            {
              testCase += content[i];
            }
            i++;
          }
          tests.testCases.push_back(testCase);
          break;
        }
        i++;
      }
    }
    else
    {
      i++;
    }
  }

  return tests;
}

std::pair<std::string, std::string> GetParamName(const std::string &param)
{
  std::string paramName = "";
  std::string paramResult = "";
  bool nameParsed = false;
  for (int i = 0; i < param.length(); i++)
  {

    if (param[i] == '=')
    {
      nameParsed = true;
      continue;
    }

    if (param[i] != ' ' && !nameParsed)
    {
      paramName += param[i];
    }
    else if (param[i] != ' ' && nameParsed)
    {
      paramResult += param[i];
    }
  }
  return {paramName, paramResult};
}

size_t write_chunk(void *data, size_t size, size_t nmemb, void *userData)
{
  size_t real_size = size * nmemb;

  Response *response = (Response *)userData;
  char *ptr = (char *)realloc(response->string, response->size + real_size + 1);

  if (ptr == nullptr)
  {
    return 0;
  }
  response->string = ptr;
  memcpy(&(response->string[response->size]), data, real_size);
  response->size += real_size;
  response->string[response->size] = '\0';
  return real_size;
}

// ==================== TEST CASES ====================

// Tests for FormatHTMLToString function
class FormatHTMLTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(FormatHTMLTest, RemovesSimpleHTMLTags) {
    std::string input = "<p>Hello World</p>";
    std::string expected = "Hello World";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLTest, RemovesNestedHTMLTags) {
    std::string input = "<div><p>Test</p><span>Content</span></div>";
    std::string expected = "TestContent";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLTest, ConvertsLessThanEntity) {
    std::string input = "x &lt; y";
    std::string expected = "x < y";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLTest, ConvertsGreaterThanEntity) {
    std::string input = "x &gt; y";
    std::string expected = "x > y";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLTest, ConvertsAmpersandEntity) {
    std::string input = "Tom &amp; Jerry";
    std::string expected = "Tom & Jerry";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLTest, RemovesApostropheSEntity) {
    std::string input = "It&#39;s working";
    std::string expected = "It working";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLTest, RemovesNonBreakingSpace) {
    std::string input = "Hello&nbsp;World";
    std::string expected = "HelloWorld";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLTest, CollapsesMultipleNewlines) {
    std::string input = "Line1\n\n\nLine2";
    std::string expected = "Line1\nLine2";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLTest, RemovesTabs) {
    std::string input = "Text\t\t\twith tabs";
    std::string expected = "Textwith tabs";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLTest, HandlesComplexMixedContent) {
    std::string input = "<p>Array &lt;strong&gt; &amp; &lt;int&gt;</p>";
    std::string expected = "Array <strong> & <int>";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLTest, HandlesEmptyString) {
    std::string input = "";
    std::string expected = "";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

TEST_F(FormatHTMLTest, HandlesPlainTextWithoutHTML) {
    std::string input = "Plain text without HTML";
    std::string expected = "Plain text without HTML";
    EXPECT_EQ(FormatHTMLToString(input), expected);
}

// Tests for GetTestCases function
class GetTestCasesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GetTestCasesTest, ParsesSingleTestCase) {
    std::string content = "Example 1:\nInput: nums = [1,2,3]\nOutput: 6\nExplanation: Sum is 6";
    TestCaseResponse result = GetTestCases(content);
    
    EXPECT_EQ(result.testCases.size(), 1);
    EXPECT_EQ(result.testCases[0], "6");
    EXPECT_EQ(result.testCaseParams.size(), 1);
    EXPECT_EQ(result.testCaseParams[0].first, "nums");
    EXPECT_EQ(result.testCaseParams[0].second, "[1,2,3]");
}

TEST_F(GetTestCasesTest, ParsesMultipleTestCases) {
    std::string content = "Example 1:\nInput: x = 5\nOutput: 25\nExample 2:\nInput: x = 3\nOutput: 9\n";
    TestCaseResponse result = GetTestCases(content);
    
    EXPECT_EQ(result.testCases.size(), 2);
    EXPECT_EQ(result.testCases[0], "25");
    EXPECT_EQ(result.testCases[1], "9");
    EXPECT_EQ(result.testCaseParams.size(), 2);
}

TEST_F(GetTestCasesTest, ParsesMultipleParameters) {
    std::string content = "Example 1:\nInput: nums = [1,2], target = 3\nOutput: [0,1]\n";
    TestCaseResponse result = GetTestCases(content);
    
    EXPECT_EQ(result.testCases.size(), 1);
    EXPECT_EQ(result.testCaseParams.size(), 2);
    EXPECT_EQ(result.testCaseParams[0].first, "nums");
    EXPECT_EQ(result.testCaseParams[0].second, "[1,2]");
    EXPECT_EQ(result.testCaseParams[1].first, "target");
    EXPECT_EQ(result.testCaseParams[1].second, "3");
}

TEST_F(GetTestCasesTest, HandlesContentWithoutExamples) {
    std::string content = "This is a problem description without examples.";
    TestCaseResponse result = GetTestCases(content);
    
    EXPECT_EQ(result.testCases.size(), 0);
    EXPECT_EQ(result.testCaseParams.size(), 0);
}

TEST_F(GetTestCasesTest, HandlesEmptyString) {
    std::string content = "";
    TestCaseResponse result = GetTestCases(content);
    
    EXPECT_EQ(result.testCases.size(), 0);
    EXPECT_EQ(result.testCaseParams.size(), 0);
}

TEST_F(GetTestCasesTest, ParsesArrayOutputs) {
    std::string content = "Example 1:\nInput: nums = [1,2,3]\nOutput: [1,2,3]\n";
    TestCaseResponse result = GetTestCases(content);
    
    EXPECT_EQ(result.testCases.size(), 1);
    EXPECT_EQ(result.testCases[0], "[1,2,3]");
}

// Tests for GetParamName function
class GetParamNameTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GetParamNameTest, ParsesSimpleParameter) {
    std::string input = "nums = [1,2,3]";
    auto result = GetParamName(input);
    
    EXPECT_EQ(result.first, "nums");
    EXPECT_EQ(result.second, "[1,2,3]");
}

TEST_F(GetParamNameTest, HandlesSpacesAroundEquals) {
    std::string input = "target  =  5";
    auto result = GetParamName(input);
    
    EXPECT_EQ(result.first, "target");
    EXPECT_EQ(result.second, "5");
}

TEST_F(GetParamNameTest, ParsesStringParameter) {
    std::string input = "s = \"hello\"";
    auto result = GetParamName(input);
    
    EXPECT_EQ(result.first, "s");
    EXPECT_EQ(result.second, "\"hello\"");
}

TEST_F(GetParamNameTest, ParsesNestedArrayParameter) {
    std::string input = "matrix = [[1,2],[3,4]]";
    auto result = GetParamName(input);
    
    EXPECT_EQ(result.first, "matrix");
    EXPECT_EQ(result.second, "[[1,2],[3,4]]");
}

TEST_F(GetParamNameTest, HandlesNoSpaces) {
    std::string input = "x=10";
    auto result = GetParamName(input);
    
    EXPECT_EQ(result.first, "x");
    EXPECT_EQ(result.second, "10");
}

TEST_F(GetParamNameTest, HandlesEmptyValue) {
    std::string input = "val = ";
    auto result = GetParamName(input);
    
    EXPECT_EQ(result.first, "val");
    EXPECT_EQ(result.second, "");
}

TEST_F(GetParamNameTest, HandlesLongParameterName) {
    std::string input = "longerParameterName = value123";
    auto result = GetParamName(input);
    
    EXPECT_EQ(result.first, "longerParameterName");
    EXPECT_EQ(result.second, "value123");
}

// Tests for write_chunk function
class WriteChunkTest : public ::testing::Test {
protected:
    Response response;
    
    void SetUp() override {
        response.string = (char *)malloc(1);
        response.size = 0;
        response.string[0] = '\0';
    }
    
    void TearDown() override {
        free(response.string);
    }
};

TEST_F(WriteChunkTest, WritesFirstChunk) {
    const char *data = "Hello";
    size_t result = write_chunk((void *)data, 1, 5, &response);
    
    EXPECT_EQ(result, 5);
    EXPECT_EQ(response.size, 5);
    EXPECT_STREQ(response.string, "Hello");
}

TEST_F(WriteChunkTest, AppendsMultipleChunks) {
    const char *data1 = "Hello";
    const char *data2 = " World";
    
    write_chunk((void *)data1, 1, 5, &response);
    size_t result = write_chunk((void *)data2, 1, 6, &response);
    
    EXPECT_EQ(result, 6);
    EXPECT_EQ(response.size, 11);
    EXPECT_STREQ(response.string, "Hello World");
}

TEST_F(WriteChunkTest, HandlesEmptyChunk) {
    const char *data = "";
    size_t result = write_chunk((void *)data, 1, 0, &response);
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(response.size, 0);
}

TEST_F(WriteChunkTest, HandlesLargeChunk) {
    std::string largeData(1000, 'X');
    size_t result = write_chunk((void *)largeData.c_str(), 1, 1000, &response);
    
    EXPECT_EQ(result, 1000);
    EXPECT_EQ(response.size, 1000);
    EXPECT_EQ(response.string[999], 'X');
}

TEST_F(WriteChunkTest, CalculatesRealSizeCorrectly) {
    const char *data = "Test";
    size_t result = write_chunk((void *)data, 2, 2, &response);
    
    EXPECT_EQ(result, 4);
    EXPECT_EQ(response.size, 4);
}

// Main function to run all tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
