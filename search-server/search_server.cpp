#include "search_server.h"

#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <cmath>

#include "string_processing.h"

using namespace std;

SearchServer::SearchServer(const string_view& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {
}

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(string_view(stop_words_text)) {
}

void SearchServer::AddDocument(int document_id, const string_view& document, DocumentStatus status,
                    const vector<int>& ratings) {
    if(document_id < 0) {
        throw invalid_argument("Document id should not be less than 0");
    }
    if(document_ids.count(document_id) != 0) {
        throw invalid_argument("Document with the same id already exists");
    }
    if(!IsValidWord(document)) {
        throw invalid_argument("Document contains invalid characters");
    }
    const vector<string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    document_ids.insert(document_id);
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        id_to_word_freqs_[document_id][word] += inv_word_count;
    }
    documents_[document_id] = DocumentData{ComputeAverageRating(ratings), status};
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query, DocumentStatus status) const {
    //LOG_DURATION_STREAM("Operation time"s, std::cout);
    return FindTopDocuments(
        raw_query, [status]([[maybe_unused]] int document_id, DocumentStatus document_status, [[maybe_unused]] int rating) {
            return document_status == status;
        });
}

int SearchServer::GetDocumentCount() const {
    return document_ids.size();
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view& raw_query, int document_id) const {
    if(document_ids.find(document_id) == document_ids.end()) {
        throw out_of_range("Document with id "s + to_string(document_id) + " doesn't exist"s);
    }
    const Query query = ParseQuery(raw_query);
    vector<string_view> matched_words;
    
    for (const string_view& word : query.minus_words) {
        if (word_to_document_freqs_.count(std::string(word)) && word_to_document_freqs_.at(std::string(word)).count(document_id)) {
            return {matched_words, documents_.at(document_id).status};
        }
    }
    
    for (const string_view& word : query.plus_words) {
        if (word_to_document_freqs_.count(std::string(word)) && word_to_document_freqs_.at(std::string(word)).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    return {matched_words, documents_.at(document_id).status};
}

const set<int>::const_iterator SearchServer::begin() const {
    return document_ids.begin();
}

const set<int>::const_iterator SearchServer::end() const {
    return document_ids.end();
}

const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if(document_ids.find(document_id) == document_ids.end()) {
        static map<string, double> empty;
        return empty;
    }
    return id_to_word_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    document_ids.erase(document_id);
    documents_.erase(document_id);
    
    for(auto [word, _]: id_to_word_freqs_.at(document_id)) {
        word_to_document_freqs_.at(word).erase(document_id);
    }
    id_to_word_freqs_.erase(document_id);
}

bool SearchServer::IsStopWord(const string_view& word) const {
    return stop_words_.count(word) > 0;
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string_view& text) const {
    vector<string> words;
    for (const string_view& word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(string(word));
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    bool is_minus = false;
    if(!IsValidWord(text)) {
        throw invalid_argument("Query word '"s + std::string(text) + "' is invalid"s);
    }
    // Word shouldn't be empty
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
        if(text.size() == 0) {
            throw invalid_argument("Query minus word is empty");
        }
        if(text[0] == '-') {
            throw invalid_argument("Query minus word '"s + std::string(text) + "' contains two minuses"s);
        }
    }
    return {text, is_minus, IsStopWord(text)};
}

bool SearchServer::IsValidWord(const string_view& word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

SearchServer::Query SearchServer::ParseQuery(const string_view& text, bool seq) const {
    Query query;
    for (const string_view& word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            } else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }
    if(seq) {
        sort(query.minus_words.begin(), query.minus_words.end());
        query.minus_words.erase(unique(query.minus_words.begin(), query.minus_words.end()),
                                query.minus_words.end());
        sort(query.plus_words.begin(), query.plus_words.end());
        query.plus_words.erase(unique(query.plus_words.begin(), query.plus_words.end()),
                                query.plus_words.end());
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}