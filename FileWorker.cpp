#include "FileWorker.h"

void FileWorker::index_token(const char * token) {
  if (Index.find(token) != Index.end())
    // if token was found in map
    Index[token] += 1;
  else
    // else add it with count = 1
    Index[token] = 1;
}

/* FileWorker::read_page()
 *   Arguments: file handle with seek pointer approprately placed and the 
 *              number of bytes to read. This would typicall be READ_PAGE_SIZE.
 *   Return: None
 *   Effects: _page and _page_size are updated with the date read from fh
 *            the seek pointer of fh has been moved n_bytes or is at eof
 *            with _page_size set to the number of bytes successfully read
 */
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

/* FileWorker::index_page()
 *   Arguments: None
 *   Return: the number of bytes at the end of _page that were possibly
 *           truncated at the page boundary. These are still in _page, so
 *           they are the responsibility of the caller. They should be fed back
 *           into this function.
 */
int FileWorker::index_page()
{

  if (_debug > 2)
    std::cout << "+ index_page()\n";
  boost::cregex_token_iterator it,old_it,j;
  boost::regex non_delimiters("[A-Za-z0-9]+");

  int remaining = 0;
  int prepend_bytes = strlen(prepend.get());
  std::cout << "prepend is: " << prepend.get() << std::endl;
  std::string temp = "";
  std::vector<const char *> to_index;
  boost::cmatch match; // for finding delimiter match at begining or end
  bool is_first_char_nondelim;

  // start off with initial being the length of the prepend byffer
  int initial_len = prepend_bytes;

  if (_page != NULL) {
    it = boost::cregex_token_iterator(_page, _page+_page_size,non_delimiters,boost::algorithm::token_compress_on);
  }
  
  /* If we were passed a string in prepend we have a special case for the
   *   first token. Index that token and then proceed normally */
  if (prepend_bytes > 0) {
    if (_debug > 2)
      std::cout << "+ handle_prepend\n";
    
    // build initial token
    if (_debug > 2)
      std::cout << "intial_len = " << initial_len;
    if (it != j) {
      // Q: Is the first character not a delimter?
      is_first_char_nondelim = boost::regex_match(_page, match, non_delimiters);

      if (is_first_char_nondelim) {
	// A: Yes, so add first token to prepend string
	temp = std::string() + *it;  // workaround for converting boost::sub_match to std::string
	initial_len += temp.length();
	if (_debug > 2)
	  std::cout << " + " << temp.length() << std::endl;
      }
      else {
	/* A: No, but the first token of the page will get added anyway
	 *    Save the token to temp so we can increment the iterator */
	temp = std::string() + *it;
	if (_debug > 2)
	  std::cout << "adding first token \"" << temp << "\"\n";
      }
      *it++;
    }
    initial.reset( new char[initial_len+1] );
    memcpy(initial.get(),prepend.get(),prepend_bytes);
    initial[prepend_bytes] = '\0';
    if (_debug > 2) {
      std::cout << "adding prepend to inital\n";
    }

    if (is_first_char_nondelim) {
      // check if the first token should be added to prepend
      // use strcpy because temp is null terminated
      strcpy(&initial[prepend_bytes],temp.c_str());
      // clear temp, so it's safe to reuse
      temp.clear();
    }

    to_index.push_back(initial.get());
    if (_debug > 2) {
      std::cout << "adding initial \"" << initial.get() << "\"\n";
      std::cout << "- handle_prepend\n";
    }
  }
  
  while (true) {
    // push temp from previous loop
    if (temp.length() > 0) {
      if (_debug > 2)
	std::cout << "adding \"" << temp << "\"\n";
      to_index.push_back(temp.c_str());
    }
    // save the iterator location pointing to this token
    old_it = it;
    *it++;
    if (it == j) {
      /* This is the end of the page and we might have a truncated token.
       *   So leave temp (with the contents of the last token) intact */
      break;
    }
    // save in temp for the next time around
    temp = std::string() + *old_it;
  }

  // If we read a full page, the last token could have been truncated
  if (_page_size == READ_PAGE_SIZE) {
    if (_debug > 2) {
      std::cout << "+ handle_remaining\n";
      std::cout << "-----------------------------------------" << std::endl;
    }
    // Working from the last byte towards the first, find the first delimiter
    for(int i = READ_PAGE_SIZE - 1; i >= 0; i--) {
      if (_debug > 2)
	std::cout << "page[" << i << "] = " << _page[i] << std::endl;
      if (boost::regex_match(&_page[i], match, non_delimiters)) {
	++remaining;
      }
      else {
	if (_debug > 2)
	  std::cout << "page[" << i << "] = " << _page[i] << std::endl;
	break;
      }
    }
    if (_debug > 2)
      std::cout << "- handle_remaining\n";
  }
  
  if (!remaining) {
    // no trailing characters, so its safe to push temp (we won't
    // need to remove it later)
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
  if (_debug > 2)
    std::cout << "- index_page()\n";
  return remaining;
}

/* FileWorker::process_file()
 *  Arguments: file name passed as an r-value reference
 *  Return: None
 *  Side effects: members _page and _page_size are used as temorary buffers
 *                a prepend and initial buffer are dynamically allocated for each page
 */  
void FileWorker::process_file(const std::string &&myFile)
{
  std::ifstream fh (myFile, std::ios::in);
  if (!fh.is_open()) {
    std::cerr << "Could not openen file: " << myFile << std::endl;
    return;
  }
  
  if (_debug > 2)
    std::cout << "+ loop_file()\n";
  // I was very careful in index_page to handle a non-null terminated string in the 
  // case that the entire 4k was used for the read. This allows us to memory and disk
  // operators page aligned.
  
  int trailing_bytes = 0;

  while (!fh.eof()) {
    // read up to READ_PAGE_SIZE bytes 
    read_page(fh,READ_PAGE_SIZE);

    // index the words in batches, but note the number of
    // characters in a word that were truncated
    trailing_bytes = index_page();
    
    // allocate prepend buffer for trailing chars from last page?
    prepend.reset( new char[trailing_bytes+1] );
    prepend[trailing_bytes] = '\0';
    if (trailing_bytes > 0) {
      // use memcpy for efficiency since this may be a large block
      memcpy(prepend.get(),&_page[READ_PAGE_SIZE-trailing_bytes],trailing_bytes);
    }

    // the current page is old data, reset page_size to 0
    _page_size = 0;
  }

  // We hit EOF, but check for trailing characters that need to be indexed
  if (trailing_bytes > 0) {
    index_page();
  }
  if (_debug > 2)
    std::cout << "- loop_file()\n";
  fh.close();
}

/* FileWorker::run()
 *  Arguments: the shared work queue containing file to process and an exception pointer
 *  Return: None
 *  Side effects: Items will be pulled from the fileQueue work queue and processes
 *                Synchronization of queue accesses is managed by the BoundedQueue class
 *                Exception ptr updated with any exceptions that occur in this thread
 */
void FileWorker::run(boost::shared_ptr<BoundedQueue<std::string>> fileQueue, boost::exception_ptr & error)
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
	process_file(std::move(file_to_process));
      else {
	_state = SHUTDOWN;
      }
    } while (_state != SHUTDOWN);

    // put the termination character on the queue to signal the rest of the workers.
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
