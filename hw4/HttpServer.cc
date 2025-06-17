/*
 * Copyright ©2025 Hal Perkins.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Spring Quarter 2025 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <sstream>

#include "./FileReader.h"
#include "./HttpConnection.h"
#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpServer.h"
#include "./libhw3/QueryProcessor.h"

using std::cerr;
using std::cout;
using std::endl;
using std::list;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;

namespace hw4 {
///////////////////////////////////////////////////////////////////////////////
// Constants, internal helper functions
///////////////////////////////////////////////////////////////////////////////
// static
const int HttpServer::kNumThreads = 8;

static const char *kThreegleStr =
  "<html><head><title>333gle</title></head>\n"
  "<body>\n"
  "<center style=\"font-size:500%;\">\n"
  "<span style=\"position:relative;bottom:-0.33em;color:orange;\">3</span>"
    "<span style=\"color:red;\">3</span>"
    "<span style=\"color:gold;\">3</span>"
    "<span style=\"color:blue;\">g</span>"
    "<span style=\"color:green;\">l</span>"
    "<span style=\"color:red;\">e</span>\n"
  "</center>\n"
  "<p>\n"
  "<div style=\"height:20px;\"></div>\n"
  "<center>\n"
  "<form action=\"/query\" method=\"get\">\n"
  "<input type=\"text\" size=30 name=\"terms\" />\n"
  "<input type=\"submit\" value=\"Search\" />\n"
  "</form>\n"
  "</center><p>\n";

// This is the function that threads are dispatched into
// in order to process new client connections.
static void HttpServer_ThrFn(ThreadPool::Task *t);

// Given a request, produce a response.
static HttpResponse ProcessRequest(const HttpRequest &req,
                                   const string &base_dir,
                                   const list<string> &indices);

// Process a file request.
static HttpResponse ProcessFileRequest(const string &uri,
                                       const string &base_dir);

// Process a query request.
static HttpResponse ProcessQueryRequest(const string &uri,
                                        const list<string> &indices,
                                        const string &base_dir);

// Returns true if 's' starts with 'prefix'.
static bool StringStartsWith(const string &s, const string &prefix);

// Cleans whitespace surrounding String s in-place
static void CleanString(string *s);

///////////////////////////////////////////////////////////////////////////////
// HttpServer
///////////////////////////////////////////////////////////////////////////////
bool HttpServer::Run(void) {
  // Create the server listening socket.
  int listen_fd;
  cout << "  creating and binding the listening socket..." << endl;
  if (!socket_.BindAndListen(AF_INET6, &listen_fd)) {
    cerr << endl << "Couldn't bind to the listening socket." << endl;
    return false;
  }

  // Spin, accepting connections and dispatching them.  Use a
  // threadpool to dispatch connections into their own thread.
  tp_.reset(new ThreadPool(kNumThreads));
  cout << "  accepting connections..." << endl << endl;
  while (!IsShuttingDown()) {
    // If the HST is successfully added to the threadpool, it'll (eventually)
    // get run and clean itself up.  But we need to manually delete it if
    // it doesn't get added.
    HttpServerTask *hst = new HttpServerTask(HttpServer_ThrFn, this);
    hst->base_dir = static_file_dir_path_;
    hst->indices = &indices_;

    if (!socket_.Accept(&hst->client_fd,
                        &hst->c_addr,
                        &hst->c_port,
                        &hst->c_dns,
                        &hst->s_addr,
                        &hst->s_dns)) {
      // The accept failed for some reason, so quit out of the server.  This
      // can happen when the `kill` command is used to shut down the server
      // instead of the more graceful /quitquitquit handler.
      delete hst;
      break;
    }

    // The accept succeeded; dispatch it to the workers.
    if (!tp_->Dispatch(hst)) {
      delete hst;
      break;
    }
  }
  return true;
}

void HttpServer::BeginShutdown() {
  Verify333(pthread_mutex_lock(&lock_) == 0);
  shutting_down_ = true;
  tp_->BeginShutdown();
  Verify333(pthread_mutex_unlock(&lock_) == 0);
}

bool HttpServer::IsShuttingDown() {
  bool retval;
  Verify333(pthread_mutex_lock(&lock_) == 0);
  retval = shutting_down_;
  Verify333(pthread_mutex_unlock(&lock_) == 0);
  return retval;
}

///////////////////////////////////////////////////////////////////////////////
// Internal helper functions
///////////////////////////////////////////////////////////////////////////////
static void HttpServer_ThrFn(ThreadPool::Task *t) {
  // Cast back our HttpServerTask structure with all of our new client's
  // information in it.  Since we the ones that created this object, we are
  // guaranteed that this is an instance of a HttpServerTask and, per Google's
  // Style Guide, can use a static_cast<> instead of a dynamic_cast<>.
  //
  // Note that, per the ThreadPool::Task API, it is the job of this function
  // to clean up the dynamically-allocated task object.
  unique_ptr<HttpServerTask> hst(static_cast<HttpServerTask*>(t));
  cout << "  client " << hst->c_dns << ":" << hst->c_port << " "
       << "(IP address " << hst->c_addr << ")" << " connected." << endl;

  // Read in the next request, process it, and write the response.

  // Use the HttpConnection class to read and process the next HTTP request
  // from our current client, then write out our response.  Recall that
  // multiple HTTP requests can be sent on the same TCP connection; we
  // need to keep the connection alive until the client sends a
  // "Connection: close\r\n" header; it is only after we finish processing
  // their request that we can shut down the connection and exit
  // this function.

  // STEP 1:

  // Create HttpConnection using client's socket file descriptor
  HttpConnection conn(hst -> client_fd);
  HttpRequest rq;
  HttpResponse response;
  while (!hst->server_->IsShuttingDown()) {

    // Try to read and parse next HTTP request from client
    if (!conn.GetNextRequest(&rq)) {
      cerr << "Error reading request from client " << hst->c_addr << endl;
      close(hst->client_fd);
      break;
    }

    response = ProcessRequest(rq, hst->base_dir, *hst->indices);

    // Write generated HttpResponse back to client socket
    if (!conn.WriteResponse(response)) {
      cerr << "Error writing response to client " << hst->c_addr << endl;
      close(hst->client_fd);
      break;
    }

    // If the client requested the server to shut down, do so.
    if (StringStartsWith(rq.uri(), "/quitquitquit")) {
      hst -> server_->BeginShutdown();
      break;
    }

    // Check connection request header if client wants to close
    // If connection close was sent, break, else wait for more requests
    if (rq.GetHeaderValue("connection") == "close") {
      break;
    }
  }
}

static HttpResponse ProcessRequest(const HttpRequest &req,
                                   const string &base_dir,
                                   const list<string> &indices) {
  // Is the user asking for a static file?
  if (StringStartsWith(req.uri(), "/static/")) {
    return ProcessFileRequest(req.uri(), base_dir);
  }

  // The user must be asking for a query.
  return ProcessQueryRequest(req.uri(), indices, base_dir);
}

static HttpResponse ProcessFileRequest(const string &uri,
                                       const string &base_dir) {
  // The response we'll build up.
  HttpResponse ret;

  // Steps to follow:
  // 1. Use the URLParser class to figure out what file name
  //    the user is asking for. Note that we identify a request
  //    as a file request if the URI starts with '/static/'
  //
  // 2. Use the FileReader class to read the file into memory
  //
  // 3. Copy the file content into the ret.body
  //
  // 4. Depending on the file name suffix, set the response
  //    Content-type header as appropriate, e.g.,:
  //      --> for ".html" or ".htm", set to "text/html"
  //      --> for ".jpeg" or ".jpg", set to "image/jpeg"
  //      --> for ".png", set to "image/png"
  //      etc.
  //    You should support the file types mentioned above,
  //    as well as ".txt", ".js", ".css", ".xml", ".gif",
  //    and any other extensions to get bikeapalooza
  //    to match the solution server.
  //
  // be sure to set the response code, protocol, and message
  // in the HttpResponse as well.
  string file_name = "";

  // STEP 2:

  // Parse URI to extract requested file path
  URLParser parser;
  parser.Parse(uri);
  
  // Append the path portion of the parsed URL to file_name
  file_name += parser.path();

  // Remove "/static/" prefix
  file_name = file_name.replace(0, 8, "");

  // Initialize FileReader to read from base_dir + file_name
  FileReader file_reader(base_dir, file_name);

  string file_content;
  // Attempt to read entire file into file_content
  if (file_reader.ReadFile(&file_content)) {
    // Determine file extension
    size_t dot_pos = file_name.rfind(".");
    string ext = (dot_pos != string::npos) ? file_name.substr(dot_pos) : "";

    // Set Content-Type header based on file extension
    if (ext == ".html" || ext == ".htm") {
      ret.set_content_type("text/html");
    }
    else if (ext == ".jpeg" || ext == ".jpg") {
      ret.set_content_type("image/jpeg");
    }
    else if (ext == ".png") {
      ret.set_content_type("image/png");
    }
    // Default to plain text if no extension found
    else if (ext == ".txt" || ext.empty()) {
      ret.set_content_type("text/plain");
    }
    else if (ext == ".js") {
      ret.set_content_type("text/javascript");
    }
    else if (ext == ".css") {
      ret.set_content_type("text/css");
    }
    else if (ext == ".xml") {
      ret.set_content_type("text/xml");
    }
    else if (ext == ".gif") {
      ret.set_content_type("image/gif");
    }
    else {
      ret.set_content_type("application/octet-stream");
    }
    
    // Set successful HTTP response
    ret.set_protocol("HTTP/1.1");
    ret.set_response_code(200);
    ret.set_message("OK");
    ret.AppendToBody(file_content);
    return ret;
  }


  // If you couldn't find the file, return an HTTP 404 error.
  ret.set_protocol("HTTP/1.1");
  ret.set_response_code(404);
  ret.set_message("Not Found");
  ret.AppendToBody("<html><body>Couldn't find file \""
                   + EscapeHtml(file_name)
                   + "\"</body></html>\n");
  return ret;
}

static HttpResponse ProcessQueryRequest(const string &uri,
                                        const list<string> &indices,
                                        const string &base_dir) {
  // The response we're building up.
  HttpResponse ret;

  // Your job here is to figure out how to present the user with
  // the same query interface as our solution_binaries/http333d server.
  // A couple of notes:
  //
  // 1. The 333gle logo and search box/button should be present on the site.
  //
  // 2. If the user had previously typed in a search query, you also need
  //    to display the search results.
  //
  // 3. you'll want to use the URLParser to parse the uri and extract
  //    search terms from a typed-in search query.  convert them
  //    to lower case.
  //
  // 4. Initialize and use hw3::QueryProcessor to process queries with the
  //    search indices.
  //
  // 5. With your results, try figuring out how to hyperlink results to file
  //    contents, like in solution_binaries/http333d. (Hint: Look into HTML
  //    tags!)

  // STEP 3:

  std::ostringstream response_stream;
  response_stream << kThreegleStr;

  // Parse the URI to get search terms
  URLParser parser;
  parser.Parse(uri);
  std::string query = parser.args()["terms"];
  CleanString(&query);
  boost::to_lower(query);

  // If URI indicates search request process query and display results
  if (uri.find("query?terms=") != std::string::npos) {
    // Split query string on spaces
    std::vector<string> qvec;
    boost::split(qvec, query, boost::is_any_of(" "), boost::token_compress_on);

    hw3::QueryProcessor qp(indices, false);

    // Search and retrieve vector of QueryResult objects
    std::vector<hw3::QueryProcessor::QueryResult> qr = qp.ProcessQuery(qvec);

    // No matches
    if (qr.empty()) {
      response_stream << "<p><br>\r\n" << "No results found for <b>" << EscapeHtml(query) << "</b>\r\n" << "<p>\r\n\r\n";
    }
    
    else {
      // Display count of matching documents
      response_stream << "<p><br>\r\n";
      response_stream << qr.size();
      if (qr.size() == 1) {
        response_stream << " result ";
      } else {
        response_stream << " results ";
      }
      response_stream << "found for <b>" << EscapeHtml(query) << "</b>\r\n"
                      << "<p>\r\n\r\n"
                      << "<ul>\r\n";

      // Generate hyperlinks to document and show its rank
      for (const auto &result : qr) {
        response_stream << " <li> <a href=\"";
        if (result.document_name.substr(0, 7) != "http://")
          response_stream << "/static/" << result.document_name << "\">" << EscapeHtml(result.document_name) << "</a> [" << result.rank << "]<br>\r\n";
      }
      response_stream << "</ul>\r\n";
    }
  }

  // Close HTML body and document
  response_stream << "</body>\r\n</html>\r\n";

  // Set successful HttpResponse fields
  ret.set_protocol("HTTP/1.1");
  ret.set_response_code(200);
  ret.set_message("OK");
  ret.set_content_type("text/html");
  ret.AppendToBody(response_stream.str());

  return ret;
}

static bool StringStartsWith(const string &s, const string &prefix) {
  return s.substr(0, prefix.size()) == prefix;
}

// Cleans whitespace surrounding String s in-place
static void CleanString(string *s) {
  // If string is already empty, nothing to do
  if (s -> empty()) {
    return;
  }

  // Find first non-whitespace character from left
  size_t left = 0;
  while (left < s -> size() && isspace(static_cast<unsigned char>((*s)[left]))) {
    left++;
  }

  // If advanced past end, string was all whitespace
  // Clear and return
  if (left == s -> size()) {
    s -> clear();
    return;
  }

  // Find last non-whitespace character from right
  size_t right = s -> size() - 1;
  while (right > left && isspace(static_cast<unsigned char>((*s)[right]))) {
    right--;
  }

  // Erase trailing whitespace
  s -> erase(right + 1);

  // Erase leading whitespace
  s -> erase(0, left);
}

}  // namespace hw4
