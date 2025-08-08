#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <netinet/in.h>  // sockaddr_in
#include "../logger/Logger.hpp"
#include "Config.hpp"
#include "LocationConfig.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "CGIHandler.hpp"
#include "utils.hpp"

class Config;

class WebServer {
public:
    explicit WebServer(const Config& cfg);
    ~WebServer();
    void shutdown();
    int  handleNewConnection(int listen_fd);
    void handleClientDataOn(int client_fd);
    const std::vector<int>& getListeningSockets() const { return listening_sockets; }
    std::vector<int> getClientSockets() const {
    std::vector<int> v;
    v.reserve(conns_.size());
    
    for (std::map<int,Connection>::const_iterator it = conns_.begin();
         it != conns_.end();
         ++it)
    {
        v.push_back(it->first);
    }
    return v;
}
    void handle_new_connection(int listen_fd);
    void handleNewConnectionOn(int listen_fd) { handle_new_connection(listen_fd); };
    std::string read_file(const std::string& path);
    void queueResponse(int client_fd,
                      const std::string& rawResponse);

    bool hasPendingWrite(int client_fd) const;
    void flushPendingWrites(int client_fd);
private:
    const Config*                 config_;

    std::vector<int>              listening_sockets;
    void make_socket_non_blocking(int fd);
    void cleanup_client(int client_fd, int i);

    struct Connection {
      std::string readBuf;    // accumulated request bytes
      std::string writeBuf;   // bytes queued for response
      bool        shouldCloseAfterWrite;
      Connection() : readBuf(), writeBuf(), shouldCloseAfterWrite(false) {}
    };

    std::map<int, Connection> conns_;
    std::string resolve_path(const std::string& raw_path,
                             const std::string& method,
                             const LocationConfig* loc);

    // Helpers that drive GET/POST/DELETE/CGI/etc.
    void handle_get    (const Request&, const LocationConfig*, int, size_t);
    void handle_post   (const Request&, const LocationConfig*, int, size_t);
    void handle_delete (const Request&, const LocationConfig*, int, size_t);
    void handle_cgi    (const LocationConfig*, const Request&, int, size_t);

    void handle_directory_request(const std::string&, const std::string&,
                                  const LocationConfig*, int, size_t);
    void handle_file_request     (const std::string&, int, size_t);

    bool handle_upload            (const Request&, const LocationConfig*, int, size_t);
    bool is_valid_upload_request  (const Request&, const LocationConfig*);
    void process_upload_content   (const Request&, std::string&, std::string&);
    std::string make_upload_filename(const std::string&);
    bool write_upload_file        (const std::string&, const std::string&);
    void send_upload_success_response(int, const std::string&, size_t);

    void send_ok_response     (int, const std::string&, const std::map<std::string,std::string>&, size_t);
    void send_error_response  (int, int, const std::string&, size_t);
    void send_file_response   (int, const std::string&, size_t);
    void send_redirect_response(int, int, const std::string&, size_t);

    size_t find_header_end          (const std::string&);
    bool   read_and_append_client_data(int, size_t);
    int    parse_content_length     (const std::string&);
    bool   is_full_body_received    (const Request&, const std::string&, size_t);
    void   process_request          (Request&, int, size_t);
    static std::string timestamp();
    void handle_client_fd(int client_fd);
};

std::string extract_file_from_multipart(const std::string& body, std::string& filename);

inline std::string to_str(int n) {
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

#endif

