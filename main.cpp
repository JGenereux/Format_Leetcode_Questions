#include <curl/curl.h>
#include <string.h>

#include <unordered_set>
#include <iostream>
#include <fstream>
#include <map>
#include <sys/stat.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Structure to hold dynamically growing response string from HTTP request
// Memory is allocated/reallocated as data arrives and must be freed by caller
typedef struct Response
{
  char *string;  // Dynamically allocated buffer for response data
  size_t size;   // Current size of data (excluding null terminator)
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
void CreateJSON(json *response, const TestCaseResponse &testCases);

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

  // Initialize response structure with minimal allocation
  // This will be expanded by write_chunk callback as data arrives
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

  // Set HTTP headers for JSON data and referrer
  // Note: headers must be freed with curl_slist_free_all() before exit
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
    // Clean up all allocated resources before returning
    free(response.string);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return -1;
  }

  formatResponse(response.string);
  
  // Clean up all allocated resources
  free(response.string);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return 0;
}

/**
 * Callback function for CURL to handle incoming data chunks
 * 
 * @param data Pointer to the block of data received in this chunk
 * @param size Size multiplier (always 1 per CURL documentation)
 * @param nmemb Number of bytes in the data block
 * @param userData User-provided pointer (points to Response structure)
 * @return Number of bytes processed (real_size), or 0 on error
 * 
 * Note: Response string memory is reallocated to accommodate new data.
 *       Caller is responsible for freeing Response.string when done.
 */
size_t write_chunk(void *data, size_t size, size_t nmemb, void *userData)
{
  // size is always 1
  size_t real_size = size * nmemb;

  Response *response = (Response *)userData;
  
  // Reallocate buffer to accommodate new chunk
  // New size = existing data + new chunk + null terminator
  char *ptr = (char *)realloc(response->string, response->size + real_size + 1);

  if (ptr == nullptr)
  {
    std::cerr << "Error: Failed to reallocate memory for incoming data chunk" << std::endl;
    return 0;  // Returning 0 signals error to CURL
  }
  
  // Update pointer to newly allocated memory
  response->string = ptr;
  
  // Append new chunk to existing data
  memcpy(&(response->string[response->size]), data, real_size);
  
  // Update size and null-terminate
  response->size += real_size;
  response->string[response->size] = '\0';
  return real_size;
}

/**
 * Parses and formats the JSON response from LeetCode GraphQL API
 * 
 * Extracts and processes the following fields:
 * - title: Question title
 * - content: Question description (HTML converted to plain text)
 * - difficulty: Easy/Medium/Hard
 * - topicTags: Array of topic names
 * - hints: Formatted hints (if available)
 * 
 * Also extracts test cases from the content and creates output JSON file.
 * 
 * @param response Raw JSON response string from HTTP request
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
 * Extracts test cases from LeetCode question content
 * 
 * Parses content to find "Example" sections containing Input/Output pairs.
 * Format expected: "Input: param1 = value1, param2 = value2\nOutput: result"
 * 
 * @param content Formatted question content string
 * @return TestCaseResponse containing test case outputs and parameter name-value pairs
 * 
 * Note: Assumes at least 2 test cases are present in typical LeetCode format.
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
  // Filter out invalid filesystem characters from title to create a safe filename
  std::string title = (*response)["title"];
  const std::string invalid_chars = "\\/:*?\"<>|";
  for (char c : invalid_chars)
  {
    std::replace(title.begin(), title.end(), c, '_');
  }
  
  // Create output directory path relative to the build directory
  // Note: This assumes execution from /workspace/build directory
  std::string outputDir = "../../../Questions";
  std::string jsonName = outputDir + "/" + title + ".txt";

  // Check if output directory exists, create it if it doesn't
  struct stat info;
  if (stat(outputDir.c_str(), &info) != 0)
  {
    std::cerr << "Warning: Output directory '" << outputDir << "' does not exist." << std::endl;
    std::cerr << "Please create the directory or update the path." << std::endl;
    return;
  }
  else if (!(info.st_mode & S_IFDIR))
  {
    std::cerr << "Error: '" << outputDir << "' exists but is not a directory." << std::endl;
    return;
  }

  std::ofstream outputJSON;
  outputJSON.open(jsonName);
  if (!outputJSON.is_open())
  {
    std::cerr << "Error: Failed to create output file '" << jsonName << "'" << std::endl;
    std::cerr << "Please check file permissions and path validity." << std::endl;
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
 * Parses parameter string to extract parameter name and value
 * 
 * Expected format: "paramName = paramValue"
 * Splits into separate name and value strings for easier JSON formatting.
 * 
 * @param param Input string containing parameter assignment
 * @return Pair of (paramName, paramValue) with whitespace removed
 * 
 * Note: This format matches the parameter names used in actual function calls,
 *       making the generated test cases easier to use.
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