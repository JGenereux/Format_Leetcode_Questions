#define UNIT_TESTING

#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>

#include "../main.cpp"

void TestFormatHTMLToString()
{
  std::string html = "<div>Value:&nbsp;&lt;tag&gt;&#39;s</div>\n\nNext\t\tLine";
  std::string expected = "Value:<tag>\nNextLine";
  auto formatted = FormatHTMLToString(html);
  assert(formatted == expected);
}

void TestGetParamName()
{
  auto param = GetParamName("nums = [1, 2, 3]");
  assert(param.first == "nums");
  assert(param.second == "[1,2,3]");
}

void TestGetTestCases()
{
  std::string content =
      "Example 1:\n"
      "Input: nums = [1, 2, 3], k = 3\n"
      "Output: 2\n"
      "Example 2:\n"
      "Input: nums = [1, 2], k = 1\n"
      "Output: 1\n";

  TestCaseResponse tests = GetTestCases(content);
  assert(tests.testCases.size() == 2);
  assert(tests.testCases[0] == "2");
  assert(tests.testCases[1] == "1");
  assert(tests.testCaseParams.size() == 4);
  assert(tests.testCaseParams[0] == std::make_pair(std::string("nums"), std::string("[1,2,3]")));
  assert(tests.testCaseParams[1] == std::make_pair(std::string("k"), std::string("3")));
}

void TestCreateJSON()
{
  json response = {{"title", "Sample/Title"}, {"content", "desc"}, {"difficulty", "Easy"}};
  TestCaseResponse tests;
  tests.testCases = {"42"};
  tests.testCaseParams = {{"nums", "[1,2,3]"}, {"k", "3"}};

  std::filesystem::path tempRoot = std::filesystem::temp_directory_path() / "leetcode_tests";
  std::filesystem::remove_all(tempRoot);

  std::filesystem::path outputDir = tempRoot / "Questions";
  CreateJSON(&response, tests, outputDir.string());

  std::filesystem::path outputFile = outputDir / "Sample_Title.txt";
  assert(std::filesystem::exists(outputFile));

  std::ifstream input(outputFile);
  std::stringstream buffer;
  buffer << input.rdbuf();
  std::string content = buffer.str();
  assert(content.find("\"expectedResult\": \"42\"") != std::string::npos);
  assert(content.find("\"nums\": \"[1,2,3]\"") != std::string::npos);
  assert(content.find("\"k\": \"3\"") != std::string::npos);

  std::filesystem::remove_all(tempRoot);
}

int main()
{
  TestFormatHTMLToString();
  TestGetParamName();
  TestGetTestCases();
  TestCreateJSON();

  std::cout << "All tests passed!" << std::endl;
  return 0;
}
