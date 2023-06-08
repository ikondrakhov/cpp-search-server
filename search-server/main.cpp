#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

// --------- Начало вспомогательных функций и структур -------------
string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

// --------------- Окончание вспомогательных функций и структур ----------------

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template <typename FilterFunc>
    vector<Document> FindTopDocuments(const string& raw_query, FilterFunc filter) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, filter);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }
    
    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }
    
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus required_status) const {
        return FindTopDocuments(raw_query, [required_status]([[maybe_unused]] int id, DocumentStatus status, [[maybe_unused]] int rating) { return status == required_status; });
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                        int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename FilterFunc>
    vector<Document> FindAllDocuments(const Query& query, FilterFunc filter) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (filter(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

// ----------- Начало тестового фреймворка ------------

template <typename T>
ostream& operator <<(ostream& out, const vector<T>& v) {
    out << "[";
    for(const T& e: v) {
        if(e != v[v.size() - 1]) {
            out << e << ", ";
        } else {
            out << e;
        }
    }
    out << "]";
    return out;
}

template <typename T>
ostream& operator <<(ostream& out, const set<T>& s) {
    out << "{";
    for(const T& e: s) {
        if(e != *s.rbegin()) {
            out << e << ", ";
        } else {
            out << e;
        }
    }
    out << "}";
    return out;
}

template <typename KeyT, typename ValueT>
ostream& operator <<(ostream& out, const map<KeyT, ValueT>& m) {
    out << "{";
    for(const auto& [key, value]: m) {
        if(key != m.rbegin()->first) {
            out << key << ": " << value << ", ";
        } else {
            out << key << ": " << value;
        }
    }
    out << "}";
    return out;
}

template <typename TestFunc>
void RunTestImpl(const string& test_name, const TestFunc& function) {
    function();
    cerr << test_name << " OK" << endl;
}

#define RUN_TEST(func)  RunTestImpl(#func, func)

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

// -------- Окончание тестового фреймворка --------------

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет исключение документов содержащих минус слова
void TestFindTopDocumentsMinusWords() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    // Убеждаемся, что документ не будет найден, если запрос содержит минус слово содержащееся в документе
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("-city").empty());
    }

}

// Проверяем матчинг документов
void TestMatchDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    // Убеждаемся, что при матчинге документа возвращаются все слова поискового запроса присутствующие в документе
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<string> expected_match_result = { "cat", "city", "the" };
        vector<string> result = get<vector<string>>(server.MatchDocument("cat outside the city", doc_id));
        sort(result.begin(), result.end());
        ASSERT_EQUAL(result, expected_match_result);
    }

    // Убеждаемся что при наличии в документе минус слова вернется пустой список слов
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(get<vector<string>>(server.MatchDocument("cat outside the -city", doc_id)).empty());
    }
}

// Тест проверяет, что документы найденные в ходе запроса отсортированы в порядке убывания релевантности
void TestSearchRelevance() {
    const int doc_id_1 = 1;
    const string content_1 = "cat in the"s;
    const vector<int> ratings_1 = { 1, 2, 3 };

    const int doc_id_2 = 2;
    const string content_2 = "cat the"s;
    const vector<int> ratings_2 = { 1, 2, 3 };

    const int doc_id_3 = 3;
    const string content_3 = "cat in the city"s;
    const vector<int> ratings_3 = { 1, 2, 3 };

    // Проверяем, что найденые документы будут отсортированы по релевантности
    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        vector<Document> result = server.FindTopDocuments("cat in the city");
        ASSERT_EQUAL(result.at(0).id, 3);
        ASSERT_EQUAL(result.at(1).id, 1);
        ASSERT_EQUAL(result.at(2).id, 2);
    }
}

