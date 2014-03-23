#include "FileWorker.h"

void FileWorker::index_token(const char * token) {
  if (Index.find(token) != Index.end())
    // if token was found in map
    Index[token] += 1;
  else
    // else add it with count = 1
    Index[token] = 1;
}

void FileWorker::read_page(std::ifstream &fh, char *page)
{
  std::cout << "+ read_page()\n";
  fh.read(page, READ_PAGE_SIZE);
  // That could fail (eof), so we make sure to add the null terminator at the end of the 
  // sucessfully read bytes
  page[fh.gcount()] = '\0';
  std::cout << "- read_page()\n";
}

int FileWorker::index_page(const char *page, int page_size, char ** prepend_ptr)
{
  std::cout << "+ index_page()\n";
  boost::cregex_token_iterator it,old_it,j;
  boost::regex non_delimiters("[A-Za-z0-9]+");

  char * initial;
  std::string temp = "";
  std::vector<const char *> to_index;
  int initial_len = 0;
  boost::cmatch match; // for finding delimiter match at begining or end
  bool is_first_char_nondelim;

  assert(prepend_ptr != NULL);
  //char * prepend = *prepend_ptr;
  
  if (page != NULL) {
    it = boost::cregex_token_iterator(page, page+strlen(page),non_delimiters,boost::algorithm::token_compress_on);
  }
  
  // If we were passed a string in prepend we have a special case for the
  // first token. Index that token and then proceed normally
  if (*prepend_ptr != NULL) {
    std::cout << "+ handle_prepend\n";
    // build initial token
    initial_len = strlen(*prepend_ptr);
    std::cout << "intial_len = " << initial_len;
    if (it != j) {
      // is the first character not a delimter?
      is_first_char_nondelim = boost::regex_match(page, match, non_delimiters);
      if (is_first_char_nondelim) {
	// yes, so add first token to prepend string
	// can't assign boost::sub_match to std::string, but can append
	temp = std::string() + *it;
	initial_len += temp.length();
	std::cout << " + " << temp.length() << std::endl;
      }
      else {
	// no, but check if it can be added to the index anyway
	temp = std::string() + *it;
	std::cout << "adding first token \"" << temp << "\"\n";
      }
      *it++;
    }    
    initial = new char[initial_len+1];
    std::cout << "allocate initial at " << &initial << std::endl;
    strncpy(initial,*prepend_ptr,strlen(*prepend_ptr));
    initial[strlen(*prepend_ptr)] = '\0';
    std::cout << "the initial buffer now has \"" << initial << "\"\n";
    
    if (is_first_char_nondelim) {
      // check that the first token should be added to prepend
      strcpy(&initial[strlen(*prepend_ptr)],temp.c_str());
    }

    // free prepend 
    delete[] *prepend_ptr;
    std::cout << "free prepend at" << &*prepend_ptr << std::endl;
    *prepend_ptr = NULL;
    
    to_index.push_back(initial);
    std::cout << "adding initial \"" << initial << "\"\n";
    std::cout << "- handle_prepend\n";
    delete [] initial;
    std::cout << "free initial at" << &initial << std::endl;

  }
  
  while (true) {
    // push temp from previous loop
    if (temp.length() > 0) {
      std::cout << "adding \"" << temp << "\"\n";
      to_index.push_back(temp.c_str());
    }
    //save the iterator location pointing to this token
    old_it = it;
    *it++;
    if (it == j) {
      // at this point we want to discard
      break;
    }
    temp = std::string() + *old_it;
  }

  int remaining = 0;
  // If we read a full page, the last token could have been truncated
   if (page_size == READ_PAGE_SIZE) {
    std::cout << "+ handle_remaining\n";
    std::cout << "page[" << READ_PAGE_SIZE << "] = " << page[READ_PAGE_SIZE] << std::endl;
    std::cout << "-----------------------------------------" << std::endl;
    // Find the first delimiter. Any characters between that point and the end have been
    // truncated
    for(int i = READ_PAGE_SIZE - 1; i >= 0; i--) {
      std::cout << "page[" << i << "] = " << page[i] << std::endl;
      if (boost::regex_match(&page[i], match, non_delimiters)) {
	++remaining;
      }
      else
	break;
    }
    std::cout << "- handle_remaining\n";
   }
  
   // If there are trailing characters, we are just going to leave them on temp
   // They will be prepended to the next page
  if (remaining) { 
    std::cout << "there are " << remaining << " characters\n";
  }
  else {
    // no trailing characters, so its safe to push temp (we won't
    // neet to remove it later)
    to_index.push_back(temp.c_str());
  }
  
  // index each of the elements
  auto index_func = boost::bind(&FileWorker::index_token, this, _1);
  std::for_each(to_index.begin(), to_index.end(), index_func);
  std::cout << "- index_page()\n";
  return remaining;
}
  
void FileWorker::process_file(const std::string myFile)
{
  std::ifstream fh (myFile, std::ios::in|std::ios::binary);
  if (!fh.is_open()) {
    std::cerr << "Trouble openening file: " << myFile << std::endl;
  }
  
  std::cout << "+ loop_file()\n";
  // I was very careful in index_page to handle a non-null terminated string in the 
  // case that the entire 4k was used for the read. This allows us to memory and disk
  // operators page aligned.
  char page[READ_PAGE_SIZE] = "";
  char *prepend = NULL;
  int trailing = 0;
  
  while (!fh.eof()) {
    trailing = 0;
    
    read_page(fh,page);
    // index the words in batche, but note the number of
    // characters in a word that was truncated
    trailing = index_page(page,fh.gcount(),&prepend);
    
    // allocate prepend buffer for trailing chars from last page?
    if (trailing) {
      prepend = new char[trailing+1];  // free in index_page()
      std::cout << "allocate prepend at " << &prepend << std::endl;
      // use memcpy for efficiency since this may be a large block
      memcpy(prepend,&page[READ_PAGE_SIZE-trailing],trailing);
      prepend[trailing]='\0';
    }
  }

  // We hit EOF, but check for trailing characters that need to be indexed
  if (trailing && (prepend != NULL)) {
    index_page(NULL,0,&prepend);
  }
  std::cout << "- loop_file()\n";
  fh.close();
}

void FileWorker::run(BoundedQueue<std::string> &fileQueue, boost::exception_ptr & error)
{
  std::string file_to_process;
  
  try {
    std::cout << "Worker (" << _workerId << ") running" << std::endl;
    do {
      file_to_process = fileQueue.receive();
      //process_file(file_to_process);
    } while (file_to_process != "");
    std::cout << "Worker (" << _workerId << ") found that search had terminated\n";
    // put the character of death back on the queue to signal the rest of the workers.
    // not ideal, but in time spent passing around the termination character is less
    // than alternatively checking if string in "" on every receive()
    fileQueue.send("");
  }
  catch (...) {
    error = boost::current_exception();
  }
}
