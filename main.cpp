#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstring>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Constants
namespace {
    constexpr const char* LEETCODE_GRAPHQL_URL = "https://leetcode.com/graphql";
    constexpr const char* LEETCODE_PROBLEMS_URL = "https://leetcode.com/problems/";
    constexpr const char* OUTPUT_DIR = "../../../Questions/";
    constexpr const char* INVALID_FILENAME_CHARS = "\\/:*?\"<>|";
}

// Struct to hold dynamic HTTP response data
struct Response {
    char* data;
    size_t size;
};

// Struct to hold parsed test case information
struct TestCaseResponse {
    std::vector<std::string> expectedOutputs;
    std::vector<std::pair<std::string, std::string>> parameters;
};

// Function declarations
size_t WriteChunkCallback(void* data, size_t size, size_t nmemb, void* userData);
void FormatAndSaveResponse(char* response);
std::string ConvertHtmlToPlainText(const std::string& html);
TestCaseResponse ExtractTestCases(const std::string& content);
void CreateOutputFile(json* questionData, const TestCaseResponse& testCases);

int main() {
    // Get question name from user
    std::string questionName;
    std::cout << "Enter Leetcode question name: " << std::endl;
    std::cin >> questionName;

    // Initialize CURL
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        std::cerr << "HTTP REQUEST FAILED: curl_easy_init() failed!" << std::endl;
        return -1;
    }
    std::cout << "Curl initialized successfully!" << std::endl;

    // Initialize response buffer
    Response response;
    response.data = static_cast<char*>(malloc(1));
    response.size = 0;

    // Configure CURL request
    curl_easy_setopt(curl, CURLOPT_URL, LEETCODE_GRAPHQL_URL);

    // Build GraphQL query
    json query = {
        {"query", "query questionData($titleSlug: String!) { question(titleSlug: $titleSlug) { title content difficulty topicTags { name } hints } }"},
        {"variables", {{"titleSlug", questionName}}}
    };

    const std::string postData = query.dump();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());

    // Set HTTP headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string referer = "Referrer: " + std::string(LEETCODE_PROBLEMS_URL) + questionName + "/";
    headers = curl_slist_append(headers, referer.c_str());

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Set callback function to handle response data
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteChunkCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void*>(&response));

    // Perform HTTP request
    CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        std::cerr << "Error: " << curl_easy_strerror(result) << std::endl;
        free(response.data);
        curl_easy_cleanup(curl);
        return -1;
    }

    // Process and save the response
    FormatAndSaveResponse(response.data);

    // Cleanup
    free(response.data);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    return 0;
}

/**
 * CURL callback function to handle incoming data chunks.
 * Dynamically reallocates memory to store the complete response.
 *
 * @param data Pointer to the received data chunk
 * @param size Size of each element (always 1)
 * @param nmemb Number of elements in the data chunk
 * @param userData Pointer to the Response struct
 * @return Number of bytes processed
 */
size_t WriteChunkCallback(void* data, size_t size, size_t nmemb, void* userData) {
    size_t realSize = size * nmemb;
    Response* response = static_cast<Response*>(userData);

    // Reallocate memory to accommodate the new chunk
    char* ptr = static_cast<char*>(realloc(response->data, response->size + realSize + 1));
    if (ptr == nullptr) {
        std::cerr << "Failed to reallocate memory for response chunk" << std::endl;
        return 0;
    }

    // Append new data to existing buffer
    response->data = ptr;
    memcpy(&(response->data[response->size]), data, realSize);
    response->size += realSize;
    response->data[response->size] = '\0';

    return realSize;
}

/**
 * Parses the JSON response from LeetCode API and formats the content.
 * Processes title, content, difficulty, topic tags, and hints.
 * Extracts test cases from content and saves everything to a file.
 *
 * @param response Raw JSON response string from the API
 */
void FormatAndSaveResponse(char* response) {
    const std::vector<std::string> EXPECTED_TAGS = {"title", "content", "difficulty", "topicTags", "hints"};

    try {
        json parsed = json::parse(response);
        json question = parsed["data"]["question"];

        TestCaseResponse testCases;

        // Process each tag in the response
        for (const auto& tag : EXPECTED_TAGS) {
            if (!question.contains(tag)) {
                continue;
            }

            if (tag == "topicTags") {
                // Convert topic tags array to simple string array
                std::vector<std::string> topics;
                for (const auto& topic : question[tag]) {
                    topics.push_back(topic["name"]);
                }
                question[tag] = topics;
            } 
            else if (tag == "hints") {
                // Format first hint if it exists
                if (!question[tag].empty() && !question[tag][0].empty()) {
                    question[tag][0] = ConvertHtmlToPlainText(question[tag][0]);
                }
            } 
            else {
                // Format HTML content to plain text
                question[tag] = ConvertHtmlToPlainText(question[tag]);
                
                // Extract test cases from content
                if (tag == "content") {
                    testCases = ExtractTestCases(question[tag]);
                }
            }
        }

        CreateOutputFile(&question, testCases);
    }
    catch (const json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }
}

/**
 * Converts HTML-formatted text to plain text by removing HTML tags
 * and decoding HTML entities.
 *
 * @param html HTML-formatted string
 * @return Plain text string with HTML removed and entities decoded
 */
