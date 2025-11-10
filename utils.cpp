#include "utils.h"
#include <iostream>
#include <fstream>
#include <algorithm>

// check for <code> tag
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
    if (i <= response.length() - 4 && (response.substr(i, 4) == "&lt;" || response.substr(i, 4) == "&gt;"))
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
    if (i <= response.length() - 5 && (response.substr(i, 5) == "&amp;"))
    {
      result += "&";
      i += 5;
      continue;
    }

    // check for &#39;s
    if (i <= response.length() - 6 && response.substr(i, 6) == "&#39;s")
    {
      i += 6;
      continue;
    }

    // check for &nbsp; tags
    if (i <= response.length() - 6 && response.substr(i, 6) == "&nbsp;")
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

/**
 * Basic test cases given by leetcode are given in a string of the form. Example case & output.
 * Should always be at least 2 test cases given.
 * @returns array of oxpected outputs for the test cases.
 */
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
          // std::cout << paramName << " " << paramRes << std::endl;
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

void CreateJSON(json *response, const TestCaseResponse &tests)
{
  // filter out invalid characters from title
  std::string title = (*response)["title"];
  const std::string invalid_chars = "\\/:*?\"<>|";
  for (char c : invalid_chars)
  {
    std::replace(title.begin(), title.end(), c, '_');
  }
  std::string jsonName = "../../../Questions/" + title + ".txt";

  std::ofstream outputJSON;
  outputJSON.open(jsonName);
  // should have to create the file so always should open
  if (!outputJSON.is_open())
  {
    std::cerr << "Error creating output file for JSON response" << std::endl;
    return;
  }

  outputJSON << "{\n";
  // iterates through json response inserting key and value as pair into output file
  for (auto it = (*response).begin(); it != (*response).end(); ++it)
  {
    outputJSON << "\"" << it.key() << "\"" << ": " << it.value() << ',' << "\n";
  }

  // handle situation where testCases might not generate

  // Insert testcases
  outputJSON << "\"testCases\"" << ": [" << "\n";

  int j = 0;
  int size = tests.testCases.size();
  for (int i = 0; i < size; i++)
  {
    // start inserting new object into array inside json file
    outputJSON << "{\n";

    std::string expectedResult = tests.testCases[i]; // testcase expected outputs
    outputJSON << "\"expectedResult\": " << "\"" << expectedResult << "\",\n";

    int numParams = tests.testCaseParams.size() / tests.testCases.size();
    for (int x = 0; x < numParams; x++)
    {
      std::pair<std::string, std::string> fixedParam = tests.testCaseParams[j++];
      if (x == numParams - 1)
      {
        outputJSON << "\"" << fixedParam.first << "\": " << "\"" << fixedParam.second << "\"\n";
      }
      else
      {
        outputJSON << "\"" << fixedParam.first << "\": " << "\"" << fixedParam.second << "\",\n";
      }
    }

    // if i is at the end then we need to close off the obj
    if (i == size - 1)
    {
      outputJSON << "}\n";
    }
    else
    {
      outputJSON << "},\n";
    }
  }

  outputJSON << "]\n";

  outputJSON << "}";
  outputJSON.close();
}

/**
 * params are taken from the json as a string containing 'paramName'='param'
 * This function splits the paramName and param seperately to label them in the output JSON easier.
 * (the problem function calls explicility used by the users will contain the same paramNames so makes using them easier as well)
 */
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
