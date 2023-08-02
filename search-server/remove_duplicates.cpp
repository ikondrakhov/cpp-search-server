#include <algorithm>

#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    set<set<string>> documents_words;
    vector<int> ids_to_remove;
    for(int document_id: search_server) {
        set<string> words;
        for(auto [word, _]: search_server.GetWordFrequencies(document_id)) {
            words.insert(word);
        }
        if(documents_words.count(words)) {
            ids_to_remove.push_back(document_id);
        }
        documents_words.insert(words);
    }
    
    for(int id: ids_to_remove) {
        cout << "Found duplicate document id " << id << endl;
        search_server.RemoveDocument(id);
    }
}