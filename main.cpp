#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Callback data structure for CURL response
struct CurlResponse
{
  std::string data;
};

struct TestCaseResponse
{
  std::vector<std::string> testCases;
  std::vector<std::pair<std::string, std::string>> testCaseParams;
};

size_t WriteCallback(void *data, size_t size, size_t nmemb, void *userData);

void FormatResponse(const std::string &response);
std::string FormatHTMLToString(const std::string &response);
TestCaseResponse GetTestCases(const std::string &content);
void CreateJSON(const json &response, const TestCaseResponse &testCases);

int main()
{
  std::string questionName;
  std::cout << "Enter LeetCode question name: " << std::endl;
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
  
  std::cout << "Fetching question data..." << std::endl;

  CurlResponse response;

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
   * curl_easy_perform will call this function repeatedly
   * Each time it is called the pointer is passed to a new chunk of response
   * string
   */
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

  // Address of response string is passed in WriteCallback as userData
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  // Perform the HTTP request
  result = curl_easy_perform(curl);
  if (result != CURLE_OK)
  {
    std::cerr << "Error: " << curl_easy_strerror(result) << std::endl;
    curl_easy_cleanup(curl);
    return -1;
  }

  FormatResponse(response.data);
  
  // Cleanup
  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);
  return 0;
}

// Callback function for CURL to write received data
// Returns number of bytes in the chunk
// data: pointer to block of data received in this chunk
// size * nmemb: total number of bytes in the block of data
// userData: pointer to where the response string is stored
size_t WriteCallback(void *data, size_t size, size_t nmemb, void *userData)
{
  size_t realSize = size * nmemb;
  CurlResponse *response = static_cast<CurlResponse*>(userData);
  
  try
  {
    response->data.append(static_cast<char*>(data), realSize);
  }
  catch (const std::bad_alloc &e)
  {
    std::cerr << "Memory allocation error while receiving data: " << e.what() << std::endl;
    return 0;
  }
  
  return realSize;
}

/**
 * Processes the JSON response from LeetCode GraphQL API
 * Extracts and formats: title, content, difficulty, topicTags, hints
 * Creates a JSON file with the formatted data and test cases
 */
void FormatResponse(const std::string &response)
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
        for (const auto &topic : question[tag])
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

    CreateJSON(question, testCases);
  }
  catch (const json::parse_error &e)
  {
    std::cerr << "Parse error: " << e.what() << std::endl;
    return;
  }
}

// Converts HTML content to plain text, removing tags and decoding HTML entities
std::string FormatHTMLToString(const std::string &response)
{
  size_t i = 0;
  std::string result;
  result.reserve(response.length());

  while (i < response.length())
  {
    // Check for HTML elements
    if (response[i] == '<')
    {
      while (response[i] != '>')
      {
        i++;
      }
      i++;
      continue;
    }

    // Check for &lt; (<), &gt; (>)
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

    // Check for &amp; (&)
    if (i < response.length() - 5 && (response.substr(i, 5) == "&amp;"))
    {
      result += "&";
      i += 5;
      continue;
    }

    // Check for &#39;s (apostrophe entity)
    if (i < response.length() - 6 && response.substr(i, 6) == "&#39;s")
    {
      i += 6;
      continue;
    }

    // Check for &nbsp; (non-breaking space)
    if (i < response.length() - 6 && response.substr(i, 6) == "&nbsp;")
    {
      i += 6;
      continue;
    }

    // Check for multiple whitespace characters - keep only one
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
 * Extracts test cases from LeetCode problem content
 * Parses "Example" sections to extract inputs and expected outputs
 * @returns TestCaseResponse containing test cases and their parameters
 */
TestCaseResponse GetTestCases(const std::string &content)
{
  TestCaseResponse tests;

  size_t i = 0;
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
          std::string paramName;
          std::string paramRes;
          int j = -1;
          while (i < content.length() - 7 && content.substr(i, 7) != "\nOutput")
          {
            // Check if new parameter is being parsed
            if (i < content.length() - 1 && (content[i] == ',' && content[i + 1] == ' '))
            {
              tests.testCaseParams.push_back({paramName, paramRes});
              paramName = "";
              paramRes = "";
              j = -1;
              i++;
              continue;
            }
            // Now looking for parameter result, set j (flag for '=' position)
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
          if (!paramName.empty() && !paramRes.empty())
          {
            tests.testCaseParams.push_back({paramName, paramRes});
          }
        }

        if (i <= content.length() - 6 && content.substr(i, 6) == "Output")
        {
          i += 6;
          std::string testCase;
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

void CreateJSON(const json &response, const TestCaseResponse &tests)
{
  // Filter out invalid characters from title for file name
  std::string title = response["title"];
  const std::string invalidChars = "\\/:*?\"<>|";
  for (char c : invalidChars)
  {
    std::replace(title.begin(), title.end(), c, '_');
  }
  std::string jsonName = "../../../Questions/" + title + ".txt";

  std::ofstream outputJSON(jsonName);
  if (!outputJSON.is_open())
  {
    std::cerr << "Error creating output file for JSON response" << std::endl;
    return;
  }

  outputJSON << "{\n";
  // Iterate through json response inserting key and value pairs into output file
  for (auto it = response.begin(); it != response.end(); ++it)
  {
    outputJSON << "\"" << it.key() << "\"" << ": " << it.value() << ',' << "\n";
  }

  // Insert test cases
  outputJSON << "\"testCases\"" << ": [" << "\n";

  size_t j = 0;
  const size_t size = tests.testCases.size();
  for (size_t i = 0; i < size; i++)
  {
    // Start inserting new object into array inside json file
    outputJSON << "{\n";

    std::string expectedResult = tests.testCases[i]; // Test case expected outputs
    outputJSON << "\"expectedResult\": " << "\"" << expectedResult << "\",\n";

    const size_t numParams = tests.testCaseParams.size() / tests.testCases.size();
    for (size_t x = 0; x < numParams; x++)
    {
      const auto &fixedParam = tests.testCaseParams[j++];
      if (x == numParams - 1)
      {
        outputJSON << "\"" << fixedParam.first << "\": " << "\"" << fixedParam.second << "\"\n";
      }
      else
      {
        outputJSON << "\"" << fixedParam.first << "\": " << "\"" << fixedParam.second << "\",\n";
      }
    }

    // If at the end, close the object without trailing comma
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
  
  std::cout << "Successfully created: " << jsonName << std::endl;
}
