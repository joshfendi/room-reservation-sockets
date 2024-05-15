#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <vector>
#include <utility>
#include <sstream>
#include <iostream>
#include <fstream>

#define UDPPORT "44966" // the port users will be connecting to

// UDP PORT NUMBERS FOR S D AND U
#define UDPS "41966"
#define UDPD "42966"
#define UDPU "43966"

#define TCPPORT "45966" // the port users will be connecting to

#define MAXBUFLEN 100
#define BACKLOG 10 // how many pending connections queue will hold

// FROM BEEJ
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// parse data received from backend
std::vector<std::pair<std::string, int> > parseData(const std::string &dataStr)
{
    std::stringstream ss(dataStr);
    std::string line;
    std::vector<std::pair<std::string, int> > parsedData;

    // get each line of data
    while (std::getline(ss, line))
    {
        std::stringstream lineStream(line);
        std::string room;
        int count;

        // get room code up to comma, then get int
        if (std::getline(lineStream, room, ',') && (lineStream >> count))
        {
            // add to parsed data
            parsedData.push_back(std::make_pair(room, count));
        }
        else
        {
            std::cerr << "Error parsing line: " << line << std::endl;
        }
    }

    return parsedData;
}

// check validity of username and password
int checkUserPass(const std::string &username, const std::string &password)
{
    // valid members
    std::ifstream file("member.txt"); // (encrypted user and pass)
    std::string userPassPair;

    // parse each line for username and password
    while (std::getline(file, userPassPair))
    {
        std::stringstream ss(userPassPair);
        std::string user, pass;

        // parse based on delimeter ','
        if (std::getline(ss, user, ',') && std::getline(ss, pass, ','))
        {
            // take out white space
            pass = pass.substr(1);

            // change usernames to lowercase for comparison
            std::string lowercaseUser = user;
            for (size_t i = 0; i < lowercaseUser.length(); i++)
            {
                lowercaseUser[i] = tolower(lowercaseUser[i]);
            }

            // valid member, log in
            if ((username == lowercaseUser) && (password == pass))
            {
                return 1; // Username and password match
            }
            // valid member, but wrong password
            else if (username == lowercaseUser && password != pass)
            {
                return 3; // username match, password doesnt match
            }
        }
    }

    return 0; // Username does not exist or password does not match
}

// send message over UDP to designated backend, either serverS, serverD, or serverU
void UDPforwardToBackend(const std::string &message, const char *serverPort)
{
    // FROM BEEJ
    struct addrinfo hints, *servinfo, *p;
    int sockfd, rv;
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo("localhost", serverPort, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return;
    }

    // loop through all the results and make a socket
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("talker: socket");
            continue;
        }
        break;
    }

    if ((numbytes = sendto(sockfd, message.c_str(), message.length(), 0, p->ai_addr, p->ai_addrlen)) == -1)
    {
        perror("talker: sendto");
        exit(1);
    }

    freeaddrinfo(servinfo);
    close(sockfd);

    std::cout << "The main server sent a request to Server " << message[0] << ".\n";
}

// send data over TCP to client
void TCPSendToClient(int socket, const std::string data)
{ // FROM BEEJ
    if (send(socket, data.c_str(), data.length(), 0) == -1)
    {
        perror("send");
    }
}

