#include "process_queries.h"

#include <execution>
#include <algorithm>
#include <deque>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, 
                                                const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> result(queries.size());
    transform(std::execution::par, queries.begin(), queries.end(), result.begin(),
             [&](const std::string& query) {
                 return search_server.FindTopDocuments(query);
             });
    return result;
}

std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server,
                                        const std::vector<std::string>& queries) {
    std::vector<Document> result;
    return transform_reduce(std::execution::par, queries.begin(), queries.end(), result,
             [](std::vector<Document> result, std::vector<Document> documents) {
                 result.insert(result.end(), documents.begin(), documents.end());
                 return result;
             },
             [&](const std::string& query) {
                 return search_server.FindTopDocuments(query);
             });
}