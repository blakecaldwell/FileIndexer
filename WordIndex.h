#ifndef WORDINDEX_H
#define WORDINDEX_H

#include <iostream>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>

class WordIndex
{
  struct word
  {
    std::string name;
    int count;

    word(const std::string &n, int c)
    : name(n), count(c)
    {
    }
  };

  typedef boost::multi_index_container<
  word,
    boost::multi_index::indexed_by<
      boost::multi_index::hashed_unique<
        boost::multi_index::member<
        word, std::string, &word::name
          >
        >,
          boost::multi_index::ordered_non_unique<
            // ordered_unique would be cleaner but had unexpected results                                                           
            boost::multi_index::member<
              word, int, &word::count
              >,
            std::greater<int>
            >
      >
  > word_multi;

private:
  word_multi words;
  typedef word_multi::nth_index<0>::type word_find_by_name;
  void incr_count (word &w);

public:
  void insert_word(const std::string & word_str);
  void get_top_words(int topN);
};

#endif
