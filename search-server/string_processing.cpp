#include "string_processing.h"

using namespace std;

vector<string_view> SplitIntoWords(string_view text) {
    vector<string_view> result;
    text.remove_prefix(min(text.find_first_not_of(" "), text.size()));
    const int64_t pos_end = text.npos;
    
    while (text.size()) {
        int64_t space = text.find(' ');
        result.push_back(space == pos_end ? text.substr(0) : text.substr(0, space));
        text.remove_prefix(min(text.find_first_not_of(" ", space), text.size()));
    }

    return result;
}