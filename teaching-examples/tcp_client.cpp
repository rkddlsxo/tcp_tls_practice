// 교육용 TCP Echo 클라이언트: 서버와 줄바꿈으로 끝나는 메시지를 한 번 교환한다.
// recv() 호출 횟수는 send() 호출 횟수나 패킷 수와 일치하지 않는다.
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

// 데이터를 끝까지 전송한다.
// 성공: true, 실패: false
bool sendAll(int socketFd, const char* data, std::size_t length)
{
    std::size_t totalSent = 0;

    while (totalSent < length)
    {
        ssize_t sent = send(
            socketFd,
            data + totalSent,
            length - totalSent,
            MSG_NOSIGNAL
        );

        if (sent == -1)
        {
            // 신호로 중단된 경우 다시 시도한다.
            if (errno == EINTR)
            {
                continue;
            }

            std::perror("send");
            return false;
        }

        if (sent == 0)
        {
            return false;
        }

        totalSent += static_cast<std::size_t>(sent);
    }

    return true;
}

int main()
{
    // 1. IPv4 TCP 소켓 생성
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket == -1)
    {
        std::perror("socket");
        return 1;
    }

    // 2. 연결할 서버 주소 설정: 127.0.0.1:18080
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(18080);
    serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // 3. 서버 연결
    if (connect(
        clientSocket,
        reinterpret_cast<sockaddr*>(&serverAddress),
        sizeof(serverAddress)) == -1)
    {
        std::perror("connect");
        close(clientSocket);
        return 1;
    }

    std::cout << "서버 연결에 성공했습니다." << std::endl;

    // 4. 메시지 전체 전송
    const char* message = "Hello, TCP!\n"; // 문장은 바꿔도 되지만 끝의 \n은 유지
    const std::size_t messageLength = std::strlen(message);

    if (!sendAll(clientSocket, message, messageLength))
    {
        std::cerr << "메시지 전송에 실패했습니다.\n";
        close(clientSocket);
        return 1;
    }

    std::cout << "메시지 전송을 완료했습니다. ("
        << messageLength << "바이트)" << std::endl;

    std::string reply;

    // 메시지의 끝은 길이가 아니라 줄바꿈으로 구분한다.
    // 개념 설명을 위해 한 바이트씩 읽는다. 실제 서비스는 버퍼 단위로 처리한다.
    while (reply.empty() || reply.back() != '\n')
    {
        if (reply.size() >= 4096)
        {
            std::cerr << "응답이 최대 허용 길이(4096바이트)를 초과했습니다.\n";
            close(clientSocket);
            return 1;
        }
        char ch;
        ssize_t receivedBytes = recv(clientSocket, &ch, 1, 0);
        if (receivedBytes == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            std::perror("recv");
            close(clientSocket);
            return 1;
        }

        if (receivedBytes == 0)
        {
            std::cerr << "응답 수신이 완료되기 전에 서버가 연결을 종료했습니다.\n";
            close(clientSocket);
            return 1;
        }

        reply.push_back(ch);


    }

    std::cout << "서버의 전체 답장: " << reply << std::endl;
    close(clientSocket);
    return 0;
}
