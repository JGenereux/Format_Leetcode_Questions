#include <curl/curl.h>
#include <cstring>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// FIX: Changed to use std::string for automatic memory management (RAII)
// REASON: Eliminates manual malloc/free and prevents memory leaks
struct Response
{
  std::string data;
};

struct TestCaseResponse
{
  std::vector<std::string> testCases;
  std::vector<std::pair<std::string, std::string>> testCaseParams;
};

// FIX: Updated signature to work with std::string-based Response
size_t write_chunk(void *data, size_t size, size_t nmemb, void *userData);

void formatResponse(const std::string &response);
std::string FormatHTMLToString(const std::string &response);
TestCaseResponse GetTestCases(const std::string &content);

std::pair<std::string, std::string> GetParamName(const std::string &param);
void CreateJSON(json *response, const TestCaseResponse &testCases);

int main()
{
  std::string questionName;
  std::cout << "Enter Leetcode question name: " << std::endl;
  // FIX: Added input validation
  // REASON: Prevent empty input from causing issues
  if (!(std::cin >> questionName) || questionName.empty())
  {
    std::cerr << "Error: Invalid question name provided" << std::endl;
    return 1;
  }

  // FIX: Initialize to nullptr for safety
  CURL *curl = nullptr;
  struct curl_slist *headers = nullptr;

  // Initialize CURL
  curl = curl_easy_init();
  if (curl == nullptr)
  {
    std::cerr << "HTTP REQUEST FAILED: curl_easy_init() failed!" << std::endl;
    return 1;
  }

  std::cout << "Curl initialized successfully!" << std::endl;

  // FIX: Now using std::string - no manual memory management needed
  // REASON: Automatic memory management via RAII
  Response response;

  // Set options for the HTTP request
  curl_easy_setopt(curl, CURLOPT_URL, "https://leetcode.com/graphql");

  // Set Post data (like JSON body) to match leetcode graph ql query
  json query = {
      {"query", "query questionData($titleSlug: String!) { question(titleSlug: $titleSlug) { title content difficulty topicTags { name } hints } }"},
      {"variables", {
                        {"titleSlug", questionName}
                    }}};

  const std::string postData = query.dump();
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());

  // Set headers for JSON data
  headers = curl_slist_append(headers, "Content-Type: application/json");

  std::string referer = "Referrer: https://leetcode.com/problems/" + questionName + "/";
  headers = curl_slist_append(headers, referer.c_str());

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  /**
   * WriteFunction allows for specifying a callback function
   * Curl_easy_perform will call this function repeatedly
   * Each time it is called the pointer is passed to a new chunk of response
   * string
   */
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_chunk);

  // Address of response string is passed in write_chunk as userData
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void *>(&response));

  // Perform the HTTP request
  CURLcode result = curl_easy_perform(curl);
  
  // FIX: Ensure proper cleanup in all paths using RAII pattern
  // REASON: Prevent resource leaks on error paths
  if (result != CURLE_OK)
  {
    std::cerr << "Error: " << curl_easy_strerror(result) << std::endl;
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return 1;
  }

  formatResponse(response.data);
  
  // Cleanup
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return 0;
}

