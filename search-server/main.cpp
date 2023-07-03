#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>
#include <deque>

using namespace std;

#define EPS 1e-6

const int MAX_RESULT_DOCUMENT_COUNT = 5;

template <typename Iterator>
void PrintRange(const Iterator begin, const Iterator end) {
    for(Iterator start = begin; start != end; start++) {
        std::cout << *start << " ";
    }
    std::cout << std::endl;
}

template <typename Container, typename Element>
void FindAndPrint(const Container& container, const Element& element_to_find) {
    auto it_begin = begin(container);
    auto it_end = end(container);
    auto it = find(it_begin, it_end, element_to_find);
    PrintRange(it_begin, it);
    PrintRange(it, it_end);
}

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

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

ostream& operator <<(ostream& out, const Document document) {
    out << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s;
    return out;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

template<typename It>
class IteratorRange {
public:
    IteratorRange(It begin, It end): begin_(begin), end_(end), size_(distance(begin, end)) {
    }
    
    It begin() const {
        return begin_;
    }
    
    It end() const {
        return end_;
    }
    
    size_t size() const {
        return size_;
    }
    
private:
    It begin_, end_;
    size_t size_;
};

template <typename Iterator>
ostream& operator<<(ostream& out, const IteratorRange<Iterator>& range) {
    for (Iterator it = range.begin(); it != range.end(); ++it) {
        out << *it;
    }
    return out;
}

template <typename It>
class Paginator {
public:
    Paginator(It begin, It end, size_t page_size) {
        for(size_t left = distance(begin, end); left > 0;) {
            const size_t current_page_size = min(left, page_size);
            const It current_page_end = next(begin, current_page_size);
            pages_.push_back({begin, current_page_end});
            
            left -= current_page_size;
            begin = current_page_end;
        }
    }
    
    auto begin() const {
        return pages_.begin();
    }
    
    auto end() const {
        return pages_.end();
    }
    
    int size() const {
        return static_cast<int>(pages_.size());
    }

private:
    vector<IteratorRange<It>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

// --------------- Окончание вспомогательных функций и структур ----------------

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
            if(!all_of(stop_words.begin(), stop_words.end(), IsValidWord)) {
                throw invalid_argument("Stop words contain invalid word");
            }
    }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        if(document_id < 0) {
            throw invalid_argument("Document id should not be less than 0");
        }
        if(documents_.count(document_id) != 0) {
            throw invalid_argument("Document with the same id already exists");
        }
        if(!IsValidWord(document)) {
            throw invalid_argument("Document contains invalid characters");
        }
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        document_ids.push_back(document_id);
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query,
                                      DocumentPredicate document_predicate) const {
        auto query = ParseQuery(raw_query);
        auto found_documents = FindAllDocuments(query, document_predicate);

        sort(found_documents.begin(), found_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < EPS) {
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

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(
            raw_query, [status]([[maybe_unused]] int document_id, DocumentStatus document_status, [[maybe_unused]] int rating) {
                return document_status == status;
            });
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
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

    int GetDocumentId(int index) const {
        return document_ids.at(index);
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> document_ids;

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
        return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        if(!IsValidWord(text)) {
            throw invalid_argument("Query word '"s + text + "' is invalid"s);
        }
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
            if(text.size() == 0) {
                throw invalid_argument("Query minus word is empty");
            }
            if(text[0] == '-') {
                throw invalid_argument("Query minus word '"s + text + "' contains two minuses"s);
            }
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    static bool IsValidWord(const string& word) {
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }

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

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query,
                          DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
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
        
        vector<Document> found_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            found_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
        }
        return found_documents;
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
        SearchServer server(""s);
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
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<string> expected_match_result = { "cat", "city", "the" };
        vector<string> result = get<vector<string>>(server.MatchDocument("cat outside the city", doc_id));
        sort(result.begin(), result.end());
        ASSERT_EQUAL(result, expected_match_result);
    }

    // Убеждаемся что при наличии в документе минус слова вернется пустой список слов
    {
        SearchServer server(""s);
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
        SearchServer server(""s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        vector<Document> result = server.FindTopDocuments("cat in the city");
        ASSERT(is_sorted(result.begin(), result.end(), [](const Document& left, const Document& right) {
            return left.relevance > right.relevance;
        }));
    }
}

// Тест проверяет, что рейтинг добавленного документа подсчитывается корректно
void TestDocumentRatingCalculation() {
    const int doc_id = 1;
    const string content = "cat in the park"s;
    const vector<int> ratings_positive = { 2, 5, 3 };
    const vector<int> ratings_negative = { -3, -4, -2};
    const vector<int> ratings_mixed = {5, -4, 8, -5};

    // Проверяем, что рейтинг добавленного документа верно подсчитывается для положительных значений
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings_positive);
        vector<Document> result = server.FindTopDocuments("cat in the park");
        int expected_rating = accumulate(ratings_positive.begin(), ratings_positive.end(), 0) / static_cast<int>(ratings_positive.size());
        ASSERT_EQUAL(result.at(0).rating, expected_rating);
    }

    // Проверяем, что рейтинг добавленного документа верно подсчитывается для отрицательных значений
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings_negative);
        vector<Document> result = server.FindTopDocuments("cat in the park");
        int expected_rating = accumulate(ratings_negative.begin(), ratings_negative.end(), 0) / static_cast<int>(ratings_negative.size());
        ASSERT_EQUAL(result.at(0).rating, expected_rating);
    }

    // Проверяем, что рейтинг добавленного документа верно подсчитывается для смешанных значений
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings_mixed);
        vector<Document> result = server.FindTopDocuments("cat in the park");
        int expected_rating = accumulate(ratings_mixed.begin(), ratings_mixed.end(), 0) / static_cast<int>(ratings_mixed.size());
        ASSERT_EQUAL(result.at(0).rating, expected_rating);
    }
}

