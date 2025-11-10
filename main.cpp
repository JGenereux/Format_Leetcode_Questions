// System headers
#include <cstring>

// External library headers
#include <curl/curl.h>
#include <nlohmann/json.hpp>

// Standard library headers
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using json = nlohmann::json;

// Constants
namespace Constants {
    constexpr const char* LEETCODE_GRAPHQL_URL = "https://leetcode.com/graphql";
    constexpr const char* LEETCODE_PROBLEMS_URL = "https://leetcode.com/problems/";
    constexpr const char* OUTPUT_PATH_PREFIX = "../../../Questions/";
    constexpr const char* INVALID_FILE_CHARS = "\\/:*?\"<>|";
    
    constexpr const char* GRAPHQL_QUERY = 
        "query questionData($titleSlug: String!) { "
        "question(titleSlug: $titleSlug) { "
        "title content difficulty topicTags { name } hints "
        "} }";
}

// Structures
struct Response {
    std::string data;
};

struct TestCaseResponse {
    std::vector<std::string> testCases;
    std::vector<std::pair<std::string, std::string>> testCaseParams;
};

// Function declarations
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userdata);
void ProcessResponse(const std::string& response);
std::string FormatHTMLToString(const std::string& html);
TestCaseResponse ExtractTestCases(const std::string& content);
void CreateOutputFile(const json& questionData, const TestCaseResponse& testCases);

/**
 * Main entry point for the LeetCode question fetcher.
 * Fetches question data from LeetCode's GraphQL API and generates a formatted output file.
 */
int main() {
    std::string questionName;
    std::cout << "Enter LeetCode question name: ";
    std::cin >> questionName;
    
    // Initialize CURL
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Error: Failed to initialize CURL" << std::endl;
        return 1;
    }
    
    std::cout << "CURL initialized successfully!" << std::endl;
    
    Response response;
    
    // Configure HTTP request
    curl_easy_setopt(curl, CURLOPT_URL, Constants::LEETCODE_GRAPHQL_URL);
    
    // Prepare GraphQL query
    json query = {
        {"query", Constants::GRAPHQL_QUERY},
        {"variables", {{"titleSlug", questionName}}}
    };
    
    const std::string postData = query.dump();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    
    // Set HTTP headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    const std::string referer = "Referer: " + std::string(Constants::LEETCODE_PROBLEMS_URL) + questionName + "/";
    headers = curl_slist_append(headers, referer.c_str());
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // Execute HTTP request
    CURLcode result = curl_easy_perform(curl);
    
    // Cleanup CURL resources
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (result != CURLE_OK) {
        std::cerr << "Error: " << curl_easy_strerror(result) << std::endl;
        return 1;
    }
    
    // Process the response
    ProcessResponse(response.data);
    
    return 0;
}

/**
 * CURL callback function for writing received data.
 * Appends data chunks to the response string as they arrive.
 * 
 * @param contents Pointer to delivered data
 * @param size Size of each data element (always 1)
 * @param nmemb Number of data elements
 * @param userdata Pointer to Response struct
 * @return Number of bytes processed
 */
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userdata) {
    const size_t realSize = size * nmemb;
    Response* response = static_cast<Response*>(userdata);
    
    try {
        response->data.append(static_cast<char*>(contents), realSize);
    } catch (const std::bad_alloc& e) {
        std::cerr << "Error: Memory allocation failed: " << e.what() << std::endl;
        return 0;
    }
    
    return realSize;
}

/**
 * Processes the JSON response from LeetCode API.
 * Extracts question data, formats HTML content, and creates output file.
 * 
 * @param response Raw JSON response string
 */