std::string ConvertHtmlToPlainText(const std::string& html) {
    std::string result;
    result.reserve(html.length());

    size_t i = 0;
    while (i < html.length()) {
        // Remove HTML tags
        if (html[i] == '<') {
            while (i < html.length() && html[i] != '>') {
                i++;
            }
            i++;
            continue;
        }

        // Decode HTML entities
        if (i + 4 <= html.length()) {
            std::string entity = html.substr(i, 4);
            if (entity == "&lt;") {
                result += "<";
                i += 4;
                continue;
            } else if (entity == "&gt;") {
                result += ">";
                i += 4;
                continue;
            }
        }

        if (i + 5 <= html.length() && html.substr(i, 5) == "&amp;") {
            result += "&";
            i += 5;
            continue;
        }

        if (i + 6 <= html.length()) {
            std::string entity = html.substr(i, 6);
            if (entity == "&#39;s" || entity == "&nbsp;") {
                i += 6;
                continue;
            }
        }

        // Collapse multiple newlines into one
        if (html[i] == '\n') {
            result += "\n";
            while (i + 1 < html.length() && html[i + 1] == '\n') {
                i++;
            }
            i++;
            continue;
        }

        // Skip multiple tabs
        if (html[i] == '\t') {
            while (i + 1 < html.length() && html[i + 1] == '\t') {
                i++;
            }
            i++;
            continue;
        }

        result += html[i];
        i++;
    }

    return result;
}

/**
 * Extracts test case inputs and expected outputs from the problem content.
 * Parses "Example" sections to find Input and Output pairs.
 *
 * @param content The problem description content
 * @return TestCaseResponse containing test case parameters and expected outputs
 */
TestCaseResponse ExtractTestCases(const std::string& content) {
    TestCaseResponse tests;

    size_t i = 0;
    while (i < content.length()) {
        // Look for "Example" sections
        if (i + 7 <= content.length() && content.substr(i, 7) == "Example") {
            i += 7;

            // Parse Input section
            while (i < content.length()) {
                if (i + 6 <= content.length() && content.substr(i, 6) == "Input:") {
                    i += 6;

                    std::string paramName;
                    std::string paramValue;
                    bool parsingValue = false;

                    // Parse input parameters until we hit "Output"
                    while (i + 7 < content.length() && content.substr(i, 7) != "\nOutput") {
                        // Handle parameter separator
                        if (i + 1 < content.length() && content[i] == ',' && content[i + 1] == ' ') {
                            if (!paramName.empty() && !paramValue.empty()) {
                                tests.parameters.push_back({paramName, paramValue});
                            }
                            paramName.clear();
                            paramValue.clear();
                            parsingValue = false;
                            i++;
                            continue;
                        }

                        // Switch to parsing value after '='
                        if (content[i] == '=') {
                            parsingValue = true;
                            i++;
                            continue;
                        }

                        // Collect parameter name or value (skip spaces)
                        if (content[i] != ' ') {
                            if (parsingValue) {
                                paramValue += content[i];
                            } else {
                                paramName += content[i];
                            }
                        }
                        i++;
                    }

                    // Add last parameter
                    if (!paramName.empty() && !paramValue.empty()) {
                        tests.parameters.push_back({paramName, paramValue});
                    }
                }

                // Parse Output section
                if (i + 6 <= content.length() && content.substr(i, 6) == "Output") {
                    i += 6;

                    std::string output;
                    while (i < content.length() && content[i] != '\n') {
                        if (content[i] != ' ' && content[i] != ':') {
                            output += content[i];
                        }
                        i++;
                    }

                    if (!output.empty()) {
                        tests.expectedOutputs.push_back(output);
                    }
                    break;
                }
                i++;
            }
        } else {
            i++;
        }
    }

    return tests;
}

/**
 * Creates a JSON output file containing the formatted question data and test cases.
 * The file is saved in the Questions directory with the question title as filename.
 *
 * @param questionData Pointer to JSON object containing formatted question data
 * @param tests TestCaseResponse containing test cases and parameters
 */
void CreateOutputFile(json* questionData, const TestCaseResponse& tests) {
    // Sanitize title for use as filename
    std::string title = (*questionData)["title"];
    for (char invalidChar : std::string(INVALID_FILENAME_CHARS)) {
        std::replace(title.begin(), title.end(), invalidChar, '_');
    }

    std::string filename = std::string(OUTPUT_DIR) + title + ".txt";

    // Open output file
    std::ofstream outputFile(filename);
    if (!outputFile.is_open()) {
        std::cerr << "Error: Failed to create output file: " << filename << std::endl;
        return;
    }

    // Write question data
    outputFile << "{\n";
    for (auto it = questionData->begin(); it != questionData->end(); ++it) {
        outputFile << "\"" << it.key() << "\": " << it.value() << ",\n";
    }

    // Write test cases
    outputFile << "\"testCases\": [\n";

    const size_t numTestCases = tests.expectedOutputs.size();
    if (numTestCases > 0) {
        const size_t paramsPerTest = tests.parameters.size() / numTestCases;
        size_t paramIndex = 0;

        for (size_t i = 0; i < numTestCases; i++) {
            outputFile << "{\n";
            outputFile << "\"expectedResult\": \"" << tests.expectedOutputs[i] << "\"";

            // Write parameters for this test case
            for (size_t j = 0; j < paramsPerTest && paramIndex < tests.parameters.size(); j++) {
                const auto& param = tests.parameters[paramIndex++];
                outputFile << ",\n\"" << param.first << "\": \"" << param.second << "\"";
            }

            outputFile << "\n}";
            if (i < numTestCases - 1) {
                outputFile << ",";
            }
            outputFile << "\n";
        }
    }

    outputFile << "]\n}\n";
    outputFile.close();

    std::cout << "Successfully created output file: " << filename << std::endl;
}
