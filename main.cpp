#include <curl/curl.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Used to store dynamically allocated HTTP response
struct Response
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
void CreateJSON(json *response, const TestCaseResponse &testCases);

int main()
{
  std::string questionName;
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

  std::cout << "Curl initialized successfully!" << std::endl;

  Response response;
  response.string = static_cast<char *>(malloc(1));
  response.size = 0;

  // Set options for the HTTP request
  curl_easy_setopt(curl, CURLOPT_URL,
                   "https://leetcode.com/graphql");

  // Set POST data (JSON body) to match LeetCode GraphQL query
  json query = {
      {"query", "query questionData($titleSlug: String!) { question(titleSlug: $titleSlug) { title content difficulty topicTags { name } hints } }"},
      {"variables", {{"titleSlug", questionName}}}
  };

  const std::string postData = query.dump();
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());

  // Set headers for JSON data
  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");

  std::string referer = "Referrer: https://leetcode.com/problems/" + questionName + "/";
  headers = curl_slist_append(headers, referer.c_str());

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  // Set write callback function to handle response data
  // curl_easy_perform will call this function repeatedly for each chunk received
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

  // Cleanup
  free(response.string);
  curl_easy_cleanup(curl);
  return 0;
}

// Callback function for handling received HTTP response data
// Returns number of bytes processed
// data: pointer to received data chunk
// size * nmemb: total size of data chunk in bytes
// userData: pointer to Response struct where data is stored
size_t write_chunk(void *data, size_t size, size_t nmemb, void *userData)
{
  // size is always 1
  size_t real_size = size * nmemb;

  Response *response = static_cast<Response *>(userData);
  
  // Allocate more space for the received chunk
  // response->size is the current size, real_size is the new chunk size, +1 for null terminator
  char *ptr = static_cast<char *>(realloc(response->string, response->size + real_size + 1));

  if (ptr == nullptr)
  {
    std::cerr << "Failed to reallocate memory for received chunk" << std::endl;
    return 0;
  }

  // Update response string to the new memory address
  response->string = ptr;
  // Append new portion to existing string
  memcpy(&(response->string[response->size]), data, real_size);
  // Update string size
  response->size += real_size;
  // Add null terminator
  response->string[response->size] = '\0';
  return real_size;
}

// Parse and format the JSON response from LeetCode API
// Extracts: title, content, difficulty, topicTags, and hints
// Formats HTML content and extracts test cases
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
      if (!question.contains(tag))
      {
        continue;
      }

      if (tag == "topicTags")
      {
        std::vector<std::string> topics;
        for (const auto &topic : question[tag])
        {
          topics.push_back(topic["name"]);
        }
        question[tag] = topics;
      }
      else if (tag == "hints")
      {
        if (question[tag][0].size() > 0)
        {
          question[tag][0] = FormatHTMLToString(question[tag][0]);
        }
      }
      else
      {
        question[tag] = FormatHTMLToString(question[tag]);
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

// Convert HTML content to plain text by removing HTML tags and decoding entities
std::string FormatHTMLToString(const std::string &response)
{
  size_t i = 0;
  std::string result;

  while (i < response.length())
  {
    // Remove HTML tags
    if (response[i] == '<')
    {
      while (response[i] != '>')
      {
        i++;
      }
      i++;
      continue;
    }

    // Decode HTML entities: &lt; and &gt;
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

    // Decode HTML entity: &amp;
    if (i < response.length() - 5 && (response.substr(i, 5) == "&amp;"))
    {
      result += "&";
      i += 5;
      continue;
    }

    // Remove &#39;s (apostrophe entity)
    if (i < response.length() - 6 && response.substr(i, 6) == "&#39;s")
    {
      i += 6;
      continue;
    }

    // Remove &nbsp; (non-breaking space)
    if (i < response.length() - 6 && response.substr(i, 6) == "&nbsp;")
    {
      i += 6;
      continue;
    }

    // Collapse multiple newlines into single newline
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

// Extract test cases from the problem content
// Returns test case inputs and expected outputs parsed from Example sections
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
          std::string input;
          std::string paramName;
          std::string paramRes;
          int j = -1;
          while (i < content.length() - 7 && content.substr(i, 7) != "\nOutput")
          {
            // Check if parsing a new parameter
            if (i < content.length() - 1 && (content[i] == ',' && content[i + 1] == ' '))
            {
              tests.testCaseParams.push_back({paramName, paramRes});
              paramName = "";
              paramRes = "";
              j = -1;
              i++;
              continue;
            }
            // Set flag for parameter value parsing
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

void CreateJSON(json *response, const TestCaseResponse &tests)
{
  // Filter out invalid filename characters from title
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
    std::cerr << "Error creating output file for JSON response" << std::endl;
    return;
  }

  outputJSON << "{\n";
  // Write JSON key-value pairs
  for (auto it = (*response).begin(); it != (*response).end(); ++it)
  {
    outputJSON << "\"" << it.key() << "\"" << ": " << it.value() << ',' << "\n";
  }

  // Insert test cases array
  outputJSON << "\"testCases\"" << ": [" << "\n";

  int j = 0;
  int size = tests.testCases.size();
  for (int i = 0; i < size; i++)
  {
    outputJSON << "{\n";
    std::string expectedResult = tests.testCases[i];
    outputJSON << "\"expectedResult\": " << "\"" << expectedResult << "\",\n";

    int numParams = tests.testCaseParams.size() / tests.testCases.size();
    for (int x = 0; x < numParams; x++)
    {
      std::pair<std::string, std::string> fixedParam = tests.testCaseParams[j++];
      outputJSON << "\"" << fixedParam.first << "\": " << "\"" << fixedParam.second << "\"";
      if (x < numParams - 1)
      {
        outputJSON << ",";
      }
      outputJSON << "\n";
    }

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