void ProcessResponse(const std::string& response) {
    const std::vector<std::string> tags = {"title", "content", "difficulty", "topicTags", "hints"};
    
    try {
        json parsed = json::parse(response);
        json question = parsed["data"]["question"];
        
        TestCaseResponse testCases;
        
        for (const auto& tag : tags) {
            if (!question.contains(tag)) {
                continue;
            }
            
            if (tag == "topicTags") {
                std::vector<std::string> topics;
                for (const auto& topic : question[tag]) {
                    topics.push_back(topic["name"]);
                }
                question[tag] = topics;
            } else if (tag == "hints") {
                if (!question[tag].empty() && !question[tag][0].empty()) {
                    question[tag][0] = FormatHTMLToString(question[tag][0]);
                }
            } else {
                question[tag] = FormatHTMLToString(question[tag]);
                if (tag == "content") {
                    testCases = ExtractTestCases(question[tag]);
                }
            }
        }
        
        CreateOutputFile(question, testCases);
        
    } catch (const json::parse_error& e) {
        std::cerr << "Error: Failed to parse JSON: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

/**
 * Converts HTML-encoded string to plain text.
 * Removes HTML tags and decodes HTML entities.
 * 
 * @param html HTML-encoded string
 * @return Plain text string
 */
std::string FormatHTMLToString(const std::string& html) {
    std::string result;
    result.reserve(html.length());
    
    for (size_t i = 0; i < html.length(); ++i) {
        // Remove HTML tags
        if (html[i] == '<') {
            while (i < html.length() && html[i] != '>') {
                ++i;
            }
            continue;
        }
        
        // Decode HTML entities
        if (html[i] == '&') {
            // &lt; -> <
            if (html.substr(i, 4) == "&lt;") {
                result += '<';
                i += 3;
                continue;
            }
            // &gt; -> >
            if (html.substr(i, 4) == "&gt;") {
                result += '>';
                i += 3;
                continue;
            }
            // &amp; -> &
            if (html.substr(i, 5) == "&amp;") {
                result += '&';
                i += 4;
                continue;
            }
            // Skip &#39;s
            if (html.substr(i, 6) == "&#39;s") {
                i += 5;
                continue;
            }
            // Skip &nbsp;
            if (html.substr(i, 6) == "&nbsp;") {
                i += 5;
                continue;
            }
        }
        
        // Normalize whitespace - keep single newlines, remove multiple
        if (html[i] == '\n') {
            result += '\n';
            while (i + 1 < html.length() && html[i + 1] == '\n') {
                ++i;
            }
            continue;
        }
        
        // Skip tabs
        if (html[i] == '\t') {
            while (i + 1 < html.length() && html[i + 1] == '\t') {
                ++i;
            }
            continue;
        }
        
        result += html[i];
    }
    
    return result;
}

/**
 * Extracts test cases from the problem content.
 * Parses example inputs and outputs from the formatted problem description.
 * 
 * @param content Formatted problem content
 * @return TestCaseResponse containing test cases and parameters
 */
TestCaseResponse ExtractTestCases(const std::string& content) {
    TestCaseResponse tests;
    
    for (size_t i = 0; i < content.length(); ++i) {
        // Look for "Example" keyword
        if (content.substr(i, 7) != "Example") {
            continue;
        }
        
        i += 7;
        
        // Parse Input section
        while (i < content.length()) {
            if (content.substr(i, 6) == "Input:") {
                i += 6;
                
                std::string paramName;
                std::string paramValue;
                bool parsingValue = false;
                
                while (i < content.length() && content.substr(i, 7) != "\nOutput") {
                    // Handle parameter separator
                    if (i < content.length() - 1 && content[i] == ',' && content[i + 1] == ' ') {
                        if (!paramName.empty() && !paramValue.empty()) {
                            tests.testCaseParams.emplace_back(paramName, paramValue);
                            paramName.clear();
                            paramValue.clear();
                            parsingValue = false;
                        }
                        ++i;
                        ++i;
                        continue;
                    }
                    
                    // Handle equals sign
                    if (content[i] == '=') {
                        parsingValue = true;
                        ++i;
                        continue;
                    }
                    
                    // Parse parameter name or value
                    if (content[i] != ' ') {
                        if (parsingValue) {
                            paramValue += content[i];
                        } else {
                            paramName += content[i];
                        }
                    }
                    
                    ++i;
                }
                
                // Add final parameter
                if (!paramName.empty() && !paramValue.empty()) {
                    tests.testCaseParams.emplace_back(paramName, paramValue);
                }
            }
            
            // Parse Output section
            if (content.substr(i, 6) == "Output") {
                i += 6;
                
                std::string output;
                while (i < content.length() && content[i] != '\n') {
                    if (content[i] != ' ' && content[i] != ':') {
                        output += content[i];
                    }
                    ++i;
                }
                
                if (!output.empty()) {
                    tests.testCases.push_back(output);
                }
                break;
            }
            
            ++i;
        }
    }
    
    return tests;
}

/**
 * Creates a formatted output file with question data and test cases.
 * 
 * @param questionData JSON object containing question information
 * @param testCases Extracted test cases and parameters
 */
void CreateOutputFile(const json& questionData, const TestCaseResponse& testCases) {
    // Sanitize title for use as filename
    std::string title = questionData["title"];
    for (char c : Constants::INVALID_FILE_CHARS) {
        std::replace(title.begin(), title.end(), c, '_');
    }
    
    const std::string filename = std::string(Constants::OUTPUT_PATH_PREFIX) + title + ".txt";
    
    std::ofstream outputFile(filename);
    if (!outputFile.is_open()) {
        std::cerr << "Error: Failed to create output file: " << filename << std::endl;
        return;
    }
    
    // Write question data
    outputFile << "{\n";
    for (auto it = questionData.begin(); it != questionData.end(); ++it) {
        outputFile << "\"" << it.key() << "\": " << it.value() << ",\n";
    }
    
    // Write test cases
    outputFile << "\"testCases\": [\n";
    
    const size_t numTestCases = testCases.testCases.size();
    if (numTestCases > 0) {
        const size_t paramsPerTest = testCases.testCaseParams.size() / numTestCases;
        
        for (size_t i = 0; i < numTestCases; ++i) {
            outputFile << "{\n";
            outputFile << "\"expectedResult\": \"" << testCases.testCases[i] << "\"";
            
            // Write test parameters
            for (size_t j = 0; j < paramsPerTest; ++j) {
                const auto& param = testCases.testCaseParams[i * paramsPerTest + j];
                outputFile << ",\n\"" << param.first << "\": \"" << param.second << "\"";
            }
            
            outputFile << "\n}";
            if (i < numTestCases - 1) {
                outputFile << ",";
            }
            outputFile << "\n";
        }
    }
    
    outputFile << "]\n";
    outputFile << "}\n";
    
    outputFile.close();
    std::cout << "Output file created successfully: " << filename << std::endl;
}
