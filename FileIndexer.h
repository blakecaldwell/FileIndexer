#ifndef FILEINDEXER_H
#define FILEINDEXER_H

/* Main header file for FileIndexer */

#include <iostream>
#include <exception> /* for defining FileIndexer exception */

// Boost library includes
#include <boost/thread.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/exception/all.hpp>

#include "FileWorker.h" // For BoundedQueue

#define DEFAULT_NUM_WORKERS 3
// Impose limit on queue growth. This will also limit how far ahead a single worker thread can get.
// The searchPath thread will block until the queue drops back below this threshold
#define MAX_QUEUE_FILES 1000
#define EXIT_ERROR 2

namespace po = boost::program_options;
namespace fs = boost::filesystem;

/* Basic exception to terminate application */
class base_exception: public std::exception
{
  virtual const char* what() const throw()
  {
    return "Terminating...";
  }
} FileIndexer_exception;

struct recursive_directory_iterator_error: virtual boost::exception { };

struct CmdLineOptions { // Pass command line options around as a struct
  int debug;
  int N;
  fs::path root_search_path;
};

/* Method protoypes */
int is_pathValid(const fs::path & root_search_path);
void get_CmdLineOptions(int argc, char * argv[], CmdLineOptions && myOptions);
void searchPath(const CmdLineOptions & myOptions, boost::shared_ptr<BoundedQueue<std::string>> fileQueue, boost::exception_ptr & error);
void cleanupWorkers (boost::thread_group &&workers,  std::vector<boost::exception_ptr> &&index_errors);


#endif
