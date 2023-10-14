#pragma once

#include <string>
#include <vector>
#include <tuple>
#include <set>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <deque>
#include <execution>

#include "document.h"
#include "string_processing.h"
#include "log_duration.h"
#include "concurrent_map.h"

const float EPS = 1e-6;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text);
    
    explicit SearchServer(const std::string_view& stop_words_text);

    void AddDocument(int document_id, const std::string_view& document, DocumentStatus status,
                     const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate, typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;

    int GetDocumentCount() const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view& raw_query, int document_id) const;
    
    template <class ExecutionPolicy>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(ExecutionPolicy policy, const std::string_view& raw_query, int document_id) const;

    const std::set<int>::const_iterator begin() const;
    const std::set<int>::const_iterator end() const;
    
    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;
    
    void RemoveDocument(int document_id);
    
    template <class ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };
    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string, double>> id_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids;

    bool IsStopWord(const std::string_view& word) const;

    std::vector<std::string> SplitIntoWordsNoStop(const std::string_view& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    QueryWord ParseQueryWord(std::string_view text) const;

    static bool IsValidWord(const std::string_view& word);

    Query ParseQuery(const std::string_view& text, bool seq = true) const;

    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query,
                          DocumentPredicate document_predicate) const;
    
    template <typename DocumentPredicate, typename ExecutionPolicy>
    std::vector<Document> FindAllDocuments(ExecutionPolicy&& policy, const Query& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
        if(!all_of(stop_words.begin(), stop_words.end(), IsValidWord)) {
            throw std::invalid_argument("Stop words contain invalid word");
        }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const {
    //LOG_DURATION_STREAM("Operation time", std::cout);
    auto query = ParseQuery(raw_query);
    auto found_documents = FindAllDocuments(query, document_predicate);

    sort(found_documents.begin(), found_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (std::abs(lhs.relevance - rhs.relevance) < EPS) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
            });
    if (found_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        found_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return found_documents;
}

template <typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentPredicate document_predicate) const {
    //LOG_DURATION_STREAM("Operation time", std::cout);
    auto query = ParseQuery(raw_query);
    auto found_documents = FindAllDocuments(policy, query, document_predicate);

    sort(policy, found_documents.begin(), found_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (std::abs(lhs.relevance - rhs.relevance) < EPS) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
            });
    if (found_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        found_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return found_documents;
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, 
    [status]([[maybe_unused]] int document_id, DocumentStatus document_status, [[maybe_unused]] int rating) {
            return document_status == status;
        });
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query,
                        DocumentPredicate document_predicate) const {
    return FindAllDocuments(std::execution::seq, query, document_predicate);
}

template <class ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id) {
    document_ids.erase(document_id);
    documents_.erase(document_id);
    
    std::deque<std::string> words;

    for(auto [word, _]: id_to_word_freqs_[document_id]) {
        words.push_back(word);
    }
    for_each(policy, words.begin(), words.end(),
                [&](const std::string& word) {
                    word_to_document_freqs_.at(word).erase(document_id);
                });
    
    id_to_word_freqs_.erase(document_id);
}

template <class ExecutionPolicy>
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(ExecutionPolicy policy, const std::string_view& raw_query, int document_id) const {
    if(document_ids.find(document_id) == document_ids.end()) {
        throw std::out_of_range("Document with id " + std::to_string(document_id) + " doesn't exist");
    }
    const Query query = ParseQuery(raw_query, std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>);
    
    if(std::any_of(policy, query.minus_words.begin(), query.minus_words.end(),
                [&](const std::string_view& word) {
            return (word_to_document_freqs_.count(std::string(word)) && word_to_document_freqs_.at(std::string(word)).count(document_id));
        })) {
        return {{}, documents_.at(document_id).status};
    }
    
    std::vector<std::string_view> matched_words(query.plus_words.size());
    
    auto it = std::copy_if(policy, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), [&](const std::string_view& word) {
        return (word_to_document_freqs_.count(std::string(word)) && word_to_document_freqs_.at(std::string(word)).count(document_id));
    });
    
    std::sort(policy, matched_words.begin(), it);
    matched_words.erase(std::unique(policy, matched_words.begin(), it), matched_words.end());
    return {matched_words, documents_.at(document_id).status};
}

template <typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy&& policy, const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance;
    for_each(policy, query.plus_words.begin(), query.plus_words.end(),
    [&](const std::string_view& word) {
        if (word_to_document_freqs_.count(std::string(word)) == 0) {
            return;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(std::string(word));
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(std::string(word))) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
            }
        }
    });

    for_each(policy, query.minus_words.begin(), query.minus_words.end(),
    [&](const std::string_view& word) {
        if (word_to_document_freqs_.count(std::string(word)) == 0) {
            return;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(std::string(word))) {
            document_to_relevance.Erase(document_id);
        }
    });
    
    std::vector<Document> found_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        found_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }
    return found_documents;
}