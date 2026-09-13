#include <cstdio>
#include <iostream>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <string>

int main()
{
    // 1. 클라이언트용 TLS 설정 생성
    SSL_CTX* context = SSL_CTX_new(TLS_client_method());

    if (context == nullptr)
    {
        ERR_print_errors_fp(stderr);
        return 1;
    }

    if (SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(context);
        return 1;
    }

    // 2. 서버 인증서 검증 활성화
    SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);

    // 실습에서 직접 만든 인증서를 신뢰하도록 설정
    if (SSL_CTX_load_verify_locations(
        context, "server-cert.pem", nullptr) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(context);
        return 1;
    }

    // 3. TCP 소켓 생성
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket == -1)
    {
        std::perror("socket");
        SSL_CTX_free(context);
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(8443);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // 4. TCP 연결
    if (connect(clientSocket,
        reinterpret_cast<sockaddr*>(&address),
        sizeof(address)) == -1)
    {
        std::perror("connect");
        close(clientSocket);
        SSL_CTX_free(context);
        return 1;
    }

    std::cout << "TCP 연결 성공!" << std::endl;

    // 5. 연결 하나를 위한 TLS 객체 생성
    SSL* ssl = SSL_new(context);

    if (ssl == nullptr)
    {
        ERR_print_errors_fp(stderr);
        close(clientSocket);
        SSL_CTX_free(context);
        return 1;
    }

    // 6. 인증서에 접속 대상 IP가 포함되어 있는지도 검증
    if (X509_VERIFY_PARAM_set1_ip_asc(
        SSL_get0_param(ssl), "127.0.0.1") != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(clientSocket);
        SSL_CTX_free(context);
        return 1;
    }

    // 7. TCP 소켓과 TLS 객체 연결
    if (SSL_set_fd(ssl, clientSocket) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(clientSocket);
        SSL_CTX_free(context);
        return 1;
    }

    // 8. 클라이언트 측 TLS 핸드셰이크
    int result = SSL_connect(ssl);

    if (result != 1)
    {
        int error = SSL_get_error(ssl, result);

        std::cerr << "TLS 핸드셰이크 실패. 오류 코드: "
            << error << std::endl;
        ERR_print_errors_fp(stderr);
    }
    else
    {
        std::cout << "서버 인증서 검증 및 TLS 연결 성공!"
            << std::endl;

        const std::string message = "hello hello";
        std::size_t totalSent = 0;
        bool success = true;

        // 메시지 전송
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

        // 보낸 길이만큼 Echo 답장 수신
        std::string reply;

        while (success && reply.size() < message.size())
        {
            char buffer[11];

            int received = SSL_read(
                ssl,
                buffer,
                static_cast<int>(message.size() - reply.size())
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

            reply.append(buffer, static_cast<std::size_t>(received));
        }

        if (success)
        {
            std::cout << "서버의 답장: " << reply << std::endl;

            int shutdownResult = SSL_shutdown(ssl);

            if (shutdownResult == 0)
            {
                shutdownResult = SSL_shutdown(ssl);
            }

            if (shutdownResult != 1)
            {
                std::cerr << "TLS 종료 교환이 완료 되지 않음.\n";
            }
        }

        result = success ? 1 : 0;
    }

    // 9. 자원 정리
    SSL_free(ssl);
    close(clientSocket);
    SSL_CTX_free(context);

    return result == 1 ? 0 : 1;
}
