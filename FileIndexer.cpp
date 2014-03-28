/*
Super Simple File Indexer
Blake Caldwell <blake.caldwell@colorado.edu>
2014-03-22
*/

#include "FileIndexer.h"
#include "FileWorker.h"
#include "WordIndex.h"

int is_pathValid(const fs::path & root_search_path) {
  try {
    fs::file_status s = fs::status(root_search_path);
    
    if ( ! fs::is_directory(s)) {
      std::cout << "Path given does not exist or is not a directory: \""
		<< root_search_path.string() << "\"" << std::endl
		<< std::endl;
      return 0;
    }
  }
  catch (std::exception &e) {
    std::cout << std::endl 
	      << e.what() << std::endl;
    throw FileIndexer_exception;
  }

  return 1;
}

void get_CmdLineOptions(int argc, char * argv[], CmdLineOptions && myOptions)
{
  po::options_description description("Usage: FileIndexer [options] PATH\naccepted options");
  description.add_options()
    ("help,h", "Display this help message")
    ("dbg,d", po::value<int>()->default_value(1),"Debug level: 1 default, 3 is full debugging")
    ("N,n", po::value<int>(),"Number of worker threads available to read text fles")
    ("path", po::value<std::string>(), "Root path for the directory traversal");
  
  // The root search path is a positional argument that doesn't require an option name
  po::positional_options_description p;

  // If multiple paths are specified on the command line (e.g. with *) 
  // only the first will be searched
  p.add("path", 1);
  
  // parse the command line arguments and handle any exceptions
  po::variables_map vm;
  try {
      po::store(po::command_line_parser(argc, argv).options(description).positional(p).run(), vm);
      po::notify(vm);
  }
  catch (std::exception const &e) {
    std::cout << e.what() << std::endl
	      << description << std::endl;
    throw;
  }

  if(vm.count("help")) {
    std::cout << std::endl 
	      << description << std::endl;
    throw FileIndexer_exception;
  }

  if ( ! vm.count("path")) {
    // use this instead of po::value<>()->required() so that help option is still
    // has expected output even with no arguments
    std::cout << std::endl 
	      << "A PATH argument is required but missing" << std::endl
	      << std::endl
	      << description << std::endl;
    throw FileIndexer_exception;
  }
  else {
    myOptions.root_search_path = vm["path"].as<std::string>();
  }
  
  if (vm.count("dbg")) {
    int user_debug = vm["dbg"].as<int>();
    
    if ( (user_debug < 1) || 
	 (user_debug > 3) ) {
      std::cout << "Debug level must be between 1 and 3" << std::endl
		<< description << std::endl;
      throw FileIndexer_exception;
    }

    myOptions.debug = user_debug;
  }
 
  if (vm.count("N")) {
    unsigned this_hardware_concurrency =  boost::thread::hardware_concurrency();
    int user_num_workers = (vm["N"].as<int>());

    if (user_num_workers < 1) {
      std::cout << "Number of workers must be 1 or more" << std::endl
                << "Hardware concurrency is: " << this_hardware_concurrency << std::endl
		<< description << std::endl;
      throw FileIndexer_exception;
    }
    
    myOptions.N = user_num_workers;
  }
  else {
    myOptions.N = DEFAULT_NUM_WORKERS;
  }

  /* root_search_path validation */
  if (is_pathValid(myOptions.root_search_path)) {
    std::cout << "Indexing files in root path: " << myOptions.root_search_path.string() << std::endl;
  }
  else {
    // can't do anything without a valid exception, so terminate
    throw FileIndexer_exception;
  }  
}

