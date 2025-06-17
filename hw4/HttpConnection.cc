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

#include <stdint.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <map>
#include <string>
#include <vector>

#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpConnection.h"

using std::map;
using std::string;
using std::vector;

namespace hw4 {

static const char *kHeaderEnd = "\r\n\r\n";
static const int kHeaderEndLen = 4;

bool HttpConnection::GetNextRequest(HttpRequest *const request) {
  // Use WrappedRead from HttpUtils.cc to read bytes from the files into
  // private buffer_ variable. Keep reading until:
  // 1. The connection drops
  // 2. You see a "\r\n\r\n" indicating the end of the request header.
  //
  // Hint: Try and read in a large amount of bytes each time you call
  // WrappedRead.
  //
  // After reading complete request header, use ParseRequest() to parse into
  // an HttpRequest and save to the output parameter request.
  //
  // Important note: Clients may send back-to-back requests on the same socket.
  // This means WrappedRead may also end up reading more than one request.
  // Make sure to save anything you read after "\r\n\r\n" in buffer_ for the
  // next time the caller invokes GetNextRequest()!

  // STEP 1:


  char read_buf[4096];
  ssize_t bytes_read;

  // Keep looking until finding kHeaderEnd in buffer_
  size_t header_pos = buffer_.find(kHeaderEnd);
  while (header_pos == string::npos) {
    bytes_read = WrappedRead(fd_, (unsigned char*)read_buf, sizeof(read_buf));
    if (bytes_read == 0) {
      // If connection closed before full header
      return false;
    } else if (bytes_read == -1) {
      return false;
    }

    // Append read content and check again for kHeaderEnd
    buffer_.append(read_buf, bytes_read);
    header_pos = buffer_.find(kHeaderEnd);
  }

  // Full header in buffer_
  size_t header_end = header_pos + kHeaderEndLen;
  string request_str = buffer_.substr(0, header_end);
  buffer_ = buffer_.substr(header_end);

  *request = ParseRequest(request_str);
  return true;
}

bool HttpConnection::WriteResponse(const HttpResponse &response) const {
  // We use a reinterpret_cast<> to cast between unrelated pointer types, and
  // a static_cast<> to perform a conversion from an unsigned type to its
  // corresponding signed one.
  string str = response.GenerateResponseString();
  int res = WrappedWrite(fd_,
                         reinterpret_cast<const unsigned char*>(str.c_str()),
                         str.length());

  if (res != static_cast<int>(str.length()))
    return false;
  return true;
}

HttpRequest HttpConnection::ParseRequest(const string &request) const {
  HttpRequest req("/");  // by default, get "/".

  // Plan for STEP 2:
  // 1. Split the request into different lines (split on "\r\n").
  // 2. Extract the URI from the first line and store it in req.URI.
  // 3. For the rest of the lines in the request, track the header name and
  //    value and store them in req.headers_ (e.g. HttpRequest::AddHeader).
  //
  // Hint: Take a look at HttpRequest.h for details about the HTTP header
  // format that you need to parse.
  //
  // You'll probably want to look up boost functions for:
  // - Splitting a string into lines on a "\r\n" delimiter
  // - Trimming whitespace from the end of a string
  // - Converting a string to lowercase.
  //
  // Note: If a header is malformed, skip that line.

  // STEP 2:
  

  // Split on \r or \n then filter out empty strings
  vector<string> raw_lines;
  boost::algorithm::split(
      raw_lines,
      request,
      boost::is_any_of("\r\n"),
      boost::token_compress_off);

  vector<string> lines;
  lines.reserve(raw_lines.size());
  for (auto &line : raw_lines) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }

  // If no lines, return "/"
  if (lines.empty()) {
    return req;
  }

  // Parse the first line to extract URI
  {
    // Delimit on spaces
    vector<string> tokens;
    boost::algorithm::split(tokens, lines[0], boost::is_space(), boost::token_compress_on);

    if (tokens.size() >= 2) {
      req.set_uri(tokens[1]);
    } else {
      return req;
    }
  }

  // Parse rest of lines
  for (size_t i = 1; i < lines.size(); ++i) {
    const string &raw = lines[i];

    // Find colon that separates header name from value
    size_t colon_pos = raw.find(':');
    if (colon_pos == string::npos) {
      // Malformed header, skip
      continue;
    }

    // Extract name and value
    string name = raw.substr(0, colon_pos);
    string value = raw.substr(colon_pos + 1);

    boost::algorithm::trim(name);
    boost::algorithm::trim(value);
    boost::algorithm::to_lower(name);

    // Check that name isn't empty before adding
    if (!name.empty()) {
      req.AddHeader(name, value);
    }
  }

  return req;
}

}  // namespace hw4
