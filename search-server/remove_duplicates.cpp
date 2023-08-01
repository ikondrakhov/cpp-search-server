#include <algorithm>

#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    map<set<string>, vector<int>> words_to_ids;
    for(int document_id: search_server) {
        set<string> words;
        for(auto [word, _]: search_server.GetWordFrequencies(document_id)) {
            words.insert(word);
        }
        words_to_ids[words].push_back(document_id);
    }
    
    for(auto [_, ids]: words_to_ids) {
        sort(ids.begin(), ids.end(), [](int idl, int idr) {
            return idl > idr;
        });
        for(int i = 0; i < ids.size() - 1; i++) {
            cout << "Found duplicate document id " << ids[i] << endl;
            search_server.RemoveDocument(ids[i]);
        }
    }
}