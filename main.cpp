#include <curl/curl.h>
#include <cstring>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
  std::string questionName = "";
  std::cout << "Enter Leetcode question name: " << std::endl;
  std::cin >> questionName;

  CURL *curl = curl_easy_init();
  if (curl == nullptr)
  {
    std::cerr << "HTTP REQUEST FAILED: curl_easy_init() failed!" << std::endl;
    return -1;
  }

  Response response;
  response.string = (char *)malloc(1);
  response.size = 0;

  curl_easy_setopt(curl, CURLOPT_URL, "https://leetcode.com/graphql");

  json query = {
      {"query", "query questionData($titleSlug: String!) { question(titleSlug: $titleSlug) { title content difficulty topicTags { name } hints } }"},
      {"variables", {{"titleSlug", questionName}}}};

  const std::string postData = query.dump();
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());

  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  std::string referer = "Referrer: https://leetcode.com/problems/" + questionName + "/";
  headers = curl_slist_append(headers, referer.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_chunk);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

  CURLcode result = curl_easy_perform(curl);
  if (result != CURLE_OK)
  {
    std::cerr << "Error: " << curl_easy_strerror(result) << std::endl;
    curl_easy_cleanup(curl);
    return -1;
  }

  formatResponse(response.string);
  free(response.string);
  curl_easy_cleanup(curl);
  return 0;
}

size_t write_chunk(void *data, size_t size, size_t nmemb, void *userData)
{
  size_t real_size = size * nmemb;
  Response *response = (Response *)userData;

  char *ptr = (char *)realloc(response->string, response->size + real_size + 1);
  if (ptr == nullptr)
  {
    std::cerr << "Problem reallocating space for chunk received" << std::endl;
    return 0;
  }

  response->string = ptr;
  memcpy(&(response->string[response->size]), data, real_size);
  response->size += real_size;
  response->string[response->size] = '\0';

  return real_size;
}

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

std::string FormatHTMLToString(const std::string &response)
{
  int i = 0;
  std::string result = "";

  while (i < response.length())
  {
    if (response[i] == '<')
    {
      while (response[i] != '>')
      {
        i++;
      }
      i++;
      continue;
    }

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

    if (i < response.length() - 5 && (response.substr(i, 5) == "&amp;"))
    {
      result += "&";
      i += 5;
      continue;
    }

    if (i < response.length() - 6 && response.substr(i, 6) == "&#39;s")
    {
      i += 6;
      continue;
    }

    if (i < response.length() - 6 && response.substr(i, 6) == "&nbsp;")
    {
      i += 6;
      continue;
    }

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
  for (auto it = (*response).begin(); it != (*response).end(); ++it)
  {
    outputJSON << "\"" << it.key() << "\"" << ": " << it.value() << ',' << "\n";
  }

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