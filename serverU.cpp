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
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#define SERVERPORT "44966" // the port users will be connecting to
#define MYPORT "43966"     // need to bind its own port number
#define MAXBUFLEN 100

// read file and extract room and count from it
std::vector<std::pair<std::string, int> > readFile(const std::string &fname)
{
    std::vector<std::pair<std::string, int> > data;

    // open file
    std::ifstream file(fname);

    // if file opened successfully
    if (file.is_open())
    {
        std::string line;
        while (getline(file, line))
        {
            std::istringstream iss(line);
            std::string room;
            int count;
            // read room and then read integer
            if (getline(iss, room, ',') && iss >> count)
            {
                // add to vector
                data.push_back(std::make_pair(room, count));
            }
        }
    }

    file.close(); // close the file
    return data;  // return the vector of pairs
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// search data vector for availability, change to appropriate room availability code
void searchAvailability(const std::string &receivedRoom, std::string &availabilityCode, std::vector<std::pair<std::string, int> > &data)
{
    // search for room
    for (std::vector<std::pair<std::string, int> >::const_iterator it = data.begin(); it != data.end(); ++it)
    {
        if (it->first == receivedRoom)
        {
            // room is available
            if (it->second > 0)
            {
                availabilityCode = "1";
                std::cout << "Room " << receivedRoom << " is available.\n";
            }
            // room not available
            else
            {
                availabilityCode = "2";
                std::cout << "Room " << receivedRoom << " is not available.\n";
            }
            break;
        }
    }

    if (availabilityCode != "1" && availabilityCode != "2")
    {
        availabilityCode = "3";
        std::cout << "Not able to find the room layout.\n";
    }
}

// search data vector for reservation, make reservation (decrement count) if necessary and change to appropriate room reservation code
void searchReservation(const std::string &receivedRoom, std::string &reservationCode, std::vector<std::pair<std::string, int> > &data)
{
    // search for room
    for (std::vector<std::pair<std::string, int> >::iterator it = data.begin(); it != data.end(); ++it)
    {
        if (it->first == receivedRoom)
        {
            // room is available
            if (it->second > 0)
            {
                reservationCode = "1";
                --(it->second); // decrement room count to make reservation
                std::cout << "Successful reservation. The count of Room " << receivedRoom << " is now " << it->second << ".\n";
            }
            // room not available
            else
            {
                reservationCode = "2";
                std::cout << "Cannot make a reservation. Room " << receivedRoom << " is not available.\n";
            }
            break;
        }
    }

    if (reservationCode != "1" && reservationCode != "2")
    {
        reservationCode = "3";
        std::cout << "Cannot make a reservation. Not able to find the room layout.\n";
    }
}

// send availability / reservation code to serverM over UDP
void sendToMainServer(const std::string number)
{
    // FROM BEEJ
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(NULL, SERVERPORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }

    // loop through all the results and make a socket
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "talker: failed to create socket\n");
    }

    if ((numbytes = sendto(sockfd, number.c_str(), number.length(), 0,
                           p->ai_addr, p->ai_addrlen)) == -1)
    {
        perror("talker: sendto");
        exit(1);
    }

    freeaddrinfo(servinfo);
    close(sockfd);
};

int main()
{
    // read file
    std::vector<std::pair<std::string, int> > data = readFile("suite.txt");

    // format into string
    std::string formattedData;
    for (std::vector<std::pair<std::string, int> >::const_iterator it = data.begin(); it != data.end(); ++it)
    {
        formattedData += it->first + ", " + std::to_string(it->second) + "\n";
    }

    printf("The Server U is up and running using UDP on port %s.\n", MYPORT);

    sendToMainServer(formattedData);

    printf("The Server U has sent the room status to the main server.\n");

    // FROM BEEJ
    // LISTEN
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    // receive availability / reservation request indefinitely
    while (true)
    {
        // FROM BEEJ
        addr_len = sizeof their_addr;
        if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
        {
            perror("recvfrom");
            exit(1);
        }
        buf[numbytes] = '\0';

        // printf("listener: packet contains \"%s\"\n", buf);
        std::string receivedRoom(buf);

        char AorR = receivedRoom[0];
        receivedRoom = receivedRoom.substr(1);
        if (AorR == 'A')
        {
            std::cout << "The Server U received an availability request from the main server.\n";
            std::string availabilityCode;
            searchAvailability(receivedRoom, availabilityCode, data);
            sendToMainServer(availabilityCode);

            std::cout << "The Server U finished sending the response to the main server.\n";
        }
        else if (AorR == 'R')
        {
            std::cout << "The Server U received an reservation request from the main server.\n";
            std::string reservationCode;
            searchReservation(receivedRoom, reservationCode, data);
            sendToMainServer(reservationCode);

            // room count changes
            if (reservationCode == "1")
            {
                std::cout << "The Server U finished sending the response and the updated room status to the main server.\n";
            }
            // room count doesnt change
            else
            {
                std::cout << "The Server U finished sending the response to the main server.\n";
            }
        }
    }

    close(sockfd);
    return 0;
}
