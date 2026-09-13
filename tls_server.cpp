#include <iostream>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <cstdio>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>

int main()
{
    // 1. TLS 서버의 공통 설정을 담을 객체 생성
    SSL_CTX* context = SSL_CTX_new(TLS_server_method());

    if (context == nullptr)
    {
        ERR_print_errors_fp(stderr);
        return 1;
    }

    // 2. TLS 1.2 이상만 허용
    if (SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(context);
        return 1;
    }

    // 3. 서버 인증서 읽기
    if (SSL_CTX_use_certificate_file(
        context,
        "server-cert.pem",
        SSL_FILETYPE_PEM) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(context);
        return 1;
    }

    // 4. 서버 개인키 읽기
    if (SSL_CTX_use_PrivateKey_file(
        context,
        "server-key.pem",
        SSL_FILETYPE_PEM) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(context);
        return 1;
    }

    // 5. 인증서의 공개키와 개인키가 짝인지 확인
    if (SSL_CTX_check_private_key(context) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(context);
        return 1;
    }

    std::cout << "서버 인증서와 개인키 로드 성공!" << std::endl;

    // TCP 소켓 생성
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket == -1)
    {
        std::perror("socket");
        SSL_CTX_free(context);
        return 1;
    }

    // 서버 재실행 시 주소 재사용 허용
    int reuse = 1;

    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR,
        &reuse, sizeof(reuse)) == -1)
    {
        std::perror("setsockopt");
        close(serverSocket);
        SSL_CTX_free(context);
        return 1;
    }

    // TLS 실습 서버는 8443번 포트 사용
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(8443);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(serverSocket,
        reinterpret_cast<sockaddr*>(&address),
        sizeof(address)) == -1)
    {
        std::perror("bind");
        close(serverSocket);
        SSL_CTX_free(context);
        return 1;
    }

    if (listen(serverSocket, 5) == -1)
    {
        std::perror("listen");
        close(serverSocket);
        SSL_CTX_free(context);
        return 1;
    }

    std::cout << "127.0.0.1:8443에서 연결 대기 중..." << std::endl;

    int clientSocket = accept(serverSocket, nullptr, nullptr);

    if (clientSocket == -1)
    {
        std::perror("accept");
        close(serverSocket);
        SSL_CTX_free(context);
        return 1;
    }

    std::cout << "TCP 연결 성공!" << std::endl;

    // 이 연결에서 사용할 TLS 객체 생성
    SSL* ssl = SSL_new(context);

    if (ssl == nullptr)
    {
        ERR_print_errors_fp(stderr);
        close(clientSocket);
        close(serverSocket);
        SSL_CTX_free(context);
        return 1;
    }

    // 연결된 TCP 소켓을 TLS 객체에 연결
    if (SSL_set_fd(ssl, clientSocket) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(clientSocket);
        close(serverSocket);
        SSL_CTX_free(context);
        return 1;
    }

    // 서버 측 TLS 핸드셰이크
    int result = SSL_accept(ssl);

    if (result != 1)
    {
        int error = SSL_get_error(ssl, result);
        std::cerr << "TLS 핸드셰이크 실패. 오류 코드: "
            << error << std::endl;
        ERR_print_errors_fp(stderr);
    }
    else
    {
        std::cout << "TLS 핸드셰이크 성공!" << std::endl;

        // 이번 실습에서는 11바이트를 받기로 약속
        constexpr std::size_t expectedLength = 11;
        std::string message;
        bool success = true;

        while (message.size() < expectedLength)
        {
            char buffer[11];

            int received = SSL_read(
                ssl,
                buffer,
                static_cast<int>(expectedLength - message.size())
            );

            if (received <= 0)
            {
                int error = SSL_get_error(ssl, received);
                std::cerr << "TLS 수신 종료 또는 실패: "
                    << error << std::endl;
                ERR_print_errors_fp(stderr);
                success = false;
                break;
            }

            message.append(buffer, static_cast<std::size_t>(received));
        }

        if (success)
        {
            std::cout << "받은 내용: " << message << std::endl;

            std::size_t totalSent = 0;

            while (totalSent < message.size())
            {
                int sent = SSL_write(
                    ssl,
                    message.data() + totalSent,
                    static_cast<int>(message.size() - totalSent)
                );

                if (sent <= 0)
                {
                    int error = SSL_get_error(ssl, sent);
                    std::cerr << "TLS 전송 실패: " << error << std::endl;
                    ERR_print_errors_fp(stderr);
                    success = false;
                    break;
                }

                totalSent += static_cast<std::size_t>(sent);
            }
        }

        if (success)
        {
            std::cout << "답장 전송 완료!" << std::endl;

            // 종료 알림을 보내고 상대의 종료 알림도 기다림
            int shutdownResult = SSL_shutdown(ssl);

            if (shutdownResult == 0)
            {
                shutdownResult = SSL_shutdown(ssl);
            }

            if (shutdownResult != 1)
            {
                std::cerr << "TLS 종료 교환이 완료되지 않음.\n";
            }
        }

        result = success ? 1 : 0;
    }

    SSL_free(ssl);
    close(clientSocket);
    close(serverSocket);
    SSL_CTX_free(context);

    return result == 1 ? 0 : 1;
    return 0;
}