// Тест проверяет фильтрацию результатов поиска с использованием предиката, задаваемого пользователем
void TestFindTopDocumentsWithLambdaFilter() {
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
        SearchServer server(""s);
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

    SearchServer server(""s);
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
        SearchServer server(""s);
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
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server("in the"s);
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
    RUN_TEST(TestFindTopDocumentsWithLambdaFilter);
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

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server)
        : search_server_(search_server)
        , no_results_requests_(0)
        , current_time_(0) {
    }
    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    vector<Document> AddFindRequest(const string& raw_query, DocumentPredicate document_predicate) {
        vector<Document> result = search_server_.FindTopDocuments(raw_query, document_predicate);
        AddRequest(result.size());
        return result;
    }
    vector<Document> AddFindRequest(const string& raw_query, DocumentStatus status) {
        vector<Document> result = search_server_.FindTopDocuments(raw_query, status);
        AddRequest(result.size());
        return result;
    }
    vector<Document> AddFindRequest(const string& raw_query) {
        vector<Document> result = search_server_.FindTopDocuments(raw_query);
        AddRequest(result.size());
        return result;
    }
    int GetNoResultRequests() const {
        return no_results_requests_;
    }
private:
    struct QueryResult {
        uint64_t timestamp;
        int results;
    };
    deque<QueryResult> requests_;
    const SearchServer& search_server_;
    int no_results_requests_;
    uint64_t current_time_;
    const static int min_in_day_ = 1440;
 
    void AddRequest(int results_num) {
        // новый запрос - новая секунда
        ++current_time_;
        // удаляем все результаты поиска, которые устарели
        while (!requests_.empty() && min_in_day_ <= current_time_ - requests_.front().timestamp) {
            if (0 == requests_.front().results) {
                --no_results_requests_;
            }
            requests_.pop_front();
        }
        // сохраняем новый результат поиска
        requests_.push_back({current_time_, results_num});
        if (0 == results_num) {
            ++no_results_requests_;
        }
    }
};

int main() {
    SearchServer search_server("and in at"s);
    RequestQueue request_queue(search_server);
    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});
    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("empty request"s);
    }
    // все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("curly dog"s);
    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    request_queue.AddFindRequest("big collar"s);
    // первый запрос удален, 1437 запросов с нулевым результатом
    request_queue.AddFindRequest("sparrow"s);
    cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << endl;
    return 0;
}