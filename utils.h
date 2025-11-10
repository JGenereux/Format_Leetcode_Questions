#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <utility>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Response structure for test cases
struct TestCaseResponse
{
  std::vector<std::string> testCases;
  std::vector<std::pair<std::string, std::string>> testCaseParams;
};

// Function declarations
std::string FormatHTMLToString(const std::string &response);
TestCaseResponse GetTestCases(const std::string &content);
std::pair<std::string, std::string> GetParamName(const std::string &param);
void CreateJSON(json *response, const TestCaseResponse &tests);

#endif // UTILS_H
