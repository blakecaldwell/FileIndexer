#ifndef FILEWORKER_H
#define FILEWORKER_H

#include <boost/thread.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string/constants.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/smart_ptr/scoped_ptr.hpp>
#include <fstream>
#include <cstring>
#include <iostream>
#include <deque>

#define READ_PAGE_SIZE 4096
// This was chosen to align with a large disk I/O operation and to fill an entire page in the buffer cache
// Anything larger is likely will bring more overhead in terms of additional IOs, buffer cache management.

// Ideally make the read_page operation asynchronous. Give control back to worker thread to do useful work
// in the meantime while waiting for IO to complete

template<class T>
class BoundedQueue
{

  int _capacity;
  int _size;
  std::deque<T> _queue;
  boost::condition_variable _cv_not_full;
  boost::condition_variable _cv_not_empty;
  boost::mutex _mutex;
  
  bool isNotEmpty()
  {
    return !_queue.empty();
  }

  bool isNotFull()
  {
    return _size != _capacity;
  }

 public:
  BoundedQueue(int capacity)  { _capacity = capacity; }
 
  void send(T && msg)
  {
    {
      boost::unique_lock<boost::mutex> lg(_mutex); // enter crit section
      
      _cv_not_full.wait(lg, boost::bind(&BoundedQueue::isNotFull, this));  // block if full

      _queue.push_front(std::move(msg));
      ++_size;
    }  // leave crit section
    _cv_not_empty.notify_one();
  }
  T receive()
  {
    boost::unique_lock<boost::mutex> lg(_mutex);
    _cv_not_empty.wait(lg, boost::bind(&BoundedQueue::isNotEmpty, this));  // block if empty
    T msg = std::move(_queue.back());
    _queue.pop_back();
    --_size;
    _cv_not_full.notify_one();
    
    return msg;
  }
};

class FileWorker : boost::noncopyable
{
  int _workerId;
  int _debug;
  enum state { STARTED, WAITING, WORKING, SHUTDOWN };
  state _state;
  std::map<std::string,int> Index;
  char _page[READ_PAGE_SIZE];
  int _page_size;
  std::string initial;
  boost::scoped_ptr<char *> prepend;  
  void index_token(const char * token);
  void read_page(std::ifstream &fh, int n_bytes);
  int index_page();
  void process_file(const std::string &&fh);  

public:
  FileWorker(int Id, int debug=0) { 
    _workerId = Id; 
    _debug = debug;
  }
  void run(boost::shared_ptr<BoundedQueue<std::string>> fileQueue, boost::exception_ptr & error);
};

#endif

