#ifndef GameConnect_HEADER
#define GameConnect_HEADER

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define BOOST_DATE_TIME_NO_LIB
#define BOOST_REGEX_NO_LIB
#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include <string_view>
#include <string>
#include <queue>
#include <winsock.h>

using boost::asio::ip::udp;

namespace Jin {

  //constexpr std::size_t MAX_SIZE = 2048;
  using Data = char[2048];

  [[nodiscard]] std::string getIP() {//この関数を実行したマシンのIPアドレスを返す
	WSADATA wsadata;
	if (WSAStartup(MAKEWORD(1, 1), &wsadata) != 0)throw "Cannot get your IP address.";
	char hostname[256];//ホストネームの取得
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

  class GameConnect {
  private:
	boost::asio::io_context ioc;
	udp::resolver resolver{ ioc };
	std::queue<std::decay_t<Data>> temporary_data;
	std::thread sub;
	std::unique_ptr<udp::socket> socket_;
	udp::endpoint endpoint_;
	Data buf = {};
	void serverside(unsigned short port) {//サーバーを立てる
	  socket_ = std::make_unique<udp::socket>(ioc, udp::endpoint(udp::v4(), port));
	  do_receive();
	  ioc.run();
	}

	void do_receive() {
	  socket_->async_receive_from(boost::asio::buffer(buf), endpoint_,
		[this](const boost::system::error_code & ec, std::size_t) {
		  if (ec)throw std::exception(ec.message().c_str());
		  temporary_data.push(buf);//データが来たらキューにプッシュする
		  do_receive();
		}
	  );
	}



  public:
	explicit GameConnect(unsigned short port = 3000) : sub(&GameConnect::serverside, this, port) {}//ポート番号を指定してソケットを開く
	~GameConnect() {
	  ioc.stop();
	  sub.join();
	}

	template<class T>
	void Send(std::string_view IP, unsigned short port, T data) {//IPのportにdataを送信する
	  udp::socket socket(ioc);
	  udp::endpoint endpoint = *resolver.resolve(udp::v4(), IP, std::to_string(port)).begin();
	  socket.open(udp::v4());
	  Data send;
	  *(T*)send = data;
	  std::cout << "Before sending." << std::endl;
	  socket.send_to(boost::asio::buffer(send), endpoint);
	  std::cout << "After sending." << std::endl;
	}

	template<class T>
	T Get(bool state) {//送信されたデータを読む
	  while (state && temporary_data.empty()) { std::cout << "goes well" << std::endl; };//もしキューの中身が空だったらずっと待つ
	  if (!state)throw std::runtime_error("Thread terminated.");
	  auto res = temporary_data.front();
	  temporary_data.pop();
	  return *(T*)res;
	}

	template<class T>
	T Get() {//送信されたデータを読む
	  bool ex = false;
	  return Get<T>(ex);
	}

  };

}

#endif