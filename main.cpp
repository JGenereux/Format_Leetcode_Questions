#ifndef UNIT_TESTING
#include <curl/curl.h>
#endif
#include <string.h>

#include <unordered_set>
#include <iostream>
#include <fstream>
#include <map>
#include <algorithm>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// used to have a dynamic string
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

size_t write_chunk(void *data, size_t size, size_t nmemb, void *userData);

void formatResponse(char *response);
std::string FormatHTMLToString(const std::string &response);
TestCaseResponse GetTestCases(const std::string &content);

std::pair<std::string, std::string> GetParamName(const std::string &param);
void CreateJSON(json *response, const TestCaseResponse &testCases, const std::string &outputDir = "../../../Questions");

#ifndef UNIT_TESTING
int main()
{
  std::string questionName = "";
  std::cout << "Enter Leetcode question name: " << std::endl;
  std::cin >> questionName;

  CURL *curl;
  CURLcode result;

  // Initialize CURL
  curl = curl_easy_init();
  if (curl == nullptr)
  {
    std::cerr << "HTTP REQUEST FAILED: curl_easy_init() failed!" << std::endl;
    return -1;
  }
  else
  {
    std::cout << "Curl initialized successfully!" << std::endl;
  }

  Response response;
  response.string = (char *)malloc(1);
  response.size = 0;

  // Set options for the HTTP request
  curl_easy_setopt(curl, CURLOPT_URL,
                   "https://leetcode.com/graphql");

  // Set Post data (like JSON body) to match leetcode graph ql query
  json query = {
      {"query", "query questionData($titleSlug: String!) { question(titleSlug: $titleSlug) { title content difficulty topicTags { name } hints } }"},
      {"variables", {
                        {"titleSlug", questionName} // This can now be easily modified
                    }}};

  const std::string postData = query.dump();
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());

  // Set headers for JSON data
  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");

  std::string referer = "Referrer: https://leetcode.com/problems/" + questionName + "/";
  headers = curl_slist_append(headers, referer.c_str());

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  /**
   * WriteFunction allows for specifying a callback function
   * Curl_easy_perfrom will call this function repeatedly
   * Each time it is called the pointer is passed to a new chunk of response
   * string
   */
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_chunk);

  // Address of response string is passed in write_chunk as userData
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

  // Perform the HTTP request
  result = curl_easy_perform(curl);
  if (result != CURLE_OK)
  {
    std::cerr << "Error: " << curl_easy_strerror(result) << std::endl;
    curl_easy_cleanup(curl);
    return -1;
  }

  formatResponse(response.string);
  free(response.string);
  // Cleanup
  curl_easy_cleanup(curl);
  return 0;
}
#endif

// returns number of bytes in the chunk
//  data is set to a ptr that points to block of data recieved in this chunk
//  nmemb is the number of bytes in the block of data
// userData points to what we want (points to where the response string is stored)
size_t write_chunk(void *data, size_t size, size_t nmemb, void *userData)
{
  // size is always 1
  size_t real_size = size * nmemb;

  Response *response = (Response *)userData;
  // allocate more space for chunk that was recieved
  // response->size is size of existing mem and real_size is the size recieved and +1 accounts for null
  char *ptr = (char *)realloc(response->string, response->size + real_size + 1);

  if (ptr == nullptr)
  {
    std::cerr << "Problem reallocating space for chunk recieved" << std::endl;
    return 0;
  }
  // set response string to the new (larger) memory address
  response->string = ptr;
  // append new porition onto existing string
  memcpy(&(response->string[response->size]), data, real_size);
  // update strings size
  response->size += real_size;
  // append null character
  response->string[response->size] = '\0';
  return real_size;
}

/**
 * Returns a map containing the following tags stored as keys
 * and their description as their value.
 *
 * title content difficulty topicTags { name } hints
 *
 * Assumes json response will use the tags in the given order above.
 */
void formatResponse(char *response)
{
  std::vector<std::string> currentTags = {"title", "content", "difficulty", "topicTags", "hints"};

  try
  {
    json parsed = json::parse(response);
    json question = parsed["data"]["question"];

    TestCaseResponse testCases;

    for (const auto &tag : currentTags)
    {
      if (question.contains(tag) && tag == "topicTags")
      {
        std::vector<std::string> topics;
        for (auto topic : question[tag])
        {
          topics.push_back(topic["name"]);
        }
        question[tag] = topics;
        continue;
      }
      if (question.contains(tag) && tag == "hints")
      {
        if (question[tag][0].size() == 0)
        {
          continue;
        }
        question[tag][0] = FormatHTMLToString(question[tag][0]);
        continue;
      }
      if (question.contains(tag))
      {
        question[tag] = FormatHTMLToString(question[tag]);
        // Get testcases from given content
        if (tag == "content")
        {
          testCases = GetTestCases(question[tag]);
        }
      }
    }

    CreateJSON(&question, testCases);
  }
  catch (json::parse_error &e)
  {
    std::cerr << "Parse error: " << e.what() << std::endl;
    return;
  }
}

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

void CreateJSON(json *response, const TestCaseResponse &tests, const std::string &outputDir)
{
  namespace fs = std::filesystem;
  // filter out invalid characters from title
  std::string title = (*response)["title"];
  const std::string invalid_chars = "\\/:*?\"<>|";
  for (char c : invalid_chars)
  {
    std::replace(title.begin(), title.end(), c, '_');
  }
  fs::path basePath(outputDir);
  std::error_code ec;
  if (!fs::exists(basePath))
  {
    fs::create_directories(basePath, ec);
    if (ec)
    {
      std::cerr << "Error creating output directory: " << ec.message() << std::endl;
      return;
    }
  }
  fs::path outputPath = basePath / (title + ".txt");

  std::ofstream outputJSON;
  outputJSON.open(outputPath);
  // should have to create the file so always should open
  if (!outputJSON.is_open())
  {
    std::cerr << "Error creating output file for JSON response: " << outputPath << std::endl;
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