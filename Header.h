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

using Data = std::map<std::string_view, std::string_view>;

std::vector<std::string> split(std::string_view str, char separator) {//�����񕪊��֐�
  std::stringstream ss(std::string(str) + separator);
  std::string buf;
  std::vector<std::string> res;
  while (std::getline(ss, buf, separator)) {
	res.push_back(buf);
  }
}

class Connection {
private:
  std::string myIP;
  unsigned short port;
  std::thread server;
  std::queue<Data> temporary_data;

  int getIP(std::string& IP) {//������IP�A�h���X����������
	WSADATA wsadata;
	if (WSAStartup(MAKEWORD(1, 1), &wsadata) != 0)return -1;
	char hostname[256];//�z�X�g�l�[���̎擾
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

  int sendRequest(std::string_view IP, std::string_view port, Data data) {//`IP`��`data`�𑗂�(�߂�l�̓��X�|���X)
	net::io_context ioc;
	tcp::resolver resolver(ioc);
	beast::tcp_stream stream(ioc);
	const auto results = resolver.resolve(IP, port);
	stream.connect(results);
	http::request<http::string_body> req(http::verb::post, "/", 11);
	req.set(http::field::host, IP);
	req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
	//���N�G�X�g�𑗂邽�߂̏���

	{//data�𕶎���ɕϊ����Ă�����
	  std::string parsed = "";
	  for (const auto& m : data) {
		parsed += m.first;
		parsed += '=';
		parsed += m.second;
		parsed += '&';
	  }
	  parsed.pop_back();
	  req.body() = parsed;
	}

	http::write(stream, req);
	//�����Ŏ��ۂɃ��N�G�X�g�𑗐M
	beast::flat_buffer buf;
	http::response<http::string_body> res;//���X�|���X�󂯎��p�̃C���X�^���X
	http::read(stream, buf, res);
	if (res.body() == "0")return 0;//����
	else return -1;//���s
  }

  template<class Send>
  int returnResponse(http::request<http::string_body> && req, Send && send) {//���N�G�X�g�������Ƃ��̏������s��
	if (req.method() != http::verb::post)return -1;

	Data receivedData;
	{//�����񂩂���ۂ̃f�[�^�ɕϊ�����
	  auto tmp = split(req.body(), '&');
	  for (const auto& v : tmp) {
		auto expr = split(v, '=');
		auto left = expr[0];
		auto right = expr[1];
		receivedData[left] = right;
	  }
	}

	temporary_data.push(receivedData);

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
	  http::request<http::string_body> req;
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

public:

  void waitRequest() {
	net::io_context ioc(1);
	tcp::acceptor acceptor(ioc, { net::ip::make_address(this->myIP), this->port });
	for (;;) {
	  tcp::socket socket(ioc);
	  acceptor.accept(socket);
	  std::thread(&Connection::doSession, this, std::move(socket)).detach();
	}
  }
  explicit Connection(unsigned p) : port(p) {
	if (getIP(myIP) == -1)throw "IP�A�h���X���擾�ł��܂���B";
	else std::cout << "goes well" << std::endl;
	server = std::thread(&Connection::waitRequest, this);
	//HTTP�ʐM�̃X���b�h�𗧂��グ��
  }

  int Send(Data data, std::string_view IP) {//data��IP��IP�A�h���X�ɑ��M����
	if (sendRequest(IP, std::to_string(port), data) != 0)return -1;//���s
	else return 0;
  }

  auto Get() {
	auto data = temporary_data.front();
	temporary_data.pop();
	return data;
  }

  ~Connection() {
	server.join();
  }
  //�f�X�g���N�^

};