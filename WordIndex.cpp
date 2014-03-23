#include "WordIndex.h"

void WordIndex::incr_count (word &w)
{
  ++w.count;
}

void WordIndex::insert_word(const std::string & word_str) {
  // Search through list for the word. According to Boost documentation                                                             
  // this should be O(log n). If existing entry is found, modify it (no copy).                                                      
  // Do regular insert if no matches                                                                                                

  word_find_by_name& hash_index=words.get<0>();
  word_find_by_name::iterator it=hash_index.find(word_str);

  std::cout << "adding word: " << word_str << std::endl;
  if (it != hash_index.end()) {
    if (hash_index.modify(it,boost::bind(&WordIndex::incr_count,this,_1)))
      std::cout << "modify successful. \" " << it->name << " => " << it->count << " \"\n";
    else {
      std::cout << "modify failed.\n";
    }
  }
  else {
    word w(word_str,1);
    auto result = words.insert(w);
    if (!result.second) {
      std::cout << "insert failed.\n";
    }
    else {
      std::cout << "insert succeed.\n";
    }
  }

  std::cout << std::endl;
}

void WordIndex::get_top_words(int topN) {
  // List is already sorted to O(n) to find n words                                                                                 

  typedef word_multi::nth_index<1>::type::iterator iterator_type;
  iterator_type it, count_index_end;
  it = words.get<1>().begin();
  count_index_end = words.get<1>().end();

  // print the topN most frequently seen words                                                                                      
  for (int i = 0; (i < topN) && (it != count_index_end) ; ++i) {
    std::cout << it->name << " => " << it->count  << std::endl;
    ++it;
  }
}