int main(void)
{
    // FROM BEEJ
    int udpSockfd, tcpSockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;

    char buf[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    printf("The main server is up and running.\n");

    // server names
    std::vector<char> servers;
    servers.push_back('S');
    servers.push_back('D');
    servers.push_back('U');

    // data pairs
    std::vector<std::pair<std::string, int> > receivedData;

    if ((rv = getaddrinfo(NULL, UDPPORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((udpSockfd = socket(p->ai_family, p->ai_socktype,
                                p->ai_protocol)) == -1)
        {
            perror("listener: socket");
            continue;
        }

        if ((bind(udpSockfd, p->ai_addr, p->ai_addrlen)) == -1)
        {
            close(udpSockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    struct sockaddr_storage their_addr;
    addr_len = sizeof their_addr;

    // listen to UDP three times
    for (int i = 0; i < 3; ++i)
    {

        if ((numbytes = recvfrom(udpSockfd, buf, MAXBUFLEN - 1, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
        {
            perror("recvfrom");
            exit(1);
        }

        buf[numbytes] = '\0';

        // Parse data and store in vector
        std::vector<std::pair<std::string, int> > parsedData;
        std::string receivedStr = std::string(buf, numbytes);
        parsedData = parseData(receivedStr);

        // received data is ALL the data from single.txt, double.txt, and suite.txt, all sent over by backend servers
        receivedData.insert(receivedData.end(), parsedData.begin(), parsedData.end());

        printf("The main server has received the room status from Server %c using UDP over port %s.\n", servers[i], UDPPORT);
    }

    // FROM BEEJ
    // TCP connection with CLIENT
    int tcpNew_fd;
    struct addrinfo tcpHints, *tcpServinfo, *tcpP;
    struct sockaddr_storage tcpTheir_addr;
    socklen_t tcpSin_size;
    int tcpYes = 1;

    // TCP server
    memset(&tcpHints, 0, sizeof tcpHints);
    tcpHints.ai_family = AF_UNSPEC;
    tcpHints.ai_socktype = SOCK_STREAM;
    tcpHints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, TCPPORT, &tcpHints, &tcpServinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for (tcpP = tcpServinfo; tcpP != NULL; tcpP = tcpP->ai_next)
    {
        // socket()
        if ((tcpSockfd = socket(tcpP->ai_family, tcpP->ai_socktype, tcpP->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(tcpSockfd, SOL_SOCKET, SO_REUSEADDR, &tcpYes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        // bind()
        if (bind(tcpSockfd, tcpP->ai_addr, tcpP->ai_addrlen) == -1)
        {
            close(tcpSockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (tcpP == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    freeaddrinfo(tcpServinfo); // all done with this structure

    // listen()
    if (listen(tcpSockfd, BACKLOG) == -1)
    {
        perror("listen");
        exit(1);
    }

    while (true)

    {
        // FROM BEEJ
        // main accept() loop
        tcpSin_size = sizeof tcpTheir_addr;

        // accept()
        tcpNew_fd = accept(tcpSockfd, (struct sockaddr *)&tcpTheir_addr, &tcpSin_size);
        if (tcpNew_fd == -1)
        {
            perror("accept");
            continue;
        }

        inet_ntop(tcpTheir_addr.ss_family,
                  get_in_addr((struct sockaddr *)&tcpTheir_addr),
                  s, sizeof s);
        // printf("server: got connection from %s\n", s);

        // FROM BEEJ
        // fork for multiple clients
        if (!fork())
        {
            std::string receivedUsername;
            std::string receivedPassword;

            // USERNAME AND PASSWORD
            // child process
            close(tcpSockfd);

            // recv() and process client request
            char buf[MAXBUFLEN];

            std::string loginResponse;

            // not valid login
            while (loginResponse != "1" && loginResponse != "2")
            {
                int numbytes = recv(tcpNew_fd, buf, MAXBUFLEN - 1, 0);
                if (numbytes == -1)
                {
                    perror("recv");
                    close(tcpNew_fd); // close client socket
                    exit(1);          // exit child
                }

                buf[numbytes] = '\0';

                // split into user and pass
                std::string receivedUserPass(buf, numbytes);
                size_t delimPos = receivedUserPass.find(','); // ',' is delimeter

                if (delimPos != std::string::npos)
                {
                    receivedUsername = receivedUserPass.substr(0, delimPos);
                    receivedPassword = receivedUserPass.substr(delimPos + 1);

                    std::cout << "The main server received the authentication for " << receivedUsername << " using TCP over port " << TCPPORT << ".\n";

                    // GUEST- password is Enter (not really but for clarity)
                    if (receivedPassword == "Hqwhu")
                    {
                        loginResponse = "2";
                        std::cout << "The main server received the guest request for " << receivedUsername << " using TCP over port " << TCPPORT << ".\n";
                        std::cout << "The main server accepts <username> as a guest.\n";

                        TCPSendToClient(tcpNew_fd, loginResponse);
                        printf("The main server sent the guest response to the client.\n");
                    }
                    else
                    {
                        int credStatus = checkUserPass(receivedUsername, receivedPassword);
                        // valid member
                        if (credStatus == 1)
                        {
                            // login is successful and a member
                            loginResponse = "1"; // Successful and a member
                            TCPSendToClient(tcpNew_fd, loginResponse);
                            printf("The main server sent the authentication result to the client.\n");
                        }
                        // pasword doesnt match
                        else if (credStatus == 3)
                        {
                            // login is successful and a member
                            loginResponse = "3"; // Successful and a member
                            TCPSendToClient(tcpNew_fd, loginResponse);
                        }
                        else
                        {
                            // login failed
                            loginResponse = "0";
                            TCPSendToClient(tcpNew_fd, loginResponse);
                        }
                    }
                }
            }

            while (true)
            {
                // RECEIVE ROOM AND REQUEST TYPE
                int numbytes = recv(tcpNew_fd, buf, MAXBUFLEN - 1, 0);
                if (numbytes == -1)
                {
                    perror("recv");
                    close(tcpNew_fd); // close client socket
                    exit(1);          // exit child
                }

                buf[numbytes] = '\0';

                // split into user and pass
                std::string receivedRoomRequest(buf, numbytes);
                size_t delimPos = receivedRoomRequest.find(',');
                char roomType = receivedRoomRequest[0];

                if (delimPos != std::string::npos)
                {
                    std::string receivedRoom = receivedRoomRequest.substr(0, delimPos);
                    std::string receivedRequest = receivedRoomRequest.substr(delimPos + 1);

                    // AVAILABILITY
                    if (receivedRequest == "Availability")
                    {
                        std::cout << "The main server has received the availability request on Room " << receivedRoom << " from " << receivedUsername
                                  << " using TCP over port " << TCPPORT << ".\n";

                        receivedRoom = "A" + receivedRoom;
                        // std::cout << "roomType" << roomType << std::endl;

                        std::string availabilityCode = "3";

                        if (roomType == 'S')
                        {
                            UDPforwardToBackend(receivedRoom, UDPS);
                            // receive availability code from serverS
                            if ((numbytes = recvfrom(udpSockfd, buf, MAXBUFLEN - 1, 0,
                                                     (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            std::cout << "The main server received the response from Server S using UDP over port " << UDPPORT << ".\n";

                            buf[numbytes] = '\0'; // Null-terminate the string

                            // change availabilityCode
                            availabilityCode = buf;
                        }
                        else if (roomType == 'D')
                        {
                            UDPforwardToBackend(receivedRoom, UDPD);
                            // receive availability code from serverS
                            if ((numbytes = recvfrom(udpSockfd, buf, MAXBUFLEN - 1, 0,
                                                     (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            std::cout << "The main server received the response from Server D using UDP over port " << UDPPORT << ".\n";

                            buf[numbytes] = '\0'; // Null-terminate the string

                            // change availabilityCode
                            availabilityCode = buf;
                        }
                        else if (roomType == 'U')
                        {
                            UDPforwardToBackend(receivedRoom, UDPU);
                            // receive availability code from serverS
                            if ((numbytes = recvfrom(udpSockfd, buf, MAXBUFLEN - 1, 0,
                                                     (struct sockaddr *)&their_addr, &addr_len)) == -1)
                            {
                                perror("recvfrom");
                                exit(1);
                            }
                            std::cout << "The main server received the response from Server U using UDP over port " << UDPPORT << ".\n";

                            buf[numbytes] = '\0'; // Null-terminate the string

                            // change availabilityCode
                            availabilityCode = buf;
                        }
                        else
                        {
                            // no valid room type
                        }

                        // send to client
                        TCPSendToClient(tcpNew_fd, availabilityCode);
                        std::cout << "The main server sent the availability information to the client.\n";
                    }
                    // RESERVATION
                    else if (receivedRequest == "Reservation")
                    {
                        std::cout << "The main server has received the reservation request on Room " << receivedRoom << " from " << receivedUsername
                                  << " using TCP over port " << TCPPORT << ".\n";

                        // default code, room not available
                        std::string reservationCode = "2";

                        receivedRoom = "R" + receivedRoom;
                        // GUEST
                        if (loginResponse == "2")
                        {
                            reservationCode = "4";
                            std::cout << receivedUsername << " cannot make a reservation.\n";
                            // send to client
                            TCPSendToClient(tcpNew_fd, reservationCode);
                            std::cout << "The main server sent the error message to the client.\n";
                        }
                        // MEMBER
                        else if (loginResponse == "1")
                        {
                            // if theres a valid room type
                            if (roomType == 'S' || roomType == 'D' || roomType == 'U')
                            {
                                if (roomType == 'S')
                                {
                                    UDPforwardToBackend(receivedRoom, UDPS);
                                }
                                else if (roomType == 'D')
                                {
                                    UDPforwardToBackend(receivedRoom, UDPD);
                                }
                                else if (roomType == 'U')
                                {
                                    UDPforwardToBackend(receivedRoom, UDPU);
                                }

                                // receive availability code from server<S/D/U>
                                if ((numbytes = recvfrom(udpSockfd, buf, MAXBUFLEN - 1, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
                                {
                                    perror("recvfrom");
                                    exit(1);
                                }

                                buf[numbytes] = '\0'; // Null-terminate the string

                                // change availabilityCode
                                reservationCode = buf;

                                if (reservationCode == "1")
                                {
                                    std::cout << "The main server received the response and the updated room status from Server " << roomType << " using UDP over port " << UDPPORT << ".\n";
                                }
                                else
                                {
                                    std::cout << "The main server received the response from Server " << roomType << " using UDP over port " << UDPPORT << ".\n";
                                }
                            }
                            else
                            {
                                reservationCode = "3";
                            }

                            // print if room updated
                            if (reservationCode == "1")
                            {

                                std::string tempRoom = receivedRoom.substr(1); // remove the R

                                // update the room status
                                for (std::vector<std::pair<std::string, int> >::iterator it = receivedData.begin(); it != receivedData.end(); ++it)
                                {
                                    if (it->first == tempRoom)
                                    {
                                        if (it->second > 0)
                                        {
                                            reservationCode = "1";
                                            --(it->second); // decrement room count to make reservation
                                        }
                                    }
                                }

                                std::cout << "The room status of Room " << tempRoom << " has been updated.\n";
                            }
                            // room count doesnt change
                            else
                            {
                            }
                            // send to client
                            TCPSendToClient(tcpNew_fd, reservationCode);
                            std::cout << "The main server sent the reservation result to the client.\n";
                        }
                    }
                }
            }

            close(tcpNew_fd);
            exit(0);
        }
    }

    close(tcpNew_fd);
    close(udpSockfd);

    return 0;
}