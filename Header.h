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
#include <map>
#include <sstream>
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

using Data = char[256];

std::vector<std::string> split(std::string_view str, char separator) {//�����񕪊��֐�
  std::stringstream ss(std::string(str) + separator);
  std::string buf;
  std::vector<std::string> res;
  while (std::getline(ss, buf, separator)) {
	res.push_back(buf);
  }
  return res;
}

[[nodiscard]] std::string getIP() {//������IP�A�h���X����������
  WSADATA wsadata;
  if (WSAStartup(MAKEWORD(1, 1), &wsadata) != 0)throw "Cannot get your IP address.";
  char hostname[256];//�z�X�g�l�[���̎擾
  if (gethostname(hostname, sizeof(hostname)) != 0)throw "Cannot get your IP address.";
  HOSTENT * hostend = gethostbyname(hostname);
  if (hostend == NULL)throw "Cannot get your IP address.";
  IN_ADDR inaddr;
  memcpy(&inaddr, hostend->h_addr_list[0], 4);
  char tmpIP[256];
  strcpy_s(tmpIP, inet_ntoa(inaddr));
  std::string IP = std::string(tmpIP);
  WSACleanup();
  return IP;
}

class Connection {
private:
  std::string myIP;
  unsigned short port;
  std::thread server;
  std::queue<Data*> temporary_data;

  int sendRequest(std::string_view IP, std::string_view port, const Data& data, std::size_t size) {//`IP`��`port`��`data`�𑗂�(�߂�l�̓��X�|���X)
	net::io_context ioc;
	tcp::resolver resolver(ioc);
	beast::tcp_stream stream(ioc);
	const auto results = resolver.resolve(IP, port);
	stream.connect(results);
	http::request<http::buffer_body> req(http::verb::post, "/", 11);
	req.set(http::field::host, IP);
	req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
	//���N�G�X�g�𑗂邽�߂̏���
	req.body().data = (void*)data;
	req.body().size = size;
	req.body().more = false;

	http::write(stream, req);
	//�����Ŏ��ۂɃ��N�G�X�g�𑗐M
	beast::flat_buffer buf;
	http::response<http::string_body> res;//���X�|���X�󂯎��p�̃C���X�^���X
	http::read(stream, buf, res);
	if (res.body() == "0")return 0;//����
	else return -1;//���s
  }

  template<class Send>
  int returnResponse(http::request<http::buffer_body> && req, Send && send) {//���N�G�X�g�������Ƃ��̏������s��
	if (req.method() != http::verb::post)return -1;

	std::cout << "A request appeared." << std::endl;

	Data * data = reinterpret_cast<Data*>(req.body().data);
	temporary_data.push(data);

	http::response<http::string_body> res;
	res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
	res.set(http::field::content_type, "application/text");
	res.keep_alive(req.keep_alive());
	res.body() = "0";
	res.prepare_payload();
	send(std::move(res));
	return 0;
  }

  void fail(const beast::error_code & ec, const char* what) {//���s��񍐂���
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

  void doSession(tcp::socket && socket) {//���ۂɃ��N�G�X�g���󂯎���ă��X�|���X��Ԃ�
	bool close = false;
	beast::error_code ec;
	beast::flat_buffer buf;
	sendLambda<tcp::socket> lambda(socket, close, ec);
	for (;;) {
	  http::request<http::buffer_body> req;
	  http::read(socket, buf, req, ec);
	  if (ec == http::error::end_of_stream)return;
	  if (ec)return fail(ec, "read");//���N�G�X�g�̓ǂݍ��ݎ��s
	  returnResponse(std::move(req), lambda);
	  //�����Ń��X�|���X��Ԃ�
	  if (ec)return fail(ec, "write");//���X�|���X�ɏ������ݎ��s
	  if (close)break;
	}
	socket.shutdown(tcp::socket::shutdown_send, ec);
  }

  void waitRequest() {
	net::io_context ioc{ 1 };
	tcp::acceptor acceptor(ioc, { net::ip::make_address(myIP), port });
	for (;;) {
	  tcp::socket socket{ ioc };
	  acceptor.accept(socket);
	  std::thread(&Connection::doSession, this, std::move(socket)).detach();
	}
  }

public:
  explicit Connection(unsigned int p = 3000) : port(p) {
	myIP = getIP();
	server = std::thread(&Connection::waitRequest, this);
	//HTTP�ʐM�̃X���b�h�𗧂��グ��
  }

  int Send(const Data & data, std::size_t size, std::string_view IP, unsigned short port) {//data��IP��IP�A�h���X�ɑ��M����
	if (sendRequest(IP, std::to_string(port), data, size) != 0)return -1;//���s
	else return 0;
  }

  auto Get() {
	while (temporary_data.empty());//�L���[����ɂȂ�Ȃ�����҂�
	auto data = temporary_data.front();
	temporary_data.pop();
	return data;
  }

  ~Connection() {
	server.join();
  }
  //�f�X�g���N�^

};