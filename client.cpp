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
#include <sys/socket.h>
#include <netinet/in.h>

#define TCPPORT "45966"  // the port users will be connecting to
#define BACKLOG 10       // how many pending connections queue will hold
#define MAXDATASIZE 1000 // max number of bytes we can get at once


// encrypt by incrementing letters and digits 3 times
std::string encrypt(const std::string &unencrypted)
{

    std::string encrypted;

    for (size_t i = 0; i < unencrypted.size(); ++i)
    {
        char c = unencrypted[i];
        int count = 3;

        // increment letter 3 times
        while (count > 0)
        {
            if (isalpha(c))
            {
                if (c == 'z' || c == 'Z')
                {
                    c -= 25; // wrap around cyclically
                }
                else
                {
                    ++c; // increment character
                }
            }
            else if (isdigit(c))
            {
                if (c == '9')
                {
                    c -= 9; // wrap around cyclically
                }
                else
                {
                    ++c; // increment character
                }
            }
            --count;
        }

        encrypted += c;
    }

    return encrypted;
}

// returns true if str is all lower case
bool allLower(const std::string &str)
{
    for (size_t i = 0; i < str.length(); ++i)
    {
        char c = str[i];
        if (!islower(c))
        {
            return false;
        }
    }
    return true;
}

int main()
{
    // Once the client boots up and the initial boot-
    // up messages are printed, the client waits for the user to check the authentication, login, and enter
    // the room layout code.

    // BEEJS
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    bool successLogin = 0;
    std::string unencrypted_username;
    std::string unencrypted_password;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo("localhost", TCPPORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("client: socket");
            continue;
        }

        // connect()
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    /*Retrieve the locally-bound name of the specified socket and store it in the sockaddr structure*/
    struct sockaddr_in my_addr;
    socklen_t addrlen = sizeof(my_addr);

    if (getsockname(sockfd, (struct sockaddr *)&my_addr, &addrlen) == -1)
    {
        perror("getsockname");
        exit(1);
    }

    // get dynamically allocated TCP port #
    int dynamicTCP = ntohs(my_addr.sin_port);

    if (p == NULL)
    {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    freeaddrinfo(servinfo); 

    printf("Client is up and running.\n");

    // login
    while (!successLogin)
    {
        printf("Please enter the username: ");
        std::getline(std::cin, unencrypted_username);

        // username error check
        while (true)
        {
            if (!allLower(unencrypted_username))
            {
                // std::cerr << "username must be all lowercase letters\n";
                printf("Please enter the username: ");
                std::getline(std::cin, unencrypted_username);
            }
            // if (!(unencrypted_username.length() >= 5 && unencrypted_username.length() <= 50))
            // {
            //     std::cerr << "username must be between 5 and 50 characters\n";
            //     printf("Please enter the username: ");
            //     std::getline(std::cin, unencrypted_username);
            // }

            if (allLower(unencrypted_username) /* && (unencrypted_username.length() >= 5 && unencrypted_username.length() <= 50)*/)
            {
                break;
            }
        }

        printf("Please enter the password (Press “Enter” to skip for guest): ");
        std::getline(std::cin, unencrypted_password);

        // if input is empty
        if (unencrypted_password.empty() || unencrypted_password.find_first_not_of(' ') == std::string::npos)
        {
            unencrypted_password = "Enter";
        }
        else
        {
            // password error check
            while (!(unencrypted_password.length() >= 5 && unencrypted_password.length() <= 50))
            {
                // std::cerr << "password must be between 5 and 50 characters\n";
                printf("Please enter the password: ");
                std::getline(std::cin, unencrypted_password);
            }
        }

        // encrypt username and password
        std::string encrypted_username = encrypt(unencrypted_username);
        std::string encrypted_password = encrypt(unencrypted_password);

        // send username and password to serverM
        std::string combined = encrypted_username + "," + encrypted_password;
        // send()
        if (send(sockfd, combined.c_str(), combined.length(), 0) == -1)
        {
            perror("send");
        }

        // RECIEVE RESPONSE FROM SERVER about login
        if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1)
        {
            perror("recv");
            exit(1);
        }

        buf[numbytes] = '\0';

        // OUTPUT if successful login or not
        std::string response(buf);
        int responseNum = std::stoi(response);

        switch (responseNum)
        {
        case (0):
            std::cout << "Failed login: Username does not exist." << std::endl;
            break;
        case (1): // VALID MEMBER
            std::cout << unencrypted_username << " sent an authentication request to the main server.\n";
            std::cout << "Welcome member " << unencrypted_username << "!" << std::endl;
            successLogin = 1;
            break;
        case (3): // MEMBER- password doesnt match
            std::cout << "Failed login: Password does not match." << std::endl;
            break;
        case (2):
        std::cout << unencrypted_username << " sent a guest request to the main server using TCP over port " << dynamicTCP << ".\n";
            std::cout << "Welcome guest " << unencrypted_username << "!" << std::endl;
            successLogin = 1;
            break;
        }
    }

// prompt for Availability or Reservation request
    while (true)
    {
        std::cout << "Please enter the room layout code: ";
        std::string roomCode;
        std::cin >> roomCode;

        std::string availabilityOrReservation;
        while (availabilityOrReservation != "Availability" && availabilityOrReservation != "Reservation")
        {
            std::cout << "Would you like to search for the availability or make a reservation? (Enter “Availability” to ";
            std::cout << "search for the availability or Enter “Reservation” to make a reservation ): ";
            std::cin >> availabilityOrReservation;
        }

        // send request to server M
        std::string request = roomCode + "," + availabilityOrReservation;
        // send()
        if (send(sockfd, request.c_str(), request.length(), 0) == -1)
        {
            perror("send");
        }

        // RECEIVE RESPONSE FROM SERVER about request
        if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1)
        {
            perror("recv");
            exit(1);
        }

        buf[numbytes] = '\0';

        // OUTPUT if successful login or not
        std::string requestResponse(buf);
        int requestResponseNum = std::stoi(requestResponse);
        // std::cout << "requestResponseNum: " << requestResponseNum << std::endl;

        // std::cout << "REQUEST RESPONSE: " << requestResponseNum <;

        if (availabilityOrReservation == "Availability")
        {
            std::cout << unencrypted_username << " sent an availability request to the main server.\n";

            switch (requestResponseNum)
            {
            // After receiving the response from Main Server (the count is greater than 0)
            case (1):
                std::cout << "The client received the response from the main server using TCP over port " << dynamicTCP << ".\n";
                std::cout << "The requested room is available.\n";
                std::cout << "\n";
                std::cout << "-----Start a new request-----\n";

                break;
            // After receiving the response from Main Server (the count is 0)
            case (2):
                std::cout << "The client received the response from the main server using TCP over port " << dynamicTCP << ".\n";
                std::cout << "The requested room is not available.\n";
                std::cout << "\n";
                std::cout << "-----Start a new request-----\n";
                break;
            // After receiving the response from Main Server (room code is not in the system)
            case (3):
                std::cout << "The client received the response from the main server using TCP over port " << dynamicTCP << ".\n";
                std::cout << "Not able to find the room layout.\n";
                std::cout << "\n";
                std::cout << "-----Start a new request-----\n";
                break;
            }
        }
        else if (availabilityOrReservation == "Reservation")
        {
            std::cout << unencrypted_username << " sent a reservation request to the main server.\n";

            switch (requestResponseNum)
            {
            // After receiving the response from Main Server (the count is greater than 0)
            case (1):
                std::cout << "The client received the response from the main server using TCP over port " << dynamicTCP << ".\n";
                std::cout << "Congratulation! The reservation for Room " << roomCode << " has been made.\n";
                std::cout << "\n";
                std::cout << "-----Start a new request-----\n";

                break;
            // After receiving the response from Main Server (the count is 0)
            case (2):
                std::cout << "The client received the response from the main server using TCP over port " << dynamicTCP << ".\n";
                std::cout << "Sorry! The requested room is not available.\n";
                std::cout << "\n";
                std::cout << "-----Start a new request-----\n";
                break;
            // After receiving the response from Main Server (room code is not in the system)
            case (3):
                std::cout << "The client received the response from the main server using TCP over port " << dynamicTCP << ".\n";
                std::cout << "Oops! Not able to find the room.\n";
                std::cout << "\n";
                std::cout << "-----Start a new request-----\n";
                break;
                // GUEST- After receiving the response from Main Server
            case (4):
                std::cout << "Permission denied: Guest cannot make a reservation.\n";
                break;
            }
        }
    }
    close(sockfd);
    return 0;
}