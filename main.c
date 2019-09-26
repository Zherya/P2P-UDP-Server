#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Функция использует функцию select() для проверки готовности сокета на чтение или
 * наступления тайм-аута раньше этого события.
 * Функция возвращает -1 в случае тайм-аута ожидания сообщения, -2 - в случае ошибки
 * или число полученных байт в случае успеха: */
ssize_t timeoutRecv(int socketFD, void *buffer, size_t size, struct timeval *timeout) {
    /* Так как нумерация дескрипторов начинается с нуля, то число дескрипторов
     * до последнего проверяемого функцией select() дескриптора
     * (в данном случае одного socketFD) есть величина socketFD + 1: */
    int FDcount = socketFD + 1;
    fd_set FDforRead; /* Набор дескрипторов, проверяемых на доступность чтения */
    /* Сбросим все биты в наборе: */
    FD_ZERO(&FDforRead);
    /* Установим в наборе бит проверяемого дескриптора: */
    FD_SET(socketFD,&FDforRead);
    int returnValue = select(FDcount, &FDforRead, NULL, NULL, timeout);
    /* Если возник тайм-аут или ошибка, то завершаем работу: */
    if (returnValue == 0 || returnValue == -1)
        /* Вычитаем дополнительно единицу, чтобы была возможность различить тайм-аут
         * и сообщение с нулевой длиной (что допустимо для UDP-сокетов): */
        return returnValue - 1;
    /* Иначе дескриптор доступен на чтение (проверять это нет смысла, так как дескриптор один): */
    if ((returnValue = recv(socketFD, buffer, size, 0)) == -1)
        /* Случай ошибки: */
        return - 2;
    else
        return returnValue;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Использование: %s <UDP-порт сервера>\n", argv[0]);
        return 0;
    }

    /* Заполним структуру адреса сервера: */
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    /* INADDR_ANY указывает ядру на то, что сокет готов принимать пакеты
     * с любого из имеющихся в системе IP-адресов/интерфейсов: */
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    /* Переведем номер UDP-порта в числовой формат и запишем в структуру адреса сервера: */
    unsigned long uintServerPortValue = strtoul(argv[1], NULL, 0);
    if (uintServerPortValue > 65535) {
        printf("Значение номера UDP-порта не может превышать значение 65535\n");
        return 0;
    }
    if (uintServerPortValue < 49152)
        printf("Не рекомендуется использовать заранее известные или зарегистрированные порты\n");
    in_port_t serverUDPPort = uintServerPortValue;
    serverAddress.sin_port = htons(serverUDPPort);

    /* Создадим UDP-сокет для соединиения с клиентом: */
    int serverSockFD = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverSockFD == -1) {
        perror("Ошибка создания UDP-сокета для соединения с клиентом");
        return 0;
    }
    /* Привяжем к сокету адрес сервера: */
    if (bind(serverSockFD, (const struct sockaddr *) &serverAddress, sizeof(serverAddress)) == -1) {
        perror("Ошибка задания адреса UDP-сокету");
    }

    /* Получим сообщение от первого клиента: */
    char received[1024];
    struct sockaddr_in clientAddress;
    socklen_t clientAddressLen = sizeof(clientAddress);
    char clientIPv4[INET_ADDRSTRLEN];
    printf("Ожидание сообщения от нового клиента...\n");
    ssize_t bytesSendRecv = recvfrom(serverSockFD, received, 1024, 0, (struct sockaddr *)&clientAddress, &clientAddressLen);
    if (bytesSendRecv == -1) {
        perror("Ошибка получения сообщения от нового клиента");
    } else {
        printf("Новый клиент: ");
        if (inet_ntop(AF_INET, &clientAddress.sin_addr, clientIPv4, INET_ADDRSTRLEN) == NULL) {
            perror("Ошибка получения IPv4-адреса нового клиента");
        } else
            printf("IPv4-адрес -> %s, UDP-порт -> %hu\n", clientIPv4, clientAddress.sin_port);
        printf("Получено: %s\n", received);
    }
    /* Так как предполагается соединение "точка-точка", то вызовем функцию connect() для присоединения сокета
     * к адресу клиента, от которого первым была получена дейтаграмма, чтобы избежать получения пакетов от
     * других клиентов на время взаимодействия с одним клиентом, а также для того, чтобы можно было
     * использовать функции send() и recv() вместо sendto() и recvfrom(): */
    if (connect(serverSockFD, (struct sockaddr *)&clientAddress, clientAddressLen) == -1) {
        perror("Ошибка присоединения UDP-сокета к адресу нового клиента");
        return 0;
    }

    char message[] = "Hey, client, it's server\n";
    printf("Отправка сообщения клиенту...\n");
    if ((bytesSendRecv = send(serverSockFD, message, strlen(message) + 1, 0)) == -1) {
        perror("Ошибка отправки сообщения");
        return 0;
    } else
        printf("Сообщение отправлено\n");
    if (bytesSendRecv != strlen(message) + 1)
        printf("Отправлено %zd байт вместо %zu байт\n", bytesSendRecv, strlen(message) + 1);
    return 0;
}