// FIX: Completely rewritten to use std::string
// REASON: Eliminates realloc leak vulnerability where old pointer is lost if realloc fails
// Returns number of bytes in the chunk
// data is set to a ptr that points to block of data received in this chunk
// nmemb is the number of bytes in the block of data
// userData points to where the response string is stored
size_t write_chunk(void *data, size_t size, size_t nmemb, void *userData)
{
  size_t real_size = size * nmemb;
  
  // FIX: Use static_cast instead of C-style cast
  // REASON: Provides compile-time type checking
  Response *response = static_cast<Response *>(userData);
  
  if (response == nullptr || data == nullptr)
  {
    std::cerr << "Error: Null pointer in write_chunk" << std::endl;
    return 0;
  }
  
  try
  {
    // FIX: Use std::string::append which handles memory automatically
    // REASON: No manual memory management, no leak if allocation fails
    response->data.append(static_cast<const char *>(data), real_size);
  }
  catch (const std::bad_alloc &e)
  {
    std::cerr << "Memory allocation error in write_chunk: " << e.what() << std::endl;
    return 0;
  }
  
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
void formatResponse(const std::string &response)
{
  const std::vector<std::string> currentTags = {"title", "content", "difficulty", "topicTags", "hints"};

  try
  {
    json parsed = json::parse(response);
    
    // FIX: Add validation for expected JSON structure
    // REASON: Prevent crashes if API returns unexpected format
    if (!parsed.contains("data") || !parsed["data"].contains("question"))
    {
      std::cerr << "Error: Unexpected JSON structure in response" << std::endl;
      return;
    }
    
    json question = parsed["data"]["question"];
    TestCaseResponse testCases;

    for (const auto &tag : currentTags)
    {
      if (!question.contains(tag))
      {
        continue;
      }
      
      if (tag == "topicTags")
      {
        std::vector<std::string> topics;
        for (const auto &topic : question[tag])
        {
          if (topic.contains("name"))
          {
            topics.push_back(topic["name"]);
          }
        }
        question[tag] = topics;
      }
      else if (tag == "hints")
      {
        // FIX: Check if array is empty, not if first element size is 0
        // REASON: Original code checked size of first element, not array emptiness
        if (!question[tag].empty() && question[tag].is_array())
        {
          for (auto &hint : question[tag])
          {
            if (hint.is_string())
            {
              hint = FormatHTMLToString(hint);
            }
          }
        }
      }
      else
      {
        if (question[tag].is_string())
        {
          question[tag] = FormatHTMLToString(question[tag]);
          // Get testcases from given content
          if (tag == "content")
          {
            testCases = GetTestCases(question[tag]);
          }
        }
      }
    }

    CreateJSON(&question, testCases);
  }
  catch (const json::parse_error &e)
  {
    std::cerr << "Parse error: " << e.what() << std::endl;
  }
  catch (const json::exception &e)
  {
    std::cerr << "JSON error: " << e.what() << std::endl;
  }
}

// FIX: Use size_t for string indices and add bounds checking
// REASON: int can overflow with large strings; prevent infinite loops
std::string FormatHTMLToString(const std::string &response)
{
  std::string result;
  result.reserve(response.length()); // Optimize memory allocation
  
  size_t i = 0;
  const size_t len = response.length();

  while (i < len)
  {
    // FIX: Added bounds checking to prevent infinite loop
    // REASON: Original code could loop forever if '>' not found
    if (response[i] == '<')
    {
      // Find closing '>'
      size_t close_pos = response.find('>', i);
      if (close_pos != std::string::npos)
      {
        i = close_pos + 1;
      }
      else
      {
        // Malformed HTML, skip the '<'
        i++;
      }
      continue;
    }

    // check for &lt; (<) , &gt; (>)
    if (i + 4 <= len && (response.substr(i, 4) == "&lt;" || response.substr(i, 4) == "&gt;"))
    {
      if (response.substr(i, 4) == "&lt;")
      {
        result += "<";
      }
      else
      {
        result += ">";
      }
      i += 4;
      continue;
    }

    // check for &amp; (&)
    if (i + 5 <= len && response.substr(i, 5) == "&amp;")
    {
      result += "&";
      i += 5;
      continue;
    }

    // check for &#39;s (apostrophe s)
    if (i + 6 <= len && response.substr(i, 6) == "&#39;s")
    {
      result += "'s";
      i += 6;
      continue;
    }

    // check for &nbsp; tags
    if (i + 6 <= len && response.substr(i, 6) == "&nbsp;")
    {
      result += " ";
      i += 6;
      continue;
    }

    // check for multiple whitespace characters
    // want to keep 1 where there are multiple
    if (response[i] == '\n')
    {
      result += "\n";
      while (i + 1 < len && response[i + 1] == '\n')
      {
        i++;
      }
      i++;
      continue;
    }

    if (response[i] == '\t')
    {
      result += " "; // Convert tab to space
      while (i + 1 < len && response[i + 1] == '\t')
      {
        i++;
      }
      i++;
      continue;
    }

    result += response[i];
    i++;
  }
  return result;
}

/**
 * Basic test cases given by leetcode are given in a string of the form. Example case & output.
 * Should always be at least 2 test cases given.
 * @returns array of expected outputs for the test cases.
 */
// FIX: Use size_t for string indices and improve bounds checking
// REASON: Prevent integer overflow and out-of-bounds access
TestCaseResponse GetTestCases(const std::string &content)
{
  TestCaseResponse tests;
  const size_t len = content.length();
  
  size_t i = 0;
  while (i < len)
  {
    if (i + 7 <= len && content.substr(i, 7) == "Example")
    {
      i += 7;
      while (i < len)
      {
        if (i + 6 <= len && content.substr(i, 6) == "Input:")
        {
          i += 6;
          std::string paramName;
          std::string paramRes;
          bool foundEquals = false;
          
          while (i + 7 <= len && content.substr(i, 7) != "\nOutput")
          {
            // check if new param is being searched
            if (i + 1 < len && content[i] == ',' && content[i + 1] == ' ')
            {
              if (!paramName.empty() && !paramRes.empty())
              {
                tests.testCaseParams.push_back({paramName, paramRes});
              }
              paramName.clear();
              paramRes.clear();
              foundEquals = false;
              i += 2; // Skip both ',' and ' '
              continue;
            }
            
            // now looking for paramResult so set flag
            if (content[i] == '=')
            {
              foundEquals = true;
              i++;
              continue;
            }

            if (!foundEquals && content[i] != ' ')
            {
              paramName += content[i];
            }
            else if (foundEquals && content[i] != ' ')
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

        if (i + 6 <= len && content.substr(i, 6) == "Output")
        {
          i += 6;
          std::string testCase;
          while (i < len && content[i] != '\n')
          {
            if (content[i] != ' ' && content[i] != ':')
            {
              testCase += content[i];
            }
            i++;
          }
          if (!testCase.empty())
          {
            tests.testCases.push_back(testCase);
          }
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

// FIX: Added validation to prevent division by zero and improved type safety
// REASON: Original code would crash if tests.testCases is empty
void CreateJSON(json *response, const TestCaseResponse &tests)
{
  if (response == nullptr)
  {
    std::cerr << "Error: Null response pointer in CreateJSON" << std::endl;
    return;
  }
  
  // FIX: Validate title exists in response
  if (!response->contains("title") || !(*response)["title"].is_string())
  {
    std::cerr << "Error: Invalid or missing title in response" << std::endl;
    return;
  }
  
  // filter out invalid characters from title
  std::string title = (*response)["title"];
  const std::string invalid_chars = "\\/:*?\"<>|";
  for (char c : invalid_chars)
  {
    std::replace(title.begin(), title.end(), c, '_');
  }
  std::string jsonName = "../../../Questions/" + title + ".txt";

  std::ofstream outputJSON(jsonName);
  if (!outputJSON.is_open())
  {
    std::cerr << "Error creating output file: " << jsonName << std::endl;
    return;
  }

  outputJSON << "{\n";
  // iterates through json response inserting key and value as pair into output file
  for (auto it = (*response).begin(); it != (*response).end(); ++it)
  {
    outputJSON << "\"" << it.key() << "\"" << ": " << it.value() << ',' << "\n";
  }

  // Insert testcases
  outputJSON << "\"testCases\"" << ": [" << "\n";

  // FIX: Prevent division by zero and use size_t for indices
  // REASON: Original code would crash if testCases is empty
  const size_t numTestCases = tests.testCases.size();
  
  if (numTestCases > 0 && tests.testCaseParams.size() % numTestCases == 0)
  {
    const size_t numParamsPerTest = tests.testCaseParams.size() / numTestCases;
    size_t paramIndex = 0;
    
    for (size_t i = 0; i < numTestCases; ++i)
    {
      outputJSON << "{\n";

      const std::string &expectedResult = tests.testCases[i];
      outputJSON << "\"expectedResult\": " << "\"" << expectedResult << "\"";
      
      if (numParamsPerTest > 0)
      {
        outputJSON << ",\n";
      }
      else
      {
        outputJSON << "\n";
      }

      for (size_t x = 0; x < numParamsPerTest; ++x)
      {
        if (paramIndex >= tests.testCaseParams.size())
        {
          std::cerr << "Warning: Parameter index out of bounds" << std::endl;
          break;
        }
        
        const auto &fixedParam = tests.testCaseParams[paramIndex++];
        outputJSON << "\"" << fixedParam.first << "\": " << "\"" << fixedParam.second << "\"";
        
        if (x < numParamsPerTest - 1)
        {
          outputJSON << ",\n";
        }
        else
        {
          outputJSON << "\n";
        }
      }

      if (i < numTestCases - 1)
      {
        outputJSON << "},\n";
      }
      else
      {
        outputJSON << "}\n";
      }
    }
  }
  else if (numTestCases > 0)
  {
    std::cerr << "Warning: Mismatch between test cases and parameters" << std::endl;
  }

  outputJSON << "]\n";
  outputJSON << "}";
  outputJSON.close();
}

/**
 * params are taken from the json as a string containing 'paramName'='param'
 * This function splits the paramName and param separately to label them in the output JSON easier.
 * (the problem function calls explicitly used by the users will contain the same paramNames so makes using them easier as well)
 * NOTE: This function appears to be unused in the current codebase.
 */
// FIX: Use size_t for string indices for consistency
// REASON: Prevent potential overflow with large strings
std::pair<std::string, std::string> GetParamName(const std::string &param)
{
  std::string paramName;
  std::string paramResult;
  bool nameParsed = false;
  
  for (size_t i = 0; i < param.length(); ++i)
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