#pragma once

#include <ostream>

template<typename It>
class IteratorRange {
public:
    IteratorRange(It begin, It end);
    
    It begin() const;
    
    It end() const;
    
    size_t size() const;
    
private:
    It begin_, end_;
    size_t size_;
};

template <typename Iterator>
std::ostream& operator<<(std::ostream& out, const IteratorRange<Iterator>& range) {
    for (Iterator it = range.begin(); it != range.end(); ++it) {
        out << *it;
    }
    return out;
}

template <typename It>
IteratorRange<It>::IteratorRange(It begin, It end): begin_(begin), end_(end), size_(distance(begin, end)) {
}

template <typename It>
It IteratorRange<It>::begin() const {
    return begin_;
}

template <typename It>
It IteratorRange<It>::end() const {
    return end_;
}

template <typename It>
size_t IteratorRange<It>::size() const {
    return size_;
}

template <typename It>
class Paginator {
public:
    Paginator(It begin, It end, size_t page_size);
    
    auto begin() const;
    
    auto end() const;
    
    int size() const;

private:
    std::vector<IteratorRange<It>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

template <typename It>
Paginator<It>::Paginator(It begin, It end, size_t page_size) {
    for(size_t left = distance(begin, end); left > 0;) {
        const size_t current_page_size = std::min(left, page_size);
        const It current_page_end = next(begin, current_page_size);
        pages_.push_back({begin, current_page_end});
        
        left -= current_page_size;
        begin = current_page_end;
    }
}

template <typename It>
auto Paginator<It>::begin() const {
    return pages_.begin();
}

template <typename It>
auto Paginator<It>::end() const {
    return pages_.end();
}

template <typename It>
int Paginator<It>::size() const {
    return static_cast<int>(pages_.size());
}