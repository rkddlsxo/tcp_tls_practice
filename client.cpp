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

    // 2. 연결할 서버 주소 설정: 127.0.0.1:8080
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
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

    std::cout << "서버 연결 성공!" << std::endl;

    // 4. 메시지 전체 전송
    const char* message = "hello hello";
    const std::size_t messageLength = std::strlen(message);

    if (!sendAll(clientSocket, message, messageLength))
    {
        std::cerr << "메시지 전송 실패\n";
        close(clientSocket);
        return 1;
    }

    std::cout << "메시지 전체 전송 완료! ("
        << messageLength << "바이트)" << std::endl;

    std::string reply;

    char buffer[4];

    while (reply.size() < messageLength)
    {
        std::size_t remaining = messageLength - reply.size();

        std::size_t receiveSize = remaining;
        if (receiveSize > sizeof(buffer))
        {
            receiveSize = sizeof(buffer);
        }

        ssize_t receivedBytes = recv(
            clientSocket,
            buffer,
            receiveSize,
            0
        );

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
            std::cerr << "답장을 전부 받기 전에 서버가 연결을 종료했어.\n";
            close(clientSocket);
            return 1;
        }

        reply.append(buffer, static_cast<std::size_t>(receivedBytes));

        std::cout << "이번에 받은 크기: " << receivedBytes
            << ", 누적: " << reply.size()
            << "/" << messageLength << std::endl;
    }

    std::cout << "서버의 전체 답장: " << reply << std::endl;
    return 0;
}
