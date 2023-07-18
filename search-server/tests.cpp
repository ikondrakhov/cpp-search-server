#include "tests.h"
#include "search_server.h"

#include <algorithm>
#include <numeric>
#include <cmath>

using namespace std;

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