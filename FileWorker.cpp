#include "FileWorker.h"

void FileWorker::index_token(const char * token) {
  if (Index.find(token) != Index.end())
    // if token was found in map
    Index[token] += 1;
  else
    // else add it with count = 1
    Index[token] = 1;
}

void FileWorker::read_page(std::ifstream &fh, int n_bytes)
{
  if (_debug > 2)
    std::cout << "+ read_page()\n";
  fh.read(_page, n_bytes);
  // That could fail (eof), so return number of bytes and let caller handle adding null terminator
  if (_debug > 2)
    std::cout << "- read_page()\n";
  _page_size = fh.gcount();
}

int FileWorker::index_page()
{
  if (_debug > 2)
    std::cout << "+ index_page()\n";
  boost::cregex_token_iterator it,old_it,j;
  boost::regex non_delimiters("[A-Za-z0-9]+");

  char * initial;
  std::string temp = "";
  int prepend_bytes = strlen(*prepend);
  if ((prepend_bytes > 0) && (_debug > 2)) {
    std::cout << "prepend is: " << *prepend << std::endl;
  }
  int initial_len = prepend_bytes;
  std::vector<const char *> to_index;
  boost::cmatch match; // for finding delimiter match at begining or end
  bool is_first_char_nondelim;

  if (_page != NULL) {
    it = boost::cregex_token_iterator(_page, _page+_page_size,non_delimiters,boost::algorithm::token_compress_on);
  }
  // If we were passed a string in prepend we have a special case for the
  // first token. Index that token and then proceed normally
  if (prepend_bytes > 0) {
    if (_debug > 2)
      std::cout << "+ handle_prepend\n";    
    // build initial token
    if (_debug > 2)
      std::cout << "intial_len = " << initial_len;
    if (it != j) {
      // is the first character not a delimter?
      is_first_char_nondelim = boost::regex_match(_page, match, non_delimiters);
      if (is_first_char_nondelim) {
	// yes, so add first token to prepend string
	temp = std::string() + *it;   // workaround for converting boost::sub_match to std::string
	initial_len += temp.length();
	if (_debug > 2)
	  std::cout << " + " << temp.length() << std::endl;
      }
      else {
	// no, but check if it can be added to the index anyway
	temp = std::string() + *it;
	if (_debug > 2)
	  std::cout << "adding first token \"" << temp << "\"\n";
      }
      *it++;
    }    
    initial = new char[initial_len+1];
    if (_debug > 2)
      std::cout << "allocate initial at " << &initial << std::endl;
    strncpy(initial,*prepend,strlen(*prepend));
    initial[strlen(*prepend)] = '\0';
    if (_debug > 2)
      std::cout << "Copied prepend to initial buffer.\n";
    
    if (is_first_char_nondelim) {
      // check that the first token should be added to prepend
      if (_debug > 2)
	std::cout << "Copied temp with first token to initial buffer.\n";
      strcpy(&initial[prepend_bytes],temp.c_str());
      temp.clear();
    }

    to_index.push_back(initial);
    if (_debug > 2) {
      std::cout << "adding initial \"" << initial << "\"\n";
      std::cout << "- handle_prepend\n";
    }
    delete [] initial;
    if (_debug > 2)
      std::cout << "free initial at" << &initial << std::endl;
  }

  while (true) {
    // push temp from previous loop
    if (temp.length() > 0) {
      if (_debug > 2)
	std::cout << "adding \"" << temp << "\"\n";
      to_index.push_back(temp.c_str());
    }
    //save the iterator location pointing to this token
    //    old_it = it;
    *it++;
    if (it == j) {
      // at this point we want to discard
      break;
    }
    //    temp = std::string() + *old_it;
  }
  
  int remaining = 0;
  // If we read a full page, the last token could have been truncated
  if (_page_size == READ_PAGE_SIZE) {
    if (_debug > 2) {
      std::cout << "+ handle_remaining\n";
      std::cout << "-----------------------------------------" << std::endl;
    }
    // Find the first delimiter. Any characters between that point and the end have been
    // truncated
    for(int i = READ_PAGE_SIZE - 1; i >= 0; i--) {
      if (_debug > 2)
	std::cout << "page[" << i << "] = " << _page[i] << std::endl;
      if (boost::regex_match(&_page[i], match, non_delimiters)) {
	++remaining;
      }
      else
	break;
    }
    if (_debug > 2)
      std::cout << "- handle_remaining\n";
   }


   if (!remaining) {
     // no trailing characters, so its safe to push temp (we won't
     // need to remove it later)
     if (_debug > 2)
       std::cout << "adding from temp: " << temp << std::endl;
     to_index.push_back(temp.c_str());
   }
   else {
     // If there are trailing characters, we are just going to leave them on temp
     // They will be prepended to the next page
     if (_debug > 2)
       std::cout << "there are " << remaining << " characters\n";
   }
   
   
   // index each of the elements
   //auto index_func = boost::bind(&FileWorker::index_token, this, _1);
   //std::for_each(to_index.begin(), to_index.end(), index_func);
   std::cout << "- index_page()\n";
   return remaining;
}
  
