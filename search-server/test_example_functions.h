#pragma once

#include <iostream>
#include <vector>
#include <set>
#include <map>

template <typename T>
std::ostream& operator <<(std::ostream& out, const std::vector<T>& v) {
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
std::ostream& operator <<(std::ostream& out, const std::set<T>& s) {
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
std::ostream& operator <<(std::ostream& out, const std::map<KeyT, ValueT>& m) {
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
void RunTestImpl(const std::string& test_name, const TestFunc& function) {
    function();
    std::cerr << test_name << " OK" << std::endl;
}

#define RUN_TEST(func)  RunTestImpl(#func, func)

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
                     const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cout << std::boolalpha;
        std::cout << file << "(" << line << "): " << func << ": ";
        std::cout << "ASSERT_EQUAL(" << t_str << ", " << u_str << ") failed: ";
        std::cout << t << " != " << u << ".";
        if (!hint.empty()) {
            std::cout << " Hint: " << hint;
        }
        std::cout << std::endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
                const std::string& hint);

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет исключение документов содержащих минус слова
void TestFindTopDocumentsMinusWords();

// Проверяем матчинг документов
void TestMatchDocument();

// Тест проверяет, что документы найденные в ходе запроса отсортированы в порядке убывания релевантности
void TestSearchRelevance();

// Тест проверяет, что рейтинг добавленного документа подсчитывается корректно
void TestDocumentRatingCalculation();

// Тест проверяет фильтрацию результатов поиска с использованием предиката, задаваемого пользователем
void TestFindTopDocumentsWithLambdaFilter();

// Тест проверяет поиск документов по заданному статусу
void TestFindTopDocumentsWithStatus();

// Тест проверяет корректное вычисление релевантности найденных документов
void TestDocumentRelevanceCalculation();

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent();

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer();

// --------- Окончание модульных тестов поисковой системы -----------