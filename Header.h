#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/config.hpp>
#include <winsock.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <string_view>
#include <queue>
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

class Connection {
private:
  std::string myIP;
  unsigned short port;
  std::thread game;
  std::thread server;
  std::queue<void*> temporary_data;

  int getIP(std::string& IP) {//引数にIPアドレスを書き込む
	WSADATA wsadata;
	if (WSAStartup(MAKEWORD(1, 1), &wsadata) != 0)return -1;
	char hostname[256];//ホストネームの取得
	if (gethostname(hostname, sizeof(hostname)) != 0)return -1;
	HOSTENT * hostend = gethostbyname(hostname);
	if (hostend == NULL)return -1;
	IN_ADDR inaddr;
	memcpy(&inaddr, hostend->h_addr_list[0], 4);
	char tmpIP[256];
	strcpy_s(tmpIP, inet_ntoa(inaddr));
	IP = std::string(tmpIP);
	WSACleanup();
	return 0;
  }

  template<class Send>
  int handleRequest(http::request<http::dynamic_body> && req, Send && send) {//リクエストが来たときの処理を行う
	if (req.method() != http::verb::post)return -1;
	this->temporary_data.push(req.body());
	http::response<http::string_body> res;
	res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
	res.set(http::field::content_type, "application/text");
	res.keep_alive(req.keep_alive());
	res.body() = "0";
	res.prepare_payload();
	send(std::move(res));
	return 0;
  }

  void fail(const beast::error_code & ec, const char* what) {//失敗を報告する
	std::cout << what << ": " << ec.message() << std::endl;
  }

  template<class Stream>
  struct sendLambda {
	Stream& stream;
	bool& close;
	beast::error_code& ec;
	explicit sendLambda(Stream& s, bool& c, beast::error_code& e) : stream(s), close(c), ec(e) {}
	template<bool isRequest, class Body, class Fields>
	void operator()(http::message<isRequest, Body, Fields>&& msg) {
	  close = msg.need_eof();
	  http::serializer<isRequest, Body, Fields> sr(msg);
	  http::write(stream, sr, ec);
	}
  };

  void doSession(tcp::socket &&socket) {//実際にリクエストを受け取ってレスポンスを返す
	bool close = false;
	beast::error_code ec;
	beast::flat_buffer buf;
	sendLambda<tcp::socket> lambda(socket, close, ec);
	for (;;) {
	  http::request<http::string_body> req;
	  http::read(socket, buf, req, ec);
	  if (ec == http::error::end_of_stream)return;
	  if (ec)return fail(ec, "read");
	}
  }
public:
  
  void waitRequest() {
	net::io_context ioc(1);
	tcp::acceptor acceptor(ioc, { net::ip::make_address(this->myIP), this->port });
	for (;;) {
	  tcp::socket socket(ioc);
	  acceptor.accept(socket);
	  std::thread(&Connection::doSession, std::move(socket)).detach();
	}
  }
  explicit Connection(void (*mainloop)(), unsigned p) : game(&mainloop, this), server(&Connection::waitRequest, this), port(p) {
	if (getIP(myIP) == -1)throw "IPアドレスを取得できません。";
	else std::cout << "goes well" << std::endl;
  }

  int Send(std::map<std::string, std::string> data, std::string_view IP) {//dataをIPのIPアドレスに送信する
	//まずこれらのデータを文字列に変換する
	std::string req = "";
	for (const auto& m : data) {
	  std::string tmp = "";
	  tmp += m.first;
	  tmp += "=";
	  tmp += m.second;
	  tmp += "&";
	  req.append(tmp);
	}
  }
  void* Get() {
	auto ptr = temporary_data.front();
	temporary_data.pop();
	return ptr;
  }

  ~Connection() {
	server.join();
	game.join();
  }

};