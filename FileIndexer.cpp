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

void searchPath(const CmdLineOptions &myOptions, BoundedQueue<std::string> &&fileQueue, boost::exception_ptr & error)
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

  try {
    for ( fs::recursive_directory_iterator end, dir(myOptions.root_search_path); 
	  dir != end; ++dir ) {
      // dir contains one entry. it could be a file, path
      
      fs::file_status s = fs::status(dir->path());
      if (fs::is_symlink(dir->path())) {
	if (myOptions.debug > 2)
	  std::cout << "Found symlink: " << dir->path().string() << std::endl;
	//		  << "to: " << fs::read_symlink(dir->path()).string() << std::endl;
	//fs::path parent = fs::read_symlink(dir->path());
	fs::path parent = dir->path();
	while (!parent.empty()) {
	  if (parent == myOptions.root_search_path) {
	    // skip because target is rooted in root_search_path
	    if (myOptions.debug > 2)
	      std::cout << "Skipping symlink: " << dir->path().string() << std::endl;
	    ++dir;
	    break;
	  }
	  parent = parent.parent_path();
	}
      }
      if ((fs::is_regular_file(s)) && (dir->path().extension() == ".txt")) {
	if (myOptions.debug > 1)
	  std::cout << "searchWorker: " << dir->path().string() << " placed on queue\n";
	std::string _temp = dir->path().string();
	fileQueue.send(boost::move(_temp));
      }
    }
    
    // finished with indexing root path now
    // place character of death on queue
    fileQueue.send(boost::move(""));
  }
  catch (...) {
    error = boost::current_exception();
  }
  
  if (myOptions.debug > 1)
    std::cout << "Finished search of path: " << myOptions.root_search_path << std::endl;
}

int main(int argc, char * argv[])
{
  try { // main try block
    CmdLineOptions user_options;

    get_CmdLineOptions(argc,argv,user_options);

    BoundedQueue<std::string> FilesToIndex(MAX_QUEUE_FILES);
    /* Launch search thread */
    boost::exception_ptr search_error;
    boost::thread searchWorker(searchPath, user_options, boost::ref(FilesToIndex), boost::ref(search_error));
    //if( search_error )
    //  boost::rethrow_exception(search_error);
    
    // instantiate workers
    std::vector<FileWorker> workers;
    for (int i = 0; i < user_options.N; ++i) {
      workers.push_back(FileWorker(i));
    }

    /* Launch worker threads */
    std::vector<boost::thread> threads;
    std::vector<boost::exception_ptr> index_errors;
    for (int i = 0; i < user_options.N; ++i) {
      index_errors.push_back(boost::exception_ptr());
      threads.push_back(boost::thread(&FileWorker::run, workers[i], boost::ref(FilesToIndex), boost::ref(index_errors[i])));
    }
    //if( index_error )
    //  boost::rethrow_exception(index_error);
    
    searchWorker.join();
    for (int i = 0; i < user_options.N; ++i) {
      threads[i].join();
    }

    if (user_options.debug > 1)
      std::cout << "workers have all joined" << std::endl;
  }
  catch(std::exception const& e) {
    std::cout << std::endl
	      << e.what() << std::endl;
    return EXIT_ERROR;
  }

  return EXIT_SUCCESS;
}  
