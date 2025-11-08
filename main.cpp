#include <curl/curl.h>
#include <cstring>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// CURL callback data structure
struct Response
{
  std::string data;
};

struct TestCaseResponse
{
  std::vector<std::string> testCases;
  std::vector<std::pair<std::string, std::string>> testCaseParams;
};

size_t writeChunk(void *data, size_t size, size_t nmemb, void *userData);
void formatResponse(const std::string &response);
std::string formatHTMLToString(const std::string &response);
TestCaseResponse getTestCases(const std::string &content);
void createJSON(const json &response, const TestCaseResponse &testCases);

int main()
{
  std::string questionName = "";
  std::cout << "Enter Leetcode question name: " << std::endl;
  std::cin >> questionName;

  // Initialize CURL
  CURL *curl = curl_easy_init();
  if (curl == nullptr)
  {
    std::cerr << "HTTP REQUEST FAILED: curl_easy_init() failed!" << std::endl;
    return -1;
  }

  std::cout << "Curl initialized successfully!" << std::endl;

  Response response;

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
   * Each time it is called the pointer is passed to a new chunk of response data
   */
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeChunk);

  // Address of response struct is passed in writeChunk as userData
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void *>(&response));

  // Perform the HTTP request
  CURLcode result = curl_easy_perform(curl);
  if (result != CURLE_OK)
  {
    std::cerr << "Error: " << curl_easy_strerror(result) << std::endl;
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    return -1;
  }

  formatResponse(response.data);

  // Cleanup
  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);
  return 0;
}

/**
 * CURL callback function to write received data
 * @param data Pointer to the received data chunk
 * @param size Size of each element (typically 1)
 * @param nmemb Number of elements
 * @param userData Pointer to Response structure
 * @return Number of bytes processed
 */
size_t writeChunk(void *data, size_t size, size_t nmemb, void *userData)
{
  size_t realSize = size * nmemb;
  Response *response = static_cast<Response *>(userData);
  
  response->data.append(static_cast<char *>(data), realSize);
  return realSize;
}

/**
 * Processes the JSON response from LeetCode GraphQL API
 * Extracts and formats: title, content, difficulty, topicTags, and hints
 * @param response Raw JSON response string
 */
void formatResponse(const std::string &response)
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
        question[tag][0] = formatHTMLToString(question[tag][0]);
        continue;
      }
      if (question.contains(tag))
      {
        question[tag] = formatHTMLToString(question[tag]);
        // Get test cases from given content
        if (tag == "content")
        {
          testCases = getTestCases(question[tag]);
        }
      }
    }

    createJSON(question, testCases);
  }
  catch (json::parse_error &e)
  {
    std::cerr << "Parse error: " << e.what() << std::endl;
    return;
  }
}

/**
 * Converts HTML entities and removes HTML tags from string
 * Handles: <tags>, &lt;, &gt;, &amp;, &#39;s, &nbsp;
 * @param response HTML string to format
 * @return Cleaned string without HTML
 */
std::string formatHTMLToString(const std::string &response)
{
  std::string result;
  result.reserve(response.length());
  size_t i = 0;

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

    // check for &lt; (<) , &gt; (>)
    if (i + 4 <= response.length())
    {
      std::string expression = response.substr(i, 4);
      if (expression == "&lt;")
      {
        result += "<";
        i += 4;
        continue;
      }
      else if (expression == "&gt;")
      {
        result += ">";
        i += 4;
        continue;
      }
    }

    // check for &amp; (&)
    if (i + 5 <= response.length() && response.substr(i, 5) == "&amp;")
    {
      result += "&";
      i += 5;
      continue;
    }

    // check for &#39;s (possessive apostrophe)
    if (i + 6 <= response.length() && response.substr(i, 6) == "&#39;s")
    {
      i += 6;
      continue;
    }

    // check for &nbsp; (non-breaking space)
    if (i + 6 <= response.length() && response.substr(i, 6) == "&nbsp;")
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
 * Extracts test cases from LeetCode problem content
 * Parses Example sections containing Input and Output
 * @param content Problem content string
 * @return TestCaseResponse containing test cases and parameters
 */
TestCaseResponse getTestCases(const std::string &content)
{
  TestCaseResponse tests;

  size_t i = 0;
  while (i < content.length())
  {
    if (i + 7 <= content.length() && content.substr(i, 7) == "Example")
    {
      i += 7;
      while (i < content.length())
      {
        if (i + 6 <= content.length() && content.substr(i, 6) == "Input:")
        {
          i += 6;
          std::string paramName;
          std::string paramRes;
          int j = -1;
          while (i + 7 <= content.length() && content.substr(i, 7) != "\nOutput")
          {
            // check if new param is being searched
            if (i < content.length() - 1 && (content[i] == ',' && content[i + 1] == ' '))
            {
              tests.testCaseParams.emplace_back(paramName, paramRes);
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
          if (!paramName.empty() && !paramRes.empty())
          {
            tests.testCaseParams.emplace_back(paramName, paramRes);
          }
        }

        if (i + 6 <= content.length() && content.substr(i, 6) == "Output")
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

/**
 * Creates a JSON output file with the problem details and test cases
 * @param response JSON object containing problem data
 * @param tests TestCaseResponse with test cases and parameters
 */
void createJSON(const json &response, const TestCaseResponse &tests)
{
  // Filter out invalid characters from title for filename
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
  // Iterate through JSON response inserting key and value pairs
  for (auto it = response.begin(); it != response.end(); ++it)
  {
    outputJSON << "\"" << it.key() << "\"" << ": " << it.value() << ",\n";
  }

  // Insert test cases
  outputJSON << "\"testCases\"" << ": [" << "\n";

  size_t j = 0;
  size_t size = tests.testCases.size();
  for (size_t i = 0; i < size; i++)
  {
    // Start inserting new object into array
    outputJSON << "{\n";

    const std::string &expectedResult = tests.testCases[i];
    outputJSON << "\"expectedResult\": \"" << expectedResult << "\",\n";

    size_t numParams = tests.testCases.empty() ? 0 : tests.testCaseParams.size() / tests.testCases.size();
    for (size_t x = 0; x < numParams; x++)
    {
      const auto &fixedParam = tests.testCaseParams[j++];
      outputJSON << "\"" << fixedParam.first << "\": \"" << fixedParam.second << "\"";
      if (x < numParams - 1)
      {
        outputJSON << ",";
      }
      outputJSON << "\n";
    }

    // Close the test case object
    outputJSON << "}";
    if (i < size - 1)
    {
      outputJSON << ",";
    }
    outputJSON << "\n";
  }

  outputJSON << "]\n";

  outputJSON << "}";
  outputJSON.close();
}