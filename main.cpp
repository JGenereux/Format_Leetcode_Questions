#include <curl/curl.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Used to accumulate response data from CURL
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
void CreateJSON(json *response, const TestCaseResponse &testCases);

int main()
{
  std::string questionName;
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
  response.string = static_cast<char *>(malloc(1));
  response.size = 0;

  // Set URL for the HTTP request
  curl_easy_setopt(curl, CURLOPT_URL, "https://leetcode.com/graphql");

  // Set POST data (GraphQL query)
  json query = {
      {"query", "query questionData($titleSlug: String!) { question(titleSlug: $titleSlug) { title content difficulty topicTags { name } hints } }"},
      {"variables", {
                        {"titleSlug", questionName}
                    }}};

  const std::string postData = query.dump();
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());

  // Set headers for JSON data
  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");

  std::string referer = "Referrer: https://leetcode.com/problems/" + questionName + "/";
  headers = curl_slist_append(headers, referer.c_str());

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  // Set write callback function to handle response data
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_chunk);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void *>(&response));

  // Perform the HTTP request
  CURLcode curlResult = curl_easy_perform(curl);
  if (curlResult != CURLE_OK)
  {
    std::cerr << "Error: " << curl_easy_strerror(curlResult) << std::endl;
    curl_easy_cleanup(curl);
    free(response.string);
    return -1;
  }

  formatResponse(response.string);

  // Cleanup
  free(response.string);
  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);

  return 0;
}

// Callback function for CURL to write received data
// Returns the number of bytes processed
size_t write_chunk(void *data, size_t size, size_t nmemb, void *userData)
{
  size_t realSize = size * nmemb;

  Response *response = static_cast<Response *>(userData);
  // Reallocate memory to accommodate the new chunk (+1 for null terminator)
  char *ptr = static_cast<char *>(realloc(response->string, response->size + realSize + 1));

  if (ptr == nullptr)
  {
    std::cerr << "Error: Failed to reallocate memory for response chunk" << std::endl;
    return 0;
  }

  response->string = ptr;
  memcpy(&(response->string[response->size]), data, realSize);
  response->size += realSize;
  response->string[response->size] = '\0';

  return realSize;
}

// Parses the JSON response and extracts question data
// Creates a formatted JSON file with the question details and test cases
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

// Converts HTML content to plain text by removing tags and decoding entities
std::string FormatHTMLToString(const std::string &response)
{
  size_t i = 0;
  std::string result;

  while (i < response.length())
  {
    // Skip HTML tags
    if (response[i] == '<')
    {
      while (i < response.length() && response[i] != '>')
      {
        i++;
      }
      i++;
      continue;
    }

    // Handle HTML entities: &lt; and &gt;
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

    // Handle HTML entity: &amp;
    if (i < response.length() - 5 && (response.substr(i, 5) == "&amp;"))
    {
      result += "&";
      i += 5;
      continue;
    }

    // Handle possessive apostrophe entity
    if (i < response.length() - 6 && response.substr(i, 6) == "&#39;s")
    {
      i += 6;
      continue;
    }

    // Handle non-breaking space entity
    if (i < response.length() - 6 && response.substr(i, 6) == "&nbsp;")
    {
      i += 6;
      continue;
    }

    // Compress multiple newlines into one
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

// Extracts test cases from the problem content
// Returns a TestCaseResponse containing test case outputs and parameters
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
            // Check if we've reached a new parameter
            if (i < content.length() - 1 && (content[i] == ',' && content[i + 1] == ' '))
            {
              tests.testCaseParams.push_back({paramName, paramRes});
              paramName = "";
              paramRes = "";
              j = -1;
              i++;
              continue;
            }
            // Found the equals sign, start looking for parameter value
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

// Creates a JSON output file containing the question details and test cases
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
  // Write all question fields to the output file
  for (auto it = (*response).begin(); it != (*response).end(); ++it)
  {
    outputJSON << "\"" << it.key() << "\"" << ": " << it.value() << ',' << "\n";
  }

  // Write test cases array
  outputJSON << "\"testCases\"" << ": [" << "\n";

  int j = 0;
  int size = tests.testCases.size();
  for (int i = 0; i < size; i++)
  {
    // start inserting new object into array inside json file
    outputJSON << "{\n";

    std::string expectedResult = tests.testCases[i];
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