void searchPath(const CmdLineOptions &myOptions, boost::shared_ptr<BoundedQueue<std::string>> fileQueue, boost::exception_ptr & error)
{
  /* DANGERS
     1. What about loops from links
     2. How are hardlinks handled
     3. Log when permissions denied is encountered and other exceptions
  */

  // There is a bug with finding symlinks. Use boost version with read_symlink
  if (myOptions.debug > 1)
    std::cout << "Started search of path: " << myOptions.root_search_path << std::endl;
  
  // the thread we are placing work on their queue

  fs::path p,sym_path,abs_path,cur_canonical;
  try {
    // convert to absolute path
    abs_path = fs::canonical(myOptions.root_search_path);
    for ( fs::recursive_directory_iterator end, it(abs_path); 
	  it != end; ++it ) {
      p = it->path();
      if ((fs::is_regular_file(p)) && (p.extension() == ".txt")) {
	if (myOptions.debug > 1)
	  std::cout << "searchWorker: " << p.string() << " placed on queue\n";
	fileQueue->send(p.string());
      }
      else if (fs::is_symlink(p)) {
	if (myOptions.debug > 2) {
	  std::cout << "Found symlink: " << p.string() << std::endl;
	}
	sym_path = fs::read_symlink(p); // this was introduced with boost 1.44
	cur_canonical = fs::canonical(sym_path);
	if (abs_path < cur_canonical.remove_filename()) { //modifies cur_canonical
	  // the symlink points to within the search path, so skip
	  if (myOptions.debug > 2)
	    std::cout << "Skipping " << p.string()
		      << " because it is a symlink to the path: " << cur_canonical.string() << std::endl;
	  it.no_push();
	}
      }
    }
    
    // finished with indexing root path now
    // place character of death on queue
    fileQueue->send("");
  }
  catch (...) {
    error = boost::current_exception();
  }
  
  if (myOptions.debug > 1)
    std::cout << "Finished search of path: " << myOptions.root_search_path << std::endl;
}

void cleanupWorkers (std::vector<boost::scoped_ptr<boost::thread>> &&workers,  std::vector<boost::exception_ptr> &&index_errors)
{
  std::for_each(workers.begin(), workers.end(), [](boost::scoped_ptr<boost::thread> t) {t->join();});
  std::for_each(index_errors.begin(), index_errors.end(), [](boost::exception_ptr & e) {boost::rethrow_exception(e);});
}


int main(int argc, char * argv[])
{
  std::vector<boost::scoped_ptr<boost::thread>> workers;
  std::vector<boost::exception_ptr> index_errors;
  CmdLineOptions user_options;
  boost::shared_ptr<BoundedQueue<std::string>> FilesToIndex( new BoundedQueue<std::string>(MAX_QUEUE_FILES) );

  try { /* get exceptions from threads launched or FileIndexer_exception */
    
    get_CmdLineOptions(argc,argv,std::move(user_options));

    /* Launch worker threads */
    for (int i = 0; i < user_options.N; ++i) {
      boost::exception_ptr index_error;
      boost::thread* t_ptr;
      try {
	FileWorker _w(i);
	boost::scoped_ptr<boost::thread> t( new boost::thread(&FileWorker::run, &_w, FilesToIndex, boost::ref(index_error)));
	workers.push_back(t);
	index_errors.push_back(index_error);
      }
      catch (boost::thread_resource_error const& e) {
	std::cout << "Couldn't launch as many threads as requested: " << e.what() << std::endl;
	std::cout << "Continuing with " << i << " threads\n";  // i starts at 0 and threads[i] is not running
	break;
      }
      catch (std::exception const& e) {
	std::cout << "Thread " << i << " thew exception: " << e.what() << std::endl;
	
	// need to tear down because we dont know what caused this
	cleanupWorkers(boost::ref(workers),boost::ref(index_errors));
	return EXIT_ERROR;
      }	
    }
    
    /* Launch search thread */
    boost::exception_ptr search_error;
    boost::thread searchWorker(searchPath, user_options, FilesToIndex, boost::ref(search_error));
    if( search_error )
      boost::rethrow_exception(search_error);
    
    /* Wait for workers to drain FilesToIndex */
    searchWorker.join();
    if (user_options.debug > 1)
      std::cout << "workers have all joined" << std::endl;
    
    /* print the resulting main index */
    
    /* all done */
    cleanupWorkers(boost::move(workers),boost::move(index_errors));
    
  } /* --- main try block --- */
  
  catch(std::exception const& e) {
    std::cout << std::endl
	      << e.what() << std::endl;
    cleanupWorkers(boost::move(workers),boost::move(index_errors));
    return EXIT_ERROR;
  } /* --- main catch block -- */ 

  return EXIT_SUCCESS;
} /* --- int main() --- */