// Тест проверяет, что рейтинг добавленного документа подсчитывается корректно
void TestDocumentRatingCalculation() {
    const int doc_id_1 = 1;
    const string content_1 = "cat in the park"s;
    const vector<int> ratings_1 = { 2, 5, 3 };

    // Проверяем, что рейтинг добавленного документа верно подсчитывается
    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        vector<Document> result = server.FindTopDocuments("cat in the park");
        ASSERT_EQUAL(result.at(0).rating, 3);
    }

}

// Тест проверяет фильтрацию результатов поиска с использованием предиката, задаваемого пользователем
void TestFindTopDocumentsWithLabmdaFilter() {
    const int doc_id_1 = 1;
    const string content_1 = "cat in the park"s;
    const vector<int> ratings_1 = { 4, 5, 4 };

    const int doc_id_2 = 2;
    const string content_2 = "cat in the park"s;
    const vector<int> ratings_2 = { 5, 5, 5 };

    const int doc_id_3 = 3;
    const string content_3 = "cat in the park"s;
    const vector<int> ratings_3 = { 2, 2, 2 };

    const int doc_id_4 = 4;
    const string content_4 = "cat in the park"s;
    const vector<int> ratings_4 = { 5, 5, 5 };

    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::BANNED, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        server.AddDocument(doc_id_4, content_4, DocumentStatus::ACTUAL, ratings_4);
        vector<Document> result = server.FindTopDocuments("cat in the park", [](int id, DocumentStatus status, int rating) {
            return id > 1 && status == DocumentStatus::ACTUAL && rating > 3;
            });
        ASSERT_EQUAL(result.size(), 1u);
        ASSERT_EQUAL(result[0].id, 4);
    }
}

// Тест проверяет поиск документов по заданному статусу
void TestFindTopDocumentsWithStatus() {
    const string content = "cat in the park"s;
    const vector<int> ratings = { 4, 5, 4 };
    const vector<DocumentStatus> statuses = { DocumentStatus::ACTUAL, DocumentStatus::BANNED,
        DocumentStatus::IRRELEVANT, DocumentStatus::REMOVED };

    SearchServer server;
    for (int id = 0; id < static_cast<int>(statuses.size()); id++) {
        server.AddDocument(id, content, statuses[id], ratings);
    }

    for (int id = 0; id < static_cast<int>(statuses.size()); id++) {
        vector<Document> actual = server.FindTopDocuments(content, statuses[id]);
        ASSERT_EQUAL(actual.size(), 1u);
        ASSERT_EQUAL(actual[0].id, id);
    }
}

// Тест проверяет корректное вычисление релевантности найденных документов
void TestDocumentRelevanceCalculation() {
    const int doc_id_1 = 1;
    const string content_1 = "the cat"s;
    const vector<int> ratings_1 = { 4, 5, 4 };

    const int doc_id_2 = 2;
    const string content_2 = "dog in park"s;
    const vector<int> ratings_2 = { 4, 5, 4 };

    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        vector<Document> result = server.FindTopDocuments("cat in park");

        double document_1_relevance = log(2.0 / 1) * 0.5;
        double document_2_relevance = log(2.0 / 1) * 1.0 / 3 + log(2.0 / 1) * 1.0 / 3;

        ASSERT_EQUAL(result.size(), 2u);
        ASSERT_EQUAL(result[0].relevance, max(document_1_relevance, document_2_relevance));
        ASSERT_EQUAL(result[1].relevance, min(document_1_relevance, document_2_relevance));
    }
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents"s);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestFindTopDocumentsMinusWords);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestSearchRelevance);
    RUN_TEST(TestDocumentRatingCalculation);
    RUN_TEST(TestFindTopDocumentsWithLabmdaFilter);
    RUN_TEST(TestFindTopDocumentsWithStatus);
    RUN_TEST(TestDocumentRelevanceCalculation);
}

// --------- Окончание модульных тестов поисковой системы -----------

// ==================== для примера =========================

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

int main() {
    TestSearchServer();

    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, [[maybe_unused]] DocumentStatus status, [[maybe_unused]] int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }
    return 0;
}