void FileWorker::process_file(const std::string myFile)
{
  std::ifstream fh (myFile, std::ios::in);
  if (!fh.is_open()) {
    std::cerr << "Could not openen file: " << myFile << std::endl;
    return;
  }
  if (_debug > 2)
    std::cout << "+ loop_file()\n";
  char * temp;  // unlike page this is null terminated
  int trailing = 0;
  int new_trailing_len = 0;

  while (!fh.eof()) {
    // read up to READ_PAGE_SIZE bytes 
    read_page(fh,READ_PAGE_SIZE);
    
    // index the words in batches, but note the number of
    // characters in a word that were truncated
    trailing = index_page();
 
    // allocate prepend buffer for trailing chars from last page?
    if (trailing) {
      new_trailing_len = trailing;
      if (trailing == READ_PAGE_SIZE)
	new_trailing_len += strlen(*prepend);
      
      if ((prepend != NULL) && (strlen(*prepend) > 0)) {
	if ((unsigned)new_trailing_len > strlen(*prepend)) {
	  std::cout << "growing trailing buffer to " << new_trailing_len << " bytes. old trailing was " << strlen(*prepend) << std::endl;
	  temp = new char[trailing+1];
	  strncpy(temp,*prepend,strlen(*prepend));
	  temp[trailing] = '\0';
	  std::cout << "temp buffer: " << *temp << std::endl;
	  delete [] prepend;
	  *prepend = new char[new_trailing_len+1];
	  memcpy(*prepend,temp,trailing);
	  delete [] temp;
	  memcpy(&(prepend)[strlen(*prepend)],&_page[READ_PAGE_SIZE-trailing],trailing);
	  (*prepend)[new_trailing_len]='\0';
	  std::cout << "new buffer: " << *prepend << std::endl;
	}
	else {
	  std::cout << "copied into existing trailing buffer. now " << new_trailing_len << " bytes.\n";
	  memcpy(*prepend,&_page[READ_PAGE_SIZE-trailing],trailing);
	  (*prepend)[new_trailing_len]='\0';
	}
      }
      else {
	  std::cout << "created new trailing buffer of " << new_trailing_len << " bytes.\n";
	  *prepend = new char[new_trailing_len+1];
	  memcpy(*prepend,&_page[READ_PAGE_SIZE-trailing],trailing);
	  (*prepend)[new_trailing_len]='\0';
	  std::cout << "new buffer: " << *prepend << std::endl;
      }
    }
    else {
      if (prepend != NULL) {
	// mark prepend as unused
	(*prepend)[0] = '\0';
      }
    }
  }
  
  // We hit EOF, but check for trailing characters that need to be indexed
  if (trailing && (prepend != NULL)) {
    index_page();
  }
  std::cout << "- loop_file()\n";
  fh.close();
}

void FileWorker::run(boost::shared_ptr<BoundedQueue<std::string>> fileQueue, boost::shared_ptr<WordIndex> memory_table, boost::exception_ptr & error)
{
  std::string file_to_process;
  try {
    do {
      _state = WAITING;
      file_to_process = fileQueue->receive();
      _state = WORKING;
      if (_debug > 2)
	std::cout << "worker has file: " << file_to_process << std::endl;
      if (file_to_process != "")
	process_file(file_to_process);
    } while (file_to_process != "");
    _state = SHUTDOWN;
    // put the character of death back on the queue to signal the rest of the workers.
    // not ideal, but in time spent passing around the termination character is less
    // than alternatively checking if string in "" on every receive()
    fileQueue->send("");
  }  
  catch (const boost::exception& e) {
    std::cout << "FileWorker encountered unexpected exception" << diagnostic_information(e) << std::endl;
    error = boost::current_exception();
  }
  catch(std::exception const& e) {
    std::cout << e.what() << std::endl;
    error = boost::current_exception();
  }
  catch (...) {
    error = boost::current_exception();
  }
